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

#include "parser/eventStreamParser.h"

std::shared_ptr<NetworkEvent> EventStreamParser::ParseNetworkEvent(LineHandler &line_handler,
                                                                   EventType event_type,
                                                                   uint64_t timestamp,
                                                                   size_t parser_ident,
                                                                   const std::string &parser_name) {
  static std::function<bool(unsigned char)> device_name_pre = [](unsigned char chara) {
    return sim_string_utils::is_alnum(chara) or chara == ':';
  };
  std::string device_name;
  std::string boundary_type_str;

  int node;
  int device;
  size_t payload_size;
  bool interesting = false;
  uint64_t packet_uid = 0;
  if (not line_handler.ConsumeAndTrimString(", node=") or
      not line_handler.ParseInt(node) or
      not line_handler.ConsumeAndTrimString(", device=") or
      not line_handler.ParseInt(device) or
      not line_handler.ConsumeAndTrimString(", device_name=") or
      not line_handler.ExtractAndSubstrUntilInto(device_name, device_name_pre) or
      not line_handler.ConsumeAndTrimTillString("packet-uid=") or
      not line_handler.ParseUintTrim(10, packet_uid) or
      not line_handler.ConsumeAndTrimTillString("interesting=") or
      not line_handler.ParseBoolFromStringRepr(interesting) or
      not line_handler.ConsumeAndTrimString(", payload_size=") or
      not line_handler.ParseUintTrim(10, payload_size) or
      not line_handler.ConsumeAndTrimString(", boundary_type=") or
      not line_handler.ExtractAndSubstrUntilInto(boundary_type_str, sim_string_utils::is_alnum)) {
    return nullptr;
  }

  NetworkEvent::NetworkDeviceType device_type;
  if (device_name == "ns3::CosimNetDevice") {
    device_type = NetworkEvent::kCosimNetDevice;
  } else if (device_name == "ns3::SimpleNetDevice") {
    device_type = NetworkEvent::kSimpleNetDevice;
  } else {
    return nullptr;
  }

  NetworkEvent::EventBoundaryType boundary_type;
  if (boundary_type_str == "kToAdapter") {
    boundary_type = NetworkEvent::EventBoundaryType::kToAdapter;
  } else if (boundary_type_str == "kFromAdapter") {
    boundary_type = NetworkEvent::EventBoundaryType::kFromAdapter;
  } else if (boundary_type_str == "kWithinSimulator") {
    boundary_type = NetworkEvent::EventBoundaryType::kWithinSimulator;
  } else {
    return nullptr;
  }

  line_handler.TrimL();
  auto eth_h = TryParseEthernetHeader(line_handler);

  line_handler.TrimL();
  auto arp_h = TryParseArpHeader(line_handler);

  line_handler.TrimL();
  auto ip_h = TryParseIpHeader(line_handler);

  switch (event_type) {
    case EventType::kNetworkEnqueueT: {
      return create_shared<NetworkEnqueue>(
          TraceException::kEventIsNull,
          timestamp,
          parser_ident,
          parser_name,
          node,
          device,
          device_type,
          packet_uid,
          interesting,
          payload_size,
          boundary_type,
          eth_h,
          arp_h,
          ip_h
      );
    }
    case EventType::kNetworkDequeueT: {
      return create_shared<NetworkDequeue>(
          TraceException::kEventIsNull,
          timestamp,
          parser_ident,
          parser_name,
          node,
          device,
          device_type,
          packet_uid,
          interesting,
          payload_size,
          boundary_type,
          eth_h,
          arp_h,
          ip_h
      );
    }
    case EventType::kNetworkDropT: {
      return create_shared<NetworkDrop>(
          TraceException::kEventIsNull,
          timestamp,
          parser_ident,
          parser_name,
          node,
          device,
          device_type,
          packet_uid,
          interesting,
          payload_size,
          boundary_type,
          eth_h,
          arp_h,
          ip_h
      );
    }
    default: {
      return nullptr;
    }
  }
}

