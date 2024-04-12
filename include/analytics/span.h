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

#ifndef SIMBRICKS_TRACE_EVENT_SPAN_H_
#define SIMBRICKS_TRACE_EVENT_SPAN_H_

#include <iostream>
#include <memory>
#include <utility>
#include <vector>
#include <optional>
#include <mutex>

#include "util/exception.h"
#include "sync/corobelt.h"
#include "events/events.h"
#include "env/traceEnvironment.h"
#include "analytics/traceContext.h"
#include "util/utils.h"

enum span_type {
  kHostCall,
  kHostMsix,
  kHostMmio,
  kHostDma,
  kHostInt,
  kHostPci,
  kNicDma,
  kNicMmio,
  kNicEth,
  kNicMsix,
  kNetDeviceSpan,
  kGenericSingle
};

inline std::ostream &operator<<(std::ostream &out, span_type type) {
  switch (type) {
    case span_type::kHostCall:out << "kHostCall";
      break;
    case span_type::kHostMsix:out << "kHostMsix";
      break;
    case span_type::kHostMmio:out << "kHostMmio";
      break;
    case span_type::kHostDma:out << "kHostDma";
      break;
    case span_type::kHostInt:out << "kHostInt";
      break;
    case span_type::kHostPci:out << "kHostPci";
      break;
    case span_type::kNicDma:out << "kNicDma";
      break;
    case span_type::kNicMmio:out << "kNicMmio";
      break;
    case span_type::kNicEth:out << "kNicEth";
      break;
    case span_type::kNicMsix:out << "kNicMsix";
      break;
    case span_type::kGenericSingle:out << "kGenericSingle";
      break;
    case span_type::kNetDeviceSpan:out << "kNetDeviceSpan";
      break;
    default:out << "could not represent given span type";
      break;
  }
  return out;
}

class EventSpan {
 protected:
  TraceEnvironment &trace_environment_;
  uint64_t id_;
  uint64_t source_id_;
  span_type type_;
  std::vector<std::shared_ptr<Event>> events_;
  bool is_pending_ = true;
  bool is_relevant_ = false;
  std::shared_ptr<EventSpan> original_ = nullptr;
  std::shared_ptr<TraceContext> trace_context_ = nullptr;
  std::string &service_name_;

//  std::recursive_mutex span_mutex_;

  inline static const char *tc_null_ = "try setting std::shared_ptr<TraceContext> which is null";

 public:
  virtual void Display(std::ostream &out);

  [[nodiscard]] inline std::string &GetServiceName() const {
    return service_name_;
  }

  inline void SetOriginal(const std::shared_ptr<EventSpan> &original) {
    throw_if_empty(original, "EventSpan::SetOriginal: original is empty", source_loc::current());
    original_ = original;
  }

  [[nodiscard]] inline bool IsCopy() const {
    return original_ != nullptr;
  }

  [[nodiscard]] inline uint64_t GetOriginalId() const {
    throw_if_empty(original_, "EventSpan::GetOriginalId: span is not a copy",
                   source_loc::current());
    return original_->GetId();
  }

  [[nodiscard]] inline size_t GetAmountEvents() const {
    return events_.size();
  }

  [[nodiscard]] inline bool HasEvents() const {
    return GetAmountEvents() > 0;
  }

  [[nodiscard]] std::shared_ptr<Event> GetAt(size_t index) const {
    if (index >= events_.size()) {
      return nullptr;
    }
    return events_[index];
  }

  [[nodiscard]] inline uint64_t GetId() const {
    return id_;
  }

  [[nodiscard]] inline uint64_t GetValidId() const {
    const uint64_t ident = GetId();
    throw_on_false(TraceEnvironment::IsValidId(ident), "invalid span id",
                   source_loc::current());
    return ident;
  }

  [[nodiscard]] inline span_type GetType() const {
    return type_;
  }

  [[nodiscard]] inline uint64_t GetSourceId() const {
    return source_id_;
  }

  [[nodiscard]] inline std::shared_ptr<TraceContext> GetContext() const {
    return trace_context_;
  }

  virtual void MarkAsDone() {
//    const std::lock_guard<std::recursive_mutex> guard(span_mutex_);
    is_pending_ = false;
  }

  [[nodiscard]] inline bool IsPending() const {
    return is_pending_;
  }

  [[nodiscard]] inline bool IsComplete() const {
    return not IsPending();
  }

  inline void MarkAsRelevant() {
    is_relevant_ = true;
  }

  inline void MarkAsNonRelevant() {
    is_relevant_ = false;
  }

