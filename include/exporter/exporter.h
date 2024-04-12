/*
 * Copyright 2022 Max Planck Institute for Software Systems, and
 * National University of Singapore
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <memory>
#include <string>
#include <mutex>
#include <unordered_map>
#include <map>
#include <cassert>

#include "opentelemetry/sdk/version/version.h"
#include "opentelemetry/trace/provider.h"
#include "opentelemetry/exporters/otlp/otlp_http_exporter_factory.h"
#include "opentelemetry/exporters/otlp/otlp_http_exporter_options.h"
#include "opentelemetry/sdk/common/global_log_handler.h"
#include "opentelemetry/sdk/trace/simple_processor_factory.h"
#include "opentelemetry/sdk/trace/batch_span_processor_factory.h"
#include "opentelemetry/sdk/trace/tracer_provider_factory.h"
#include "opentelemetry/sdk/trace/tracer_provider.h"
#include "opentelemetry/exporters/otlp/otlp_grpc_exporter_factory.h"
#include "opentelemetry/exporters/ostream/span_exporter_factory.h"
#include "opentelemetry/trace/scope.h"
#include "opentelemetry/common/timestamp.h"
#include "opentelemetry/sdk/trace/processor.h"
#include "opentelemetry/sdk/trace/batch_span_processor_options.h"

#include "util/exception.h"
#include "events/events.h"
#include "analytics/span.h"
#include "analytics/trace.h"
#include "util/utils.h"
#include "util/ttlMap.h"

#ifndef SIMBRICKS_TRACE_EXPORTER_H_
#define SIMBRICKS_TRACE_EXPORTER_H_

namespace simbricks::trace {

class SpanExporter {

 protected:
  TraceEnvironment &trace_environment_;

 public:
  explicit SpanExporter(TraceEnvironment &trace_environment)
      : trace_environment_(trace_environment) {
  };

  virtual void StartSpan(std::shared_ptr<EventSpan> to_start) = 0;

  virtual void EndSpan(std::shared_ptr<EventSpan> to_end) = 0;

  virtual void ExportSpan(std::shared_ptr<EventSpan> to_export) = 0;

  virtual void ForceFlush() = 0;
};

// special span exporter that doies nothing, may be useful for debugging purposes
class NoOpExporter : public SpanExporter {
 public:
  explicit NoOpExporter(TraceEnvironment &trace_environment) : SpanExporter(trace_environment) {};

  void StartSpan(std::shared_ptr<EventSpan> to_start) override {
  }

  void EndSpan(std::shared_ptr<EventSpan> to_end) override {
  }

  void ExportSpan(std::shared_ptr<EventSpan> to_export) override {
    spdlog::warn("NoOpExporter 'exported' Span a.k.a did nothing");
  }

  void ForceFlush() override {}
};

class OtlpSpanExporter : public SpanExporter {

  const int64_t time_offset_nanosec_;
  const std::string url_;
  bool batch_mode_ = false;
  std::string lib_name_;

  using context_t = opentelemetry::trace::SpanContext;
  // mapping old_span_id -> opentelemetry_span_context
  //LazyTtLMap<uint64_t, context_t, 900> context_map_;
  std::unordered_map<uint64_t, context_t> context_map_;

  // mapping: own_span_id -> opentelemetry_span
  using span_t = opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span>;
  std::unordered_map<uint64_t, span_t> span_map_;

  // service/simulator/spanner name -> otlp_exporter
  using tracer_t = opentelemetry::nostd::shared_ptr<opentelemetry::trace::Tracer>;
  using provider_t = std::shared_ptr<opentelemetry::trace::TracerProvider>;
  std::unordered_map<std::string, tracer_t> tracer_map_;
  std::vector<provider_t> provider_;

  static constexpr int kPicoToNanoDenominator = 1000;
  using ts_steady = std::chrono::time_point<std::chrono::steady_clock, std::chrono::nanoseconds>;
  using ts_system = std::chrono::time_point<std::chrono::system_clock, std::chrono::nanoseconds>;

  void InsertNewContext(uint64_t span_id,
                        context_t &context) {
    throw_on_false(TraceEnvironment::IsValidId(span_id),
                   "invalid id", source_loc::current());
    //const bool suc = context_map_.Insert(span_id, context);
    const bool suc = context_map_.insert({span_id, context}).second;
    throw_on_false(suc, "InsertNewContext could not insert context into map",
                   source_loc::current());
  }

  opentelemetry::trace::SpanContext
  GetContext(uint64_t span_id) {
    throw_on_false(TraceEnvironment::IsValidId(span_id),
                   "GetContext context_to_get is null", source_loc::current());
    //auto con_opt = context_map_.Find(span_id);
    auto con_opt = context_map_.find(span_id);
    throw_on(con_opt == context_map_.end(), "could not find context for key",
             source_loc::current());
    //auto con = OrElseThrow(con_opt, "could not find context for key",
    //                       source_loc::current());
    //return con;
    return con_opt->second;
  }

  void InsertNewSpan(std::shared_ptr<EventSpan> &old_span,
                     span_t &new_span) {
    throw_if_empty(old_span, "InsertNewSpan old span is null", source_loc::current());
    throw_on(not new_span, "InsertNewSpan new_span is null", source_loc::current());

    const uint64_t span_id = old_span->GetId();
    auto iter = span_map_.insert({span_id, new_span});
    throw_on_false(iter.second, "InsertNewSpan could not insert into span map",
                   source_loc::current());
  }

  void RemoveSpan(const std::shared_ptr<EventSpan> &old_span) {
    const size_t erased = span_map_.erase(old_span->GetId());
    throw_on(erased != 1, "RemoveSpan did not remove a single span", source_loc::current());
  }

  tracer_t CreateTracer(std::string &service_name) {
    // NOTE: Lock must be held when calling this method!

    // create exporter
    opentelemetry::exporter::otlp::OtlpHttpExporterOptions opts;
    opts.url = url_;
    auto exporter = opentelemetry::exporter::otlp::OtlpHttpExporterFactory::Create(
        opts);
//    auto exporter = opentelemetry::exporter::trace::OStreamSpanExporterFactory::Create();
    throw_if_empty(exporter, TraceException::kSpanExporterNull, source_loc::current());

    // create span processor
    std::unique_ptr<opentelemetry::sdk::trace::SpanProcessor> processor = nullptr;
    if (batch_mode_) {
      const opentelemetry::sdk::trace::BatchSpanProcessorOptions batch_opts;
      processor = opentelemetry::sdk::trace::BatchSpanProcessorFactory::Create(
          std::move(exporter), batch_opts);
    } else {
      processor = opentelemetry::sdk::trace::SimpleSpanProcessorFactory::Create(std::move(exporter));
    }
    throw_if_empty(processor, TraceException::kSpanProcessorNull, source_loc::current());

    // create trace provider
    static uint64_t next_instance_id = 0;
    auto resource_attr = opentelemetry::sdk::resource::ResourceAttributes{
        {"service.name", service_name},
        {"service-instance", std::to_string(next_instance_id)}
    };
    ++next_instance_id;
    auto resource = opentelemetry::sdk::resource::Resource::Create(resource_attr);
    const provider_t
        provider = opentelemetry::sdk::trace::TracerProviderFactory::Create(std::move(processor), resource);
    throw_if_empty(provider, TraceException::kTraceProviderNull, source_loc::current());

    // Set the global trace provider
    provider_.push_back(provider);
    //opentelemetry::trace::Provider::SetTracerProvider(provider);

    // set tracer
    auto tracer = provider->GetTracer(lib_name_);//g, OPENTELEMETRY_SDK_VERSION);
    return tracer;
  }

  tracer_t GetTracerLazy(std::string &service_name) {
    auto iter = tracer_map_.find(service_name);
    if (iter != tracer_map_.end()) {
      return iter->second;
    }

    auto tracer = CreateTracer(service_name);
    tracer_map_.insert({service_name, tracer});
    return tracer;
  }

  span_t GetSpan(std::shared_ptr<EventSpan> &span_to_get) {
    throw_if_empty(span_to_get, "GetSpan span_to_get is null", source_loc::current());

    const uint64_t span_id = span_to_get->GetId();
    auto span = span_map_.find(span_id)->second;
    throw_on(not span, "InsertNewSpan span is null", source_loc::current());
    return span;
  }

  opentelemetry::common::SteadyTimestamp ToSteadyNanoseconds(uint64_t timestamp_pico) {
    const std::chrono::nanoseconds nano_sec(time_offset_nanosec_ + timestamp_pico / kPicoToNanoDenominator);
    const ts_steady time_point(nano_sec);
    return opentelemetry::common::SteadyTimestamp(time_point);
  }

  opentelemetry::common::SystemTimestamp ToSystemNanoseconds(uint64_t timestamp_pico) {
    const std::chrono::nanoseconds nano_sec(time_offset_nanosec_ + timestamp_pico / kPicoToNanoDenominator);
    const ts_system time_point(nano_sec);
    return opentelemetry::common::SystemTimestamp(time_point);
  }

  static void add_Event(std::map<std::string, std::string> &attributes, const std::shared_ptr<Event> &event) {
    assert(event and "event is not null");
    const std::string type = GetTypeStr(event);
    attributes.insert({"timestamp", std::to_string(event->GetTs())});
    attributes.insert({"parser_ident", std::to_string(event->GetParserIdent())});
    attributes.insert({"parser name", event->GetParserName()});
    attributes.insert({"type", type});
  }

  static void add_SimSendSync(std::map<std::string, std::string> &attributes,
                              const std::shared_ptr<SimSendSync> &event) {
    assert(event and "event is not null");
    add_Event(attributes, event);
  }

  static void add_SimProcInEvent(std::map<std::string, std::string> &attributes,
                                 const std::shared_ptr<SimProcInEvent> &event) {
    assert(event and "event is not null");
    add_Event(attributes, event);
  }

  static void add_HostInstr(std::map<std::string, std::string> &attributes, const std::shared_ptr<HostInstr> &event) {
    assert(event and "event is not null");
    add_Event(attributes, event);
    attributes.insert({"pc", std::to_string(event->GetPc())});
  }

  static void add_HostCall(std::map<std::string, std::string> &attributes, const std::shared_ptr<HostCall> &event) {
    assert(event and "event is not null");
    add_HostInstr(attributes, event);
    attributes.insert({"func", *(event->GetFunc())});
    attributes.insert({"comp", *(event->GetComp())});
  }

  static void add_HostMmioImRespPoW(std::map<std::string, std::string> &attributes,
                                    const std::shared_ptr<HostMmioImRespPoW> &event) {
    assert(event and "event is not null");
    add_Event(attributes, event);
  }

  static void add_HostIdOp(std::map<std::string, std::string> &attributes, const std::shared_ptr<HostIdOp> &event) {
    assert(event and "event is not null");
    add_Event(attributes, event);
    attributes.insert({"id", std::to_string(event->GetId())});
  }

  static void add_HostMmioCR(std::map<std::string, std::string> &attributes, const std::shared_ptr<HostMmioCR> &event) {
    assert(event and "event is not null");
    add_HostIdOp(attributes, event);
  }

  static void add_HostMmioCW(std::map<std::string, std::string> &attributes, const std::shared_ptr<HostMmioCW> &event) {
    assert(event and "event is not null");
    add_HostIdOp(attributes, event);
  }

  static void add_HostAddrSizeOp(std::map<std::string, std::string> &attributes,
                                 const std::shared_ptr<HostAddrSizeOp> &event) {
    assert(event and "event is not null");
    add_HostIdOp(attributes, event);
    attributes.insert({"addr", std::to_string(event->GetAddr())});
    attributes.insert({"size", std::to_string(event->GetSize())});
  }

  static void add_HostMmioOp(std::map<std::string, std::string> &attributes, const std::shared_ptr<HostMmioOp> &event) {
    assert(event and "event is not null");
    add_HostAddrSizeOp(attributes, event);
    attributes.insert({"bar", std::to_string(event->GetBar())});
    attributes.insert({"offset", std::to_string(event->GetOffset())});
  }

  static void add_HostMmioR(std::map<std::string, std::string> &attributes, const std::shared_ptr<HostMmioR> &event) {
    assert(event and "event is not null");
    add_HostMmioOp(attributes, event);
  }

  static void add_HostMmioW(std::map<std::string, std::string> &attributes, const std::shared_ptr<HostMmioW> &event) {
    assert(event and "event is not null");
    add_HostMmioOp(attributes, event);
  }

  static void add_HostDmaC(std::map<std::string, std::string> &attributes, const std::shared_ptr<HostDmaC> &event) {
    assert(event and "event is not null");
    add_HostIdOp(attributes, event);
  }

  static void add_HostDmaR(std::map<std::string, std::string> &attributes, const std::shared_ptr<HostDmaR> &event) {
    assert(event and "event is not null");
    add_HostAddrSizeOp(attributes, event);
  }

  static void add_HostDmaW(std::map<std::string, std::string> &attributes, const std::shared_ptr<HostDmaW> &event) {
    assert(event and "event is not null");
    add_HostAddrSizeOp(attributes, event);
  }

  static void add_HostMsiX(std::map<std::string, std::string> &attributes, const std::shared_ptr<HostMsiX> &event) {
    assert(event and "event is not null");
    add_Event(attributes, event);
    attributes.insert({"vec", std::to_string(event->GetVec())});
  }

  static void add_HostConf(std::map<std::string, std::string> &attributes, const std::shared_ptr<HostConf> &event) {
    assert(event and "event is not null");
    add_Event(attributes, event);
    attributes.insert({"dev", std::to_string(event->GetDev())});
    attributes.insert({"func", std::to_string(event->GetFunc())});
    attributes.insert({"reg", std::to_string(event->GetReg())});
    attributes.insert({"bytes", std::to_string(event->GetBytes())});
    attributes.insert({"data", std::to_string(event->GetBytes())});
    attributes.insert({"is_read", event->IsRead() ? "true" : "false"});
  }

  static void add_HostClearInt(std::map<std::string, std::string> &attributes,
                               const std::shared_ptr<HostClearInt> &event) {
    assert(event and "event is not null");
    add_Event(attributes, event);
  }

  static void add_HostPostInt(std::map<std::string, std::string> &attributes,
                              const std::shared_ptr<HostPostInt> &event) {
    assert(event and "event is not null");
    add_Event(attributes, event);
  }

  static void add_HostPciRW(std::map<std::string, std::string> &attributes, const std::shared_ptr<HostPciRW> &event) {
    assert(event and "event is not null");
    add_Event(attributes, event);
    attributes.insert({"offset", std::to_string(event->GetOffset())});
    attributes.insert({"size", std::to_string(event->GetSize())});
    attributes.insert({"is_read", event->IsRead() ? "true" : "false"});
  }

  static void add_NicMsix(std::map<std::string, std::string> &attributes, const std::shared_ptr<NicMsix> &event) {
    assert(event and "event is not null");
    add_Event(attributes, event);
    attributes.insert({"vec", std::to_string(event->GetVec())});
    attributes.insert({"isX", BoolToString(event->IsX())});
  }

  static void add_NicDma(std::map<std::string, std::string> &attributes, const std::shared_ptr<NicDma> &event) {
    assert(event and "event is not null");
    add_Event(attributes, event);
    attributes.insert({"id", std::to_string(event->GetId())});
    attributes.insert({"addr", std::to_string(event->GetId())});
    attributes.insert({"len", std::to_string(event->GetLen())});
  }

  static void add_SetIX(std::map<std::string, std::string> &attributes, const std::shared_ptr<SetIX> &event) {
    assert(event and "event is not null");
    add_Event(attributes, event);
    attributes.insert({"intr", std::to_string(event->GetIntr())});
  }

  static void add_NicDmaI(std::map<std::string, std::string> &attributes, const std::shared_ptr<NicDmaI> &event) {
    assert(event and "event is not null");
    add_NicDma(attributes, event);
  }

  static void add_NicDmaEx(std::map<std::string, std::string> &attributes, const std::shared_ptr<NicDmaEx> &event) {
    assert(event and "event is not null");
    add_NicDma(attributes, event);
  }

  static void add_NicDmaEn(std::map<std::string, std::string> &attributes, const std::shared_ptr<NicDmaEn> &event) {
    assert(event and "event is not null");
    add_NicDma(attributes, event);
  }

  static void add_NicDmaCR(std::map<std::string, std::string> &attributes, const std::shared_ptr<NicDmaCR> &event) {
    assert(event and "event is not null");
    add_NicDma(attributes, event);
  }

  static void add_NicDmaCW(std::map<std::string, std::string> &attributes, const std::shared_ptr<NicDmaCW> &event) {
    assert(event and "event is not null");
    add_NicDma(attributes, event);
  }

  static void add_NicMmio(std::map<std::string, std::string> &attributes, const std::shared_ptr<NicMmio> &event) {
    assert(event and "event is not null");
    add_Event(attributes, event);
    attributes.insert({"off", std::to_string(event->GetOff())});
    attributes.insert({"len", std::to_string(event->GetLen())});
    attributes.insert({"val", std::to_string(event->GetVal())});
  }

  static void add_NicMmioR(std::map<std::string, std::string> &attributes, const std::shared_ptr<NicMmioR> &event) {
    assert(event and "event is not null");
    add_NicMmio(attributes, event);
  }

  static void add_NicMmioW(std::map<std::string, std::string> &attributes, const std::shared_ptr<NicMmioW> &event) {
    assert(event and "event is not null");
    add_NicMmio(attributes, event);
    attributes.insert({"posted", BoolToString(event->IsPosted())});
  }

  static void add_NicTrx(std::map<std::string, std::string> &attributes, const std::shared_ptr<NicTrx> &event) {
    assert(event and "event is not null");
    add_Event(attributes, event);
    attributes.insert({"len", std::to_string(event->GetLen())});
  }

  static void add_NicTx(std::map<std::string, std::string> &attributes, const std::shared_ptr<NicTx> &event) {
    assert(event and "event is not null");
    add_NicTrx(attributes, event);
  }

  static void add_NicRx(std::map<std::string, std::string> &attributes, const std::shared_ptr<NicRx> &event) {
    assert(event and "event is not null");
    add_NicTrx(attributes, event);
    attributes.insert({"port", std::to_string(event->GetPort())});
  }

  static void add_NetworkEvent(std::map<std::string, std::string> &attributes,
                               const std::shared_ptr<NetworkEvent> &event) {
    assert(event and "event is not null");
    add_Event(attributes, event);
    attributes.insert({"node", std::to_string(event->GetNode())});
    attributes.insert({"device", std::to_string(event->GetDevice())});
    attributes.insert({"device-type", sim_string_utils::ValueToString(event->GetDeviceType())});
    attributes.insert({"payload-size", std::to_string(event->GetPayloadSize())});
    attributes.insert({"boundary-type", sim_string_utils::ValueToString(event->GetBoundaryType())});
    attributes.insert({"device-tpe", sim_string_utils::ValueToString(event->GetDeviceType())});
    if (event->HasEthernetHeader()) {
      const NetworkEvent::EthernetHeader &header = event->GetEthernetHeader();
      attributes.insert({"src-mac", sim_string_utils::ValueToString(header.src_mac_)});
      attributes.insert({"dst-mac", sim_string_utils::ValueToString(header.dst_mac_)});
      std::stringstream len_ty;
      len_ty << "0x" << std::hex << header.length_type_;
      attributes.insert({"length-type", len_ty.str()});
    }
    if (event->HasArpHeader()) {
      const NetworkEvent::ArpHeader &header = event->GetArpHeader();
      attributes.insert({"request", BoolToString(header.is_request_)});
      attributes.insert({"src-ip", sim_string_utils::ValueToString(header.src_ip_)});
      attributes.insert({"dst-ip", sim_string_utils::ValueToString(header.dst_ip_)});
    }
    if (event->HasIpHeader()) {
      const NetworkEvent::Ipv4Header &header = event->GetIpHeader();
      attributes.insert({"length", std::to_string(header.length_)});
      attributes.insert({"src-ip", sim_string_utils::ValueToString(header.src_ip_)});
      attributes.insert({"dst-ip", sim_string_utils::ValueToString(header.dst_ip_)});
    }
  }

  opentelemetry::trace::StartSpanOptions GetSpanStartOpts(const std::shared_ptr<EventSpan> &span) {
    opentelemetry::trace::StartSpanOptions span_options;
    if (span->HasParent()) {
      const uint64_t parent_id = span->GetValidParentId();
      // Note: lock bust be free
      auto open_context = GetContext(parent_id);
      span_options.parent = open_context;
    }
    // Note: lock bust be free
    span_options.start_system_time = ToSystemNanoseconds(span->GetStartingTs());
    // Note: lock bust be free
    span_options.start_steady_time = ToSteadyNanoseconds(span->GetStartingTs());

    // 'hack' for jaeger ui...
    span_options.kind = opentelemetry::trace::SpanKind::kServer;

    return std::move(span_options);
  }

  void end_span(const std::shared_ptr<EventSpan> &old_span, span_t &new_span) {
    assert(old_span and "old span is null");
    assert(new_span and "new span is null");
    opentelemetry::trace::EndSpanOptions end_opts;
    // Note: lock bust be free
    end_opts.end_steady_time = ToSteadyNanoseconds(old_span->GetCompletionTs());
    new_span->End(end_opts);

    // Note: lock bust be free
    RemoveSpan(old_span);
  }

  static void set_EventSpanAttr(span_t &new_span, std::shared_ptr<EventSpan> old_span) {
    auto span_name = GetTypeStr(old_span);
    new_span->SetAttribute("id", std::to_string(old_span->GetId()));
    new_span->SetAttribute("source id", std::to_string(old_span->GetSourceId()));
    new_span->SetAttribute("type", span_name);
    new_span->SetAttribute("pending", BoolToString(old_span->IsPending()));
    auto context = old_span->GetContext();
    throw_if_empty(context, "add_EventSpanAttr context is null", source_loc::current());
    new_span->SetAttribute("trace id", std::to_string(context->GetTraceId()));
    if (context->HasParent()) {
      const uint64_t parent_id = context->GetParentId();
      new_span->SetAttribute("parent_id", parent_id);
    }
    new_span->SetAttribute("start-ts", std::to_string(old_span->GetStartingTs()));
    new_span->SetAttribute("end-ts", std::to_string(old_span->GetCompletionTs()));
  }

  static void set_HostCallSpanAttr(span_t &new_span, std::shared_ptr<HostCallSpan> &old_span) {
    set_EventSpanAttr(new_span, old_span);
    new_span->SetAttribute("kernel-transmit", BoolToString(old_span->DoesKernelTransmit()));
    new_span->SetAttribute("driver-transmit", BoolToString(old_span->DoesDriverTransmit()));
    new_span->SetAttribute("kernel-receive", BoolToString(old_span->DoesKernelReceive()));
    new_span->SetAttribute("driver-receive", BoolToString(old_span->DoesDriverReceive()));
    new_span->SetAttribute("overall-transmit", BoolToString(old_span->IsOverallTx()));
    new_span->SetAttribute("overall-receive", BoolToString(old_span->IsOverallRx()));
    new_span->SetAttribute("fragmented", BoolToString(old_span->IsFragmented()));
    const bool is_copy = old_span->IsCopy();
    new_span->SetAttribute("is-copy", BoolToString(is_copy));
    if (is_copy) {
      new_span->SetAttribute("original-id", std::to_string(old_span->GetOriginalId()));
    }
  }

  static void set_HostDmaSpanAttr(span_t &new_span, std::shared_ptr<HostDmaSpan> &old_span) {
    set_EventSpanAttr(new_span, old_span);
    new_span->SetAttribute("is-read", BoolToString(old_span->IsRead()));
  }

  void set_HostMmioSpanAttr(span_t &new_span, std::shared_ptr<HostMmioSpan> &old_span) {
    set_EventSpanAttr(new_span, old_span);
    new_span->SetAttribute("is-read", BoolToString(old_span->IsRead()));
    new_span->SetAttribute("BAR-number", std::to_string(old_span->GetBarNumber()));
    new_span->SetAttribute("is-going-to-device",
                           BoolToString(this->trace_environment_.IsToDeviceBarNumber(
                               old_span->GetBarNumber())));
  }

  static void set_HostPciSpanAttr(span_t &new_span, std::shared_ptr<HostPciSpan> &old_span) {
    set_EventSpanAttr(new_span, old_span);
    new_span->SetAttribute("is-read", BoolToString(old_span->IsRead()));
  }

  static void set_NicMmioSpanAttr(span_t &new_span, std::shared_ptr<NicMmioSpan> &old_span) {
    set_EventSpanAttr(new_span, old_span);
    new_span->SetAttribute("is-read", BoolToString(old_span->IsRead()));
  }

  static void set_NicDmaSpanAttr(span_t &new_span, std::shared_ptr<NicDmaSpan> &old_span) {
    set_EventSpanAttr(new_span, old_span);
    new_span->SetAttribute("is-read", BoolToString(old_span->IsRead()));
  }

  static void set_NicEthSpanAttr(span_t &new_span, std::shared_ptr<NicEthSpan> &old_span) {
    set_EventSpanAttr(new_span, old_span);
    new_span->SetAttribute("is-transmit", BoolToString(old_span->IsTransmit()));
  }

  static void set_NetDeviceSpanAttr(span_t &new_span, std::shared_ptr<NetDeviceSpan> &old_span) {
    set_EventSpanAttr(new_span, old_span);
    new_span->SetAttribute("is-arp", BoolToString(old_span->IsArp()));
    new_span->SetAttribute("is-drop", BoolToString(old_span->IsDrop()));
    if (old_span->HasIpsSet()) {
      new_span->SetAttribute("src-ip", sim_string_utils::ValueToString(old_span->GetSrcIp()));
      new_span->SetAttribute("dst-ip", sim_string_utils::ValueToString(old_span->GetDstIp()));
    }
    std::stringstream boundary_types;
    std::ranges::for_each(old_span->GetBoundaryTypes(), [&boundary_types](NetworkEvent::EventBoundaryType boundT) {
      boundary_types << boundT << ",";
    });
    new_span->SetAttribute("boundary-types", boundary_types.str());
    new_span->SetAttribute("is-interesting", BoolToString(old_span->InterestingFlag()));
    new_span->SetAttribute("node", std::to_string(old_span->GetNode()));
    new_span->SetAttribute("device", std::to_string(old_span->GetDevice()));
  }

  void set_Attr(span_t &span, std::shared_ptr<EventSpan> &to_end) {
    switch (to_end->GetType()) {
      case kHostCall: {
        auto call_span = std::static_pointer_cast<HostCallSpan>(to_end);
        set_HostCallSpanAttr(span, call_span);
        break;
      }
      case kHostMmio: {
        auto mmio_span = std::static_pointer_cast<HostMmioSpan>(to_end);
        set_HostMmioSpanAttr(span, mmio_span);
        break;
      }
      case kHostPci: {
        auto pci_span = std::static_pointer_cast<HostPciSpan>(to_end);
        set_HostPciSpanAttr(span, pci_span);
        break;
      }
      case kHostDma: {
        auto dma_span = std::static_pointer_cast<HostDmaSpan>(to_end);
        set_HostDmaSpanAttr(span, dma_span);
        break;
      }
      case kNicDma: {
        auto dma_span = std::static_pointer_cast<NicDmaSpan>(to_end);
        set_NicDmaSpanAttr(span, dma_span);
        break;
      }
      case kNicMmio: {
        auto mmio_span = std::static_pointer_cast<NicMmioSpan>(to_end);
        set_NicMmioSpanAttr(span, mmio_span);
        break;
      }
      case kNicEth: {
        auto eth_span = std::static_pointer_cast<NicEthSpan>(to_end);
        set_NicEthSpanAttr(span, eth_span);
        break;
      }
      case kNetDeviceSpan: {
        auto net_span = std::static_pointer_cast<NetDeviceSpan>(to_end);
        set_NetDeviceSpanAttr(span, net_span);
        break;
      }
      case kNicMsix:
      case kGenericSingle:
      case kHostInt:
      case kHostMsix:
      default: {
        set_EventSpanAttr(span, to_end);
      }
    }
  }

  void add_Events(span_t &span, std::shared_ptr<EventSpan> &to_end) {
    const size_t amount_events = to_end->GetAmountEvents();
    for (size_t index = 0; index < amount_events; index++) {

      auto action = to_end->GetAt(index);
      throw_if_empty(action, "EndSpan action is null", source_loc::current());

      auto type = GetTypeStr(action);
      std::map<std::string, std::string> attributes;

      switch (action->GetType()) {
        case EventType::kHostCallT: {
          add_HostCall(attributes, std::static_pointer_cast<HostCall>(action));
          break;
        }
        case EventType::kHostMsiXT: {
          add_HostMsiX(attributes, std::static_pointer_cast<HostMsiX>(action));
          break;
        }
        case EventType::kHostMmioWT: {
          add_HostMmioW(attributes, std::static_pointer_cast<HostMmioW>(action));
          break;
        }
        case EventType::kHostMmioRT: {
          add_HostMmioR(attributes, std::static_pointer_cast<HostMmioR>(action));
          break;
        }
        case EventType::kHostMmioImRespPoWT: {
          add_HostMmioImRespPoW(attributes,
                                std::static_pointer_cast<HostMmioImRespPoW>(action));
          break;
        }
        case EventType::kHostMmioCWT: {
          add_HostMmioCW(attributes, std::static_pointer_cast<HostMmioCW>(action));
          break;
        }
        case EventType::kHostMmioCRT: {
          add_HostMmioCR(attributes, std::static_pointer_cast<HostMmioCR>(action));
          break;
        }
        case EventType::kHostPciRWT: {
          add_HostPciRW(attributes, std::static_pointer_cast<HostPciRW>(action));
          break;
        }
        case EventType::kHostConfT: {
          add_HostConf(attributes, std::static_pointer_cast<HostConf>(action));
          break;
        }
        case EventType::kHostDmaWT: {
          add_HostDmaW(attributes, std::static_pointer_cast<HostDmaW>(action));
          break;
        }
        case EventType::kHostDmaRT: {
          add_HostDmaR(attributes, std::static_pointer_cast<HostDmaR>(action));
          break;
        }
        case EventType::kHostDmaCT: {
          add_HostDmaC(attributes, std::static_pointer_cast<HostDmaC>(action));
          break;
        }
        case EventType::kHostPostIntT: {
          add_HostPostInt(attributes, std::static_pointer_cast<HostPostInt>(action));
          break;
        }
        case EventType::kHostClearIntT: {
          add_HostClearInt(attributes, std::static_pointer_cast<HostClearInt>(action));
          break;
        }
        case EventType::kNicDmaIT: {
          add_NicDmaI(attributes, std::static_pointer_cast<NicDmaI>(action));
          break;
        }
        case EventType::kNicDmaExT: {
          add_NicDmaEx(attributes, std::static_pointer_cast<NicDmaEx>(action));
          break;
        }
        case EventType::kNicDmaCWT: {
          add_NicDmaCW(attributes, std::static_pointer_cast<NicDmaCW>(action));
          break;
        }
        case EventType::kNicDmaCRT: {
          add_NicDmaCR(attributes, std::static_pointer_cast<NicDmaCR>(action));
          break;
        }
        case EventType::kNicMmioRT: {
          add_NicMmioR(attributes, std::static_pointer_cast<NicMmioR>(action));
          break;
        }
        case EventType::kNicMmioWT: {
          add_NicMmioW(attributes, std::static_pointer_cast<NicMmioW>(action));
          break;
        }
        case EventType::kNicRxT: {
          add_NicRx(attributes, std::static_pointer_cast<NicRx>(action));
          break;
        }
        case EventType::kNicTxT: {
          add_NicTx(attributes, std::static_pointer_cast<NicTx>(action));
          break;
        }
        case EventType::kNicMsixT: {
          add_NicMsix(attributes, std::static_pointer_cast<NicMsix>(action));
          break;
        }
        case EventType::kNetworkEnqueueT:
        case EventType::kNetworkDequeueT:
        case EventType::kNetworkDropT: {
          add_NetworkEvent(attributes, std::static_pointer_cast<NetworkEvent>(action));
          break;
        }
        default: {
          throw_just(source_loc::current(),
                     "transform_HostCallSpan unexpected event: ", *action);
        }
      }

      // Note: lock bust be free
      const auto ts = ToSystemNanoseconds(action->GetTs());
      span->AddEvent(type, ts, attributes);
    }
  }

 public:
  explicit OtlpSpanExporter(TraceEnvironment &trace_environment,
                            const std::string &&url,
                            bool batch_mode,
                            std::string &&lib_name)
      : SpanExporter(trace_environment),
        time_offset_nanosec_(GetNowOffsetNanoseconds()),
        url_(url),
        batch_mode_(batch_mode),
        lib_name_(lib_name) {
  }

  explicit OtlpSpanExporter(TraceEnvironment &trace_environment,
                            const std::string &url,
                            bool batch_mode,
                            std::string &&lib_name)
      : SpanExporter(trace_environment),
        time_offset_nanosec_(GetNowOffsetNanoseconds()),
        url_(url),
        batch_mode_(batch_mode),
        lib_name_(lib_name) {
  }

  ~OtlpSpanExporter() {
    const std::shared_ptr<opentelemetry::trace::TracerProvider> none;
    opentelemetry::trace::Provider::SetTracerProvider(none);
  }

  void ForceFlush() override {
    for (auto &str_tracer : tracer_map_) {
      str_tracer.second->ForceFlush(std::chrono::seconds(60));
    }
    auto provider = opentelemetry::trace::Provider::GetTracerProvider();
    if (provider) {
      static_cast<opentelemetry::sdk::trace::TracerProvider *>(provider.get())->ForceFlush();
    }
  }

  void StartSpan(std::shared_ptr<EventSpan> to_start) override {
    auto span_opts = GetSpanStartOpts(to_start);
    auto span_name = GetTypeStr(to_start);

    // Note: lock bust be free
    auto tracer = GetTracerLazy(to_start->GetServiceName());
    auto span = tracer->StartSpan(span_name, span_opts);
    span->SetStatus(opentelemetry::trace::StatusCode::kOk);

    // Note: lock bust be free
    InsertNewSpan(to_start, span);

    const uint64_t span_id = to_start->GetId();
    throw_on_false(TraceEnvironment::IsValidId(span_id),
                   "invalid id", source_loc::current());
    auto new_context = span->GetContext();

    // Note: lock bust be free
    InsertNewContext(span_id, new_context);
    spdlog::debug("started span");
  }

  void EndSpan(std::shared_ptr<EventSpan> to_end) override {
    // Note: lock bust be free
    span_t span = GetSpan(to_end);
    set_Attr(span, to_end);
    // Note: lock bust be free
    add_Events(span, to_end);
    end_span(to_end, span);
    spdlog::debug("ended span");
  }

  void ExportSpan(std::shared_ptr<EventSpan> to_export) override {
    spdlog::debug("Start exporting Span");
    {
      StartSpan(to_export);
      EndSpan(to_export);
    }
    spdlog::debug("Exported Span");
  }
};

}  // namespace simbricks::trace

#endif //SIMBRICKS_TRACE_EXPORTER_H_