concurrencpp::result<std::shared_ptr<Event>>
EventStreamParser::ParseEvent(LineHandler &line_handler) {
  line_handler.TrimL();
  std::function<bool(unsigned char)> pred = [](unsigned char c) {
    return c != ':';
  };
  std::string event_name = line_handler.ExtractAndSubstrUntil(pred);
  if (event_name.empty()) {
    spdlog::info("could not parse event name: {}", line_handler.GetRawLine());
    co_return nullptr;
  }

  uint64_t ts;
  size_t parser_ident;
  std::string p_name;
  if (not ParseIdentNameTs(line_handler, parser_ident, p_name, ts)) {
    spdlog::info("could not parse timestamp or source: {}", line_handler.GetRawLine());
    co_return nullptr;
  }
  const std::string *singleton = trace_environment_.InternalizeAdditional(p_name);
  const std::string &parser_name = *(singleton);

  std::shared_ptr<Event> event = nullptr;
  uint64_t pc = 0, id = 0, addr = 0, vec = 0, dev = 0, func = 0,
      data = 0, reg = 0, offset = 0, intr = 0,
      val = 0;
  int bar = 0, port = 0;
  size_t len = 0, size = 0, bytes = 0;
  bool posted;
  std::string function, component;

  if (event_name == "SimSendSyncSimSendSync") {
    event = std::make_shared<SimSendSync>(ts, parser_ident, parser_name);

  } else if (event_name == "SimProcInEvent") {
    event = std::make_shared<SimProcInEvent>(ts, parser_ident, parser_name);

  } else if (event_name == "HostInstr") {
    if (not line_handler.ConsumeAndTrimString(", pc=") or
        not line_handler.ParseUintTrim(16, pc)) {
      std::cout << "error parsing HostInstr" << '\n';
      co_return nullptr;
    }
    event = std::make_shared<HostInstr>(ts, parser_ident, parser_name, pc);

  } else if (event_name == "HostCall") {
    if (not line_handler.ConsumeAndTrimString(", pc=") or
        not line_handler.ParseUintTrim(16, pc) or
        not line_handler.ConsumeAndTrimString(", func=") or
        not line_handler.ExtractAndSubstrUntilInto(
            function, sim_string_utils::is_alnum_dot_bar) or
        not line_handler.ConsumeAndTrimString(", comp=") or
        not line_handler.ExtractAndSubstrUntilInto(
            component, sim_string_utils::is_alnum_dot_bar)) {
      spdlog::info("error parsing HostInstr");
      co_return nullptr;
    }
    const std::string *func_ptr =
        trace_environment_.InternalizeAdditional(function);
    const std::string *comp =
        trace_environment_.InternalizeAdditional(component);

    event = std::make_shared<HostCall>(ts, parser_ident, parser_name, pc,
                                       func_ptr, comp);

  } else if (event_name == "HostMmioImRespPoW") {
    event =
        std::make_shared<HostMmioImRespPoW>(ts, parser_ident, parser_name);

  } else if (event_name == "HostMmioCR" or
      event_name == "HostMmioCW" or
      event_name == "HostDmaC") {
    if (not line_handler.ConsumeAndTrimString(", id=") or
        not line_handler.ParseUintTrim(10, id)) {
      spdlog::info("error parsing HostMmioCR, HostMmioCW or HostDmaC");
      co_return nullptr;
    }

    if (event_name == "HostMmioCR") {
      event =
          std::make_shared<HostMmioCR>(ts, parser_ident, parser_name, id);
    } else if (event_name == "HostMmioCW") {
      event =
          std::make_shared<HostMmioCW>(ts, parser_ident, parser_name, id);
    } else {
      event = std::make_shared<HostDmaC>(ts, parser_ident, parser_name, id);
    }

  } else if (event_name == "HostMmioR" or
      event_name == "HostMmioW" or
      event_name == "HostDmaR" or
      event_name == "HostDmaW") {
    if (not line_handler.ConsumeAndTrimString(", id=") or
        not line_handler.ParseUintTrim(10, id) or
        not line_handler.ConsumeAndTrimString(", addr=") or
        not line_handler.ParseUintTrim(16, addr) or
        not line_handler.ConsumeAndTrimString(", size=") or
        not line_handler.ParseUintTrim(16, size)) {
      spdlog::info("error parsing HostMmioR, HostMmioW, HostDmaR or HostDmaW");
      co_return nullptr;
    }

    if (event_name == "HostMmioR" or
        event_name == "HostMmioW") {
      if (not line_handler.ConsumeAndTrimString(", bar=") or
          not line_handler.ParseInt(bar) or
          not line_handler.ConsumeAndTrimString(", offset=") or
          not line_handler.ParseUintTrim(16, offset)) {
        spdlog::info("error parsing HostMmioR, HostMmioW bar or offset");
        co_return nullptr;
      }

      if (event_name == "HostMmioW") {
        if (not line_handler.ConsumeAndTrimString(", posted=") or
            not line_handler.ParseBoolFromStringRepr(posted)) {
          spdlog::info("error parsing HostMmioW posted");
          co_return nullptr;
        }
        event = std::make_shared<HostMmioW>(ts, parser_ident, parser_name,
                                            id, addr, size, bar, offset, posted);
      } else {
        event = std::make_shared<HostMmioR>(ts, parser_ident, parser_name,
                                            id, addr, size, bar, offset);
      }
    } else if (event_name == "HostDmaR") {
      event = std::make_shared<HostDmaR>(ts, parser_ident, parser_name, id,
                                         addr, size);
    } else {
      event = std::make_shared<HostDmaW>(ts, parser_ident, parser_name, id,
                                         addr, size);
    }

  } else if (event_name == "HostMsiX") {
    if (not line_handler.ConsumeAndTrimString(", vec=") or
        not line_handler.ParseUintTrim(10, vec)) {
      spdlog::info("error parsing HostMsiX");
      co_return nullptr;
    }
    event = std::make_shared<HostMsiX>(ts, parser_ident, parser_name, vec);

  } else if (event_name == "HostConfRead" or
      event_name == "HostConfWrite") {
    if (not line_handler.ConsumeAndTrimString(", dev=") or
        not line_handler.ParseUintTrim(16, dev) or
        not line_handler.ConsumeAndTrimString(", func=") or
        not line_handler.ParseUintTrim(16, func) or
        not line_handler.ConsumeAndTrimString(", reg=") or
        not line_handler.ParseUintTrim(16, reg) or
        not line_handler.ConsumeAndTrimString(", bytes=") or
        not line_handler.ParseUintTrim(10, bytes) or
        not line_handler.ConsumeAndTrimString(", data=") or
        not line_handler.ParseUintTrim(16, data)) {
      spdlog::info("error parsing HostConfRead or HostConfWrite");
      co_return nullptr;
    }

    if (event_name == "HostConfRead") {
      event = std::make_shared<HostConf>(ts, parser_ident, parser_name, dev,
                                         func, reg, bytes, data, true);
    } else {
      event = std::make_shared<HostConf>(ts, parser_ident, parser_name, dev,
                                         func, reg, bytes, data, false);
    }

  } else if (event_name == "HostClearInt") {
    event = std::make_shared<HostClearInt>(ts, parser_ident, parser_name);

  } else if (event_name == "HostPostInt") {
    event = std::make_shared<HostPostInt>(ts, parser_ident, parser_name);

  } else if (event_name == "HostPciR" or
      event_name == "HostPciW") {
    if (not line_handler.ConsumeAndTrimString(", offset=") or
        not line_handler.ParseUintTrim(16, offset) or
        not line_handler.ConsumeAndTrimString(", size=") or
        not line_handler.ParseUintTrim(10, size)) {
      spdlog::info("error parsing HostPciR or HostPciW");
      co_return nullptr;
    }

    if (event_name == "HostPciR") {
      event = std::make_shared<HostPciRW>(ts, parser_ident, parser_name,
                                          offset, size, true);
    } else {
      event = std::make_shared<HostPciRW>(ts, parser_ident, parser_name,
                                          offset, size, false);
    }

  } else if (event_name == "NicMsix" or
      event_name == "NicMsi") {
    if (not line_handler.ConsumeAndTrimString(", vec=") or
        not line_handler.ParseUintTrim(10, vec)) {
      spdlog::info("error parsing NicMsix");
      co_return nullptr;
    }

    if (event_name == "NicMsix") {
      event = std::make_shared<NicMsix>(ts, parser_ident, parser_name, vec,
                                        true);
    } else {
      event = std::make_shared<NicMsix>(ts, parser_ident, parser_name, vec,
                                        false);
    }

  } else if (event_name == "SetIX") {
    if (not line_handler.ConsumeAndTrimString(", interrupt=") or
        not line_handler.ParseUintTrim(16, intr)) {
      std::cout << "error parsing NicMsix" << '\n';
      co_return nullptr;
    }
    event = std::make_shared<SetIX>(ts, parser_ident, parser_name, intr);

  } else if (event_name == "NicDmaI" or
      event_name == "NicDmaEx" or
      event_name == "NicDmaEn" or
      event_name == "NicDmaCR" or
      event_name == "NicDmaCW") {
    if (not line_handler.ConsumeAndTrimString(", id=") or
        not line_handler.ParseUintTrim(10, id) or
        not line_handler.ConsumeAndTrimString(", addr=") or
        not line_handler.ParseUintTrim(16, addr) or
        not line_handler.ConsumeAndTrimString(", size=") or
        not line_handler.ParseUintTrim(16, len)) {
      spdlog::info("error parsing NicDmaI, NicDmaEx, NicDmaEn, NicDmaCR or NicDmaCW");
      co_return nullptr;
    }

    if (event_name == "NicDmaI") {
      event = std::make_shared<NicDmaI>(ts, parser_ident, parser_name, id,
                                        addr, len);
    } else if (event_name == "NicDmaEx") {
      event = std::make_shared<NicDmaEx>(ts, parser_ident, parser_name, id,
                                         addr, len);
    } else if (event_name == "NicDmaEn") {
      event = std::make_shared<NicDmaEn>(ts, parser_ident, parser_name, id,
                                         addr, len);
    } else if (event_name == "NicDmaCW") {
      event = std::make_shared<NicDmaCW>(ts, parser_ident, parser_name, id,
                                         addr, len);
    } else {
      event = std::make_shared<NicDmaCR>(ts, parser_ident, parser_name, id,
                                         addr, len);
    }

  } else if (event_name == "NicMmioR" or
      event_name == "NicMmioW") {
    if (not line_handler.ConsumeAndTrimString(", off=") or
        not line_handler.ParseUintTrim(16, offset) or
        not line_handler.ConsumeAndTrimString(", len=") or
        not line_handler.ParseUintTrim(16, len) or
        not line_handler.ConsumeAndTrimString(", val=") or
        not line_handler.ParseUintTrim(16, val)) {
      spdlog::info("error parsing NicMmioR or NicMmioW: {}", line_handler.GetRawLine());
      co_return nullptr;
    }

    if (event_name == "NicMmioR") {
      event = std::make_shared<NicMmioR>(ts, parser_ident, parser_name,
                                         offset, len, val);
    } else {
      if (not line_handler.ConsumeAndTrimString(", posted=") or
          not line_handler.ParseBoolFromStringRepr(posted)) {
        spdlog::info("error parsing NicMmioW: {}", line_handler.GetRawLine());
        co_return nullptr;
      }
      event = std::make_shared<NicMmioW>(ts, parser_ident, parser_name,
                                         offset, len, val, posted);
    }

  } else if (event_name == "NicTx") {
    if (not line_handler.ConsumeAndTrimString(", len=") or
        not line_handler.ParseUintTrim(16, len)) {
      spdlog::info("error parsing NicTx");
      co_return nullptr;
    }
    event = std::make_shared<NicTx>(ts, parser_ident, parser_name, len);

  } else if (event_name == "NicRx") {
    if (not line_handler.ConsumeAndTrimString(", len=") or
        not line_handler.ParseUintTrim(16, len) or
        not line_handler.ConsumeAndTrimString(", is_read=true") or
        not line_handler.ConsumeAndTrimString(", port=") or
        not line_handler.ParseInt(port)) {
      spdlog::info("error parsing NicRx");
      co_return nullptr;
    }
    event = std::make_shared<NicRx>(ts, parser_ident,
                                    parser_name, len, addr);
  } else if (event_name == "NetworkEnqueue") {
    co_return ParseNetworkEvent(line_handler, EventType::kNetworkEnqueueT, ts, parser_ident, parser_name);
  } else if (event_name == "NetworkDequeue") {
    co_return ParseNetworkEvent(line_handler, EventType::kNetworkDequeueT, ts, parser_ident, parser_name);
  } else if (event_name == "NetworkDrop") {
    co_return ParseNetworkEvent(line_handler, EventType::kNetworkDropT, ts, parser_ident, parser_name);
  } else {
    spdlog::info("unknown event found, it will be skipped");
    co_return nullptr;
  }

  throw_if_empty(event, "event stream parser must have an event when returning an event",
                 source_loc::current());
  co_return event;
}