  uint64_t GetStartingTs();

  uint64_t GetCompletionTs();

  bool SetContext(const std::shared_ptr<TraceContext> &traceContext, bool override_existing);

  bool HasParent() {
    return trace_context_ != nullptr and trace_context_->HasParent();
  }

  uint64_t GetParentId();

  uint64_t GetValidParentId() {
    const uint64_t parent_id = GetParentId();
    throw_on_false(TraceEnvironment::IsValidId(parent_id),
                   "invalid parent id", source_loc::current());
    return parent_id;
  }

  uint64_t GetTraceId() {
    if (trace_context_ == nullptr) {
      return 0; // invalid id
    }
    return trace_context_->GetTraceId();
  }

  uint64_t GetValidTraceId() {
    const uint64_t trace_id = GetTraceId();
    throw_on_false(TraceEnvironment::IsValidId(trace_id),
                   TraceException::kInvalidId, source_loc::current());
    return trace_id;
  }

  virtual ~EventSpan() = default;

  explicit EventSpan(TraceEnvironment &trace_environment,
                     std::shared_ptr<TraceContext> trace_context,
                     uint64_t source_id,
                     span_type type,
                     std::string &service_name)
      : trace_environment_(trace_environment),
        id_(trace_environment_.GetNextSpanId()),
        source_id_(source_id),
        type_(type),
        trace_context_(trace_context),
        service_name_(service_name) {
    throw_if_empty(trace_context_, tc_null_, source_loc::current());
  }

  EventSpan(const EventSpan &other)
      : trace_environment_(other.trace_environment_),
        id_(trace_environment_.GetNextSpanId()),
        source_id_(other.source_id_),
        type_(other.type_),
        events_(other.events_),
        is_pending_(other.is_pending_),
        is_relevant_(other.is_relevant_),
        original_(other.original_),
        trace_context_(other.trace_context_),
        service_name_(other.service_name_) {
    // NOTE: we make only a shallow copy
  }

  virtual EventSpan *clone() = 0;

  virtual bool AddToSpan(const std::shared_ptr<Event> &event_ptr) = 0;

 protected:
  // When calling this method the lock must be held
  bool IsPotentialAdd(const std::shared_ptr<Event> &event_ptr);
};

class HostCallSpan final : public EventSpan {
  std::shared_ptr<Event> call_span_entry_ = nullptr;
  std::shared_ptr<Event> syscall_return_ = nullptr;
  bool kernel_transmit_ = false;
  bool driver_transmit_ = false;
  bool kernel_receive_ = false;
  bool driver_receive_ = false;
  bool is_fragmented_ = true;

 public:
  explicit HostCallSpan(TraceEnvironment &trace_environment,
                        std::shared_ptr<TraceContext> &trace_context,
                        uint64_t source_id,
                        std::string &service_name,
                        bool fragmented)
      : EventSpan(trace_environment, trace_context, source_id, span_type::kHostCall, service_name),
        is_fragmented_(fragmented) {
  }

  // NOTE: we make only a shallow copy
  HostCallSpan(const HostCallSpan &other) = default;

  EventSpan *clone() override {
    return new HostCallSpan(*this);
  }

  ~HostCallSpan() override = default;

  [[nodiscard]] bool DoesKernelTransmit() const {
    return kernel_transmit_;
  }

  [[nodiscard]] bool DoesDriverTransmit() const {
    return driver_transmit_;
  }

  [[nodiscard]] bool DoesKernelReceive() const {
    return kernel_receive_;
  }

  [[nodiscard]] bool DoesDriverReceive() const {
    return driver_receive_;
  }

  [[nodiscard]] bool IsOverallTx() const {
    return kernel_transmit_ and driver_transmit_;
  }

  [[nodiscard]] bool IsOverallRx() const {
    return kernel_receive_ and driver_receive_;
  }

  [[nodiscard]] bool IsFragmented() const {
    return is_fragmented_;
  }

  void MarkAsDone() override {
    is_fragmented_ = is_fragmented_ or call_span_entry_ == nullptr or syscall_return_ == nullptr;
    is_pending_ = false;
  }

  bool AddToSpan(const std::shared_ptr<Event> &event_ptr) override;
};

class HostIntSpan final : public EventSpan {
  std::shared_ptr<Event> host_post_int_ = nullptr;
  std::shared_ptr<Event> host_clear_int_ = nullptr;

 public:
  explicit HostIntSpan(TraceEnvironment &trace_environment,
                       std::shared_ptr<TraceContext> &trace_context,
                       uint64_t source_id,
                       std::string &service_name)
      : EventSpan(trace_environment, trace_context, source_id, span_type::kHostInt, service_name) {
  }

  // NOTE: we make only a shallow copy
  HostIntSpan(const HostIntSpan &other) = default;

  EventSpan *clone() override {
    return new HostIntSpan(*this);
  }

  ~HostIntSpan() override = default;

  bool AddToSpan(const std::shared_ptr<Event> &event_ptr) override;
};

class HostDmaSpan final : public EventSpan {
  // kHostDmaWT or kHostDmaRT
  std::shared_ptr<Event> host_dma_execution_ = nullptr;
  bool is_read_ = true;
  // kHostDmaCT
  std::shared_ptr<Event> host_dma_completion_ = nullptr;

 public:
  explicit HostDmaSpan(TraceEnvironment &trace_environment,
                       std::shared_ptr<TraceContext> &trace_context,
                       uint64_t source_id,
                       std::string &service_name)
      : EventSpan(trace_environment, trace_context, source_id, span_type::kHostDma, service_name) {
  }

  // NOTE: we make only a shallow copy
  HostDmaSpan(const HostDmaSpan &other) = default;

  EventSpan *clone() override {
    return new HostDmaSpan(*this);
  }

  ~HostDmaSpan() override = default;

  [[nodiscard]] bool IsRead() const {
    return is_read_;
  }

  bool AddToSpan(const std::shared_ptr<Event> &event_ptr) override;
};

class HostMmioSpan final : public EventSpan {
  // issue, either host_mmio_w_ or host_mmio_r_
  std::shared_ptr<Event> host_mmio_issue_ = nullptr;
  bool is_read_ = false;
  bool is_posted_ = false;
  int bar_number_ = 0;
  std::shared_ptr<Event> im_mmio_resp_ = nullptr;
  // completion, either host_mmio_cw_ or host_mmio_cr_
  std::shared_ptr<Event> completion_ = nullptr;

 public:
  explicit HostMmioSpan(TraceEnvironment &trace_environment,
                        std::shared_ptr<TraceContext> &trace_context,
                        uint64_t source_id,
                        std::string &service_name,
                        int bar_number)
      : EventSpan(trace_environment, trace_context, source_id, span_type::kHostMmio, service_name),
        bar_number_(bar_number) {
  }

  // NOTE: we make only a shallow copy
  HostMmioSpan(const HostMmioSpan &other) = default;

  EventSpan *clone() override {
    return new HostMmioSpan(*this);
  }

  ~HostMmioSpan() override = default;

  [[nodiscard]] inline bool IsRead() const {
    return is_read_;
  }

  [[nodiscard]] inline int GetBarNumber() const {
    return bar_number_;
  }

  [[nodiscard]] inline bool IsPosted() const {
    return is_posted_;
  }

  bool AddToSpan(const std::shared_ptr<Event> &event_ptr) override;
};

class HostMsixSpan final : public EventSpan {
  std::shared_ptr<Event> host_msix_ = nullptr;
  std::shared_ptr<Event> host_dma_c_ = nullptr;

 public:
  explicit HostMsixSpan(TraceEnvironment &trace_environment,
                        std::shared_ptr<TraceContext> &trace_context,
                        uint64_t source_id,
                        std::string &service_name)
      : EventSpan(trace_environment, trace_context, source_id, span_type::kHostMsix, service_name) {
  }

  // NOTE: we make only a shallow copy
  HostMsixSpan(const HostMsixSpan &other) = default;

  EventSpan *clone() override {
    return new HostMsixSpan(*this);
  }

  ~HostMsixSpan() override = default;

  bool AddToSpan(const std::shared_ptr<Event> &event_ptr) override;
};

class HostPciSpan final : public EventSpan {
  std::shared_ptr<Event> host_pci_rw_ = nullptr;
  std::shared_ptr<Event> host_conf_rw_ = nullptr;
  bool is_read_ = false;

 public:
  explicit HostPciSpan(TraceEnvironment &trace_environment,
                       std::shared_ptr<TraceContext> &trace_context,
                       uint64_t source_id,
                       std::string &service_name)
      : EventSpan(trace_environment, trace_context, source_id, span_type::kHostPci, service_name) {
  }

  // NOTE: we make only a shallow copy
  HostPciSpan(const HostPciSpan &other) = default;

  EventSpan *clone() override {
    return new HostPciSpan(*this);
  }

  ~HostPciSpan() override = default;

  [[nodiscard]] inline bool IsRead() const {
    return is_read_;
  }

  [[nodiscard]] inline bool IsWrite() const {
    return not IsRead();
  }

  bool AddToSpan(const std::shared_ptr<Event> &event_ptr) override;
};

class NicMsixSpan final : public EventSpan {
  std::shared_ptr<Event> nic_msix_ = nullptr;

 public:
  NicMsixSpan(TraceEnvironment &trace_environment,
              std::shared_ptr<TraceContext> &trace_context,
              uint64_t source_id,
              std::string &service_name)
      : EventSpan(trace_environment, trace_context, source_id, span_type::kNicMsix, service_name) {
  }

  // NOTE: we make only a shallow copy
  NicMsixSpan(const NicMsixSpan &other) = default;

  EventSpan *clone() override {
    return new NicMsixSpan(*this);
  }

  ~NicMsixSpan() override = default;

  bool AddToSpan(const std::shared_ptr<Event> &event_ptr) override;
};

class NicMmioSpan final : public EventSpan {
  // nic action nic_mmio_w_ or nic_mmio_r_
  std::shared_ptr<Event> action_ = nullptr;
  bool is_read_ = false;

 public:
  explicit NicMmioSpan(TraceEnvironment &trace_environment,
                       std::shared_ptr<TraceContext> &trace_context,
                       uint64_t source_id,
                       std::string &service_name)
      : EventSpan(trace_environment, trace_context, source_id, span_type::kNicMmio, service_name) {
  }

  // NOTE: we make only a shallow copy
  NicMmioSpan(const NicMmioSpan &other) = default;

  EventSpan *clone() override {
    return new NicMmioSpan(*this);
  }

  ~NicMmioSpan() override = default;

  [[nodiscard]] inline bool IsRead() const {
    return is_read_;
  }

  [[nodiscard]] inline bool IsWrite() const {
    return not IsRead();
  }

  bool AddToSpan(const std::shared_ptr<Event> &event_ptr) override;
};

class NicDmaSpan final : public EventSpan {
  // kNicDmaIT
  std::shared_ptr<Event> dma_issue_ = nullptr;
  // kNicDmaExT
  std::shared_ptr<Event> nic_dma_execution_ = nullptr;
  // kNicDmaCWT or kNicDmaCRT
  std::shared_ptr<Event> nic_dma_completion_ = nullptr;
  bool is_read_ = true;

 public:
  explicit NicDmaSpan(TraceEnvironment &trace_environment,
                      std::shared_ptr<TraceContext> &trace_context,
                      uint64_t source_id,
                      std::string &service_name)
      : EventSpan(trace_environment, trace_context, source_id, span_type::kNicDma, service_name) {
  }

  // NOTE: we make only a shallow copy
  NicDmaSpan(const NicDmaSpan &other) = default;

  EventSpan *clone() override {
    return new NicDmaSpan(*this);
  }

  ~NicDmaSpan() override = default;

  [[nodiscard]] inline bool IsRead() const {
    return is_read_;
  }

  bool AddToSpan(const std::shared_ptr<Event> &event_ptr) override;
};

class NicEthSpan final : public EventSpan {
  // NicTx or NicRx
  std::shared_ptr<Event> tx_rx_ = nullptr;
  bool is_send_ = false;

 public:
  explicit NicEthSpan(TraceEnvironment &trace_environment,
                      std::shared_ptr<TraceContext> &trace_context,
                      uint64_t source_id,
                      std::string &service_name)
      : EventSpan(trace_environment, trace_context, source_id, span_type::kNicEth, service_name) {
  }

  // NOTE: we make only a shallow copy
  NicEthSpan(const NicEthSpan &other) = default;

  EventSpan *clone() override {
    return new NicEthSpan(*this);
  }

  ~NicEthSpan() override = default;

  [[nodiscard]] inline bool IsTransmit() const {
    return is_send_;
  }

  [[nodiscard]] inline bool IsReceive() const {
    return not IsTransmit();
  }

  bool AddToSpan(const std::shared_ptr<Event> &event_ptr) override;
};

class NetDeviceSpan final : public EventSpan {

  std::shared_ptr<NetworkEvent> dev_enq_ = nullptr;
  std::shared_ptr<NetworkEvent> dev_deq_ = nullptr;

  std::shared_ptr<NetworkEvent> drop_ = nullptr;
  NetworkEvent::NetworkDeviceType device_type_;

  NetworkEvent::Ipv4 src_{0};
  NetworkEvent::Ipv4 dst_{0};
  bool ips_set_ = false;
  bool is_arp_ = false;
  std::set<NetworkEvent::EventBoundaryType> boundary_types_;
  bool interesting_flag_ = false;
  int node_ = -1;
  int device_ = -1;

  static bool IsConsistent(const std::shared_ptr<NetworkEvent> &event_a, const std::shared_ptr<NetworkEvent> &event_b);

  bool SetAndCheckPotentialArp(const std::shared_ptr<NetworkEvent> &event_ptr);

  bool SetAndCheckPotentialIp(const std::shared_ptr<NetworkEvent> &event_ptr);

  bool HandelEnqueue(const std::shared_ptr<NetworkEvent> &event_ptr);

  bool HandelDequeue(const std::shared_ptr<NetworkEvent> &event_ptr);

  bool HandelDrop(const std::shared_ptr<NetworkEvent> &event_ptr);

 public:
  explicit NetDeviceSpan(TraceEnvironment &trace_environment,
                         std::shared_ptr<TraceContext> &trace_context,
                         uint64_t source_id,
                         std::string &service_name)
      : EventSpan(trace_environment, trace_context, source_id, span_type::kNetDeviceSpan, service_name) {
  }

  NetDeviceSpan(const NetDeviceSpan &other) = default;

  EventSpan *clone() override {
    return new NetDeviceSpan(*this);
  }

  ~NetDeviceSpan() override = default;

  [[nodiscard]] inline bool IsArp() const {
    return is_arp_;
  }

  [[nodiscard]] inline bool HasIpsSet() const {
    return ips_set_;
  }

  [[nodiscard]] inline bool ContainsBoundaryType(NetworkEvent::EventBoundaryType boundary_type) const {
    return boundary_types_.contains(boundary_type);
  }

  [[nodiscard]] inline NetworkEvent::Ipv4 GetSrcIp() const {
    return src_;
  }

  [[nodiscard]] inline NetworkEvent::Ipv4 GetDstIp() const {
    return dst_;
  }

  [[nodiscard]] inline bool InterestingFlag() const {
    return interesting_flag_;
  }

  [[nodiscard]] inline bool IsDrop() const {
    return drop_ != nullptr;
  }

  [[nodiscard]] inline int GetNode() const {
    return node_;
  }

  [[nodiscard]] inline int GetDevice() const {
    return device_;
  }

  inline std::set<NetworkEvent::EventBoundaryType>
  GetBoundaryTypes() {
    return boundary_types_;
  }

  bool AddToSpan(const std::shared_ptr<Event> &event_ptr) override;
};

class GenericSingleSpan final : public EventSpan {
  std::shared_ptr<Event> event_p_ = nullptr;

 public:
  explicit GenericSingleSpan(TraceEnvironment &trace_environment,
                             std::shared_ptr<TraceContext> &trace_context,
                             uint64_t source_id,
                             std::string &service_name)
      : EventSpan(trace_environment, trace_context, source_id, span_type::kGenericSingle, service_name) {
  }

  // NOTE: we make only a shallow copy
  GenericSingleSpan(const GenericSingleSpan &other) = default;

  EventSpan *clone() override {
    return new GenericSingleSpan(*this);
  }

  bool AddToSpan(const std::shared_ptr<Event> &event_ptr) override {
    if (not IsPotentialAdd(event_ptr) or event_p_) {
      return false;
    }

    if (event_p_) {
      return false;
    }

    event_p_ = event_ptr;
    is_pending_ = false;
    events_.push_back(event_ptr);
    return true;
  }
};

inline bool IsType(std::shared_ptr<EventSpan> &span, span_type type) {
  if (not span) {
    return false;
  }
  return span->GetType() == type;
}

inline std::shared_ptr<EventSpan> CloneShared(const std::shared_ptr<EventSpan> &other) {
  throw_if_empty(other, TraceException::kSpanIsNull, source_loc::current());
  auto raw_ptr = other->clone();
  throw_if_empty(raw_ptr, "EventSpan CloneShared: raw pointer is null", source_loc::current());
  return std::shared_ptr<EventSpan>(raw_ptr);
}

inline std::string GetTypeStr(std::shared_ptr<EventSpan> &span) {
  if (not span) {
    return "";
  }
  std::stringstream sss;
  sss << span->GetType();
  return std::move(sss.str());
}

inline std::ostream &operator<<(std::ostream &out, EventSpan &span) {
  span.Display(out);
  return out;
}

inline std::ostream &operator<<(std::ostream &out, std::shared_ptr<EventSpan> &span) {
  if (span) {
    out << *span;
  } else {
    out << "null";
  }
  return out;
}

#endif  // SIMBRICKS_TRACE_EVENT_SPAN_H_
