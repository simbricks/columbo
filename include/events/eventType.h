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

#include <iostream>
#include <unordered_map>

#include "util/exception.h"

#ifndef SIMBRICKS_TRACE_EVENTS_EVENTTYPE_H_
#define SIMBRICKS_TRACE_EVENTS_EVENTTYPE_H_

enum EventType {
  kEventT,
  kSimSendSyncT,
  kSimProcInEventT,
  kHostInstrT,
  kHostCallT,
  kHostMmioImRespPoWT,
  kHostIdOpT,
  kHostMmioCRT,
  kHostMmioCWT,
  kHostAddrSizeOpT,
  kHostMmioRT,
  kHostMmioWT,
  kHostDmaCT,
  kHostDmaRT,
  kHostDmaWT,
  kHostMsiXT,
  kHostConfT,
  kHostClearIntT,
  kHostPostIntT,
  kHostPciRWT,
  kNicMsixT,
  kNicDmaT,
  kSetIXT,
  kNicDmaIT,
  kNicDmaExT,
  kNicDmaEnT,
  kNicDmaCRT,
  kNicDmaCWT,
  kNicMmioT,
  kNicMmioRT,
  kNicMmioWT,
  kNicTrxT,
  kNicTxT,
  kNicRxT,
  kNetworkEnqueueT,
  kNetworkDequeueT,
  kNetworkDropT,
};

inline std::ostream &operator<<(std::ostream &into, EventType type) {
  switch (type) {
    case EventType::kEventT:into << "kEventT";
      break;
    case EventType::kSimSendSyncT:into << "kSimSendSyncT";
      break;
    case EventType::kSimProcInEventT:into << "kSimProcInEventT";
      break;
    case EventType::kHostInstrT:into << "kHostInstrT";
      break;
    case EventType::kHostCallT:into << "kHostCallT";
      break;
    case EventType::kHostMmioImRespPoWT:into << "kHostMmioImRespPoWT";
      break;
    case EventType::kHostIdOpT:into << "kHostIdOpT";
      break;
    case EventType::kHostMmioCRT:into << "kHostMmioCRT";
      break;
    case EventType::kHostMmioCWT:into << "kHostMmioCWT";
      break;
    case EventType::kHostAddrSizeOpT:into << "kHostAddrSizeOpT";
      break;
    case EventType::kHostMmioRT:into << "kHostMmioRT";
      break;
    case EventType::kHostMmioWT:into << "kHostMmioWT";
      break;
    case EventType::kHostDmaCT:into << "kHostDmaCT";
      break;
    case EventType::kHostDmaRT:into << "kHostDmaRT";
      break;
    case EventType::kHostDmaWT:into << "kHostDmaWT";
      break;
    case EventType::kHostMsiXT:into << "kHostMsiXT";
      break;
    case EventType::kHostConfT:into << "kHostConfT";
      break;
    case EventType::kHostClearIntT:into << "kHostClearIntT";
      break;
    case EventType::kHostPostIntT:into << "kHostPostIntT";
      break;
    case EventType::kHostPciRWT:into << "kHostPciRWT";
      break;
    case EventType::kNicMsixT:into << "kNicMsixT";
      break;
    case EventType::kNicDmaT:into << "kNicDmaT";
      break;
    case EventType::kSetIXT:into << "kSetIXT";
      break;
    case EventType::kNicDmaIT:into << "kNicDmaIT";
      break;
    case EventType::kNicDmaExT:into << "kNicDmaExT";
      break;
    case EventType::kNicDmaEnT:into << "kNicDmaEnT";
      break;
    case EventType::kNicDmaCRT:into << "kNicDmaCRT";
      break;
    case EventType::kNicDmaCWT:into << "kNicDmaCWT";
      break;
    case EventType::kNicMmioT:into << "kNicMmioT";
      break;
    case EventType::kNicMmioRT:into << "kNicMmioRT";
      break;
    case EventType::kNicMmioWT:into << "kNicMmioWT";
      break;
    case EventType::kNicTrxT:into << "kNicTrxT";
      break;
    case EventType::kNicTxT:into << "kNicTxT";
      break;
    case EventType::kNicRxT:into << "kNicRxT";
      break;
    case EventType::kNetworkEnqueueT:into << "kNetworkEnqueueT";
      break;
    case EventType::kNetworkDequeueT:into << "kNetworkDequeueT";
      break;
    case EventType::kNetworkDropT:into << "kNetworkDropT";
      break;
    default:throw_just(source_loc::current(), "encountered unknown event type");
  }
  return into;
}

inline EventType EventTypeFromString(const std::string &event_type_str) {
  static const std::unordered_map<std::string, EventType> kTypeLookup{
      {"kEventT", kEventT},
      {"kSimSendSyncT", kSimSendSyncT},
      {"kSimProcInEventT", kSimProcInEventT},
      {"kHostInstrT", kHostInstrT},
      {"kHostCallT", kHostCallT},
      {"kHostMmioImRespPoWT", kHostMmioImRespPoWT},
      {"kHostIdOpT", kHostIdOpT},
      {"kHostMmioCRT", kHostMmioCRT},
      {"kHostMmioCWT", kHostMmioCWT},
      {"kHostAddrSizeOpT", kHostAddrSizeOpT},
      {"kHostMmioRT", kHostMmioRT},
      {"kHostMmioWT", kHostMmioWT},
      {"kHostDmaCT", kHostDmaCT},
      {"kHostDmaRT", kHostDmaRT},
      {"kHostDmaWT", kHostDmaWT},
      {"kHostMsiXT", kHostMsiXT},
      {"kHostConfT", kHostConfT},
      {"kHostClearIntT", kHostClearIntT},
      {"kHostPostIntT", kHostPostIntT},
      {"kHostPciRWT", kHostPciRWT},
      {"kNicMsixT", kNicMsixT},
      {"kNicDmaT", kNicDmaT},
      {"kSetIXT", kSetIXT},
      {"kNicDmaIT", kNicDmaIT},
      {"kNicDmaExT", kNicDmaExT},
      {"kNicDmaEnT", kNicDmaEnT},
      {"kNicDmaCRT", kNicDmaCRT},
      {"kNicDmaCWT", kNicDmaCWT},
      {"kNicMmioT", kNicMmioT},
      {"kNicMmioRT", kNicMmioRT},
      {"kNicMmioWT", kNicMmioWT},
      {"kNicTrxT", kNicTrxT},
      {"kNicTxT", kNicTxT},
      {"kNicRxT", kNicRxT},
      {"kNetworkEnqueueT", kNetworkEnqueueT},
      {"kNetworkDequeueT", kNetworkDequeueT},
      {"kNetworkDropT", kNetworkDropT}
  };

  auto iter = kTypeLookup.find(event_type_str);
  if (iter != kTypeLookup.end()) {
    return iter->second;
  }
  std::cerr << "could not map string '" << event_type_str << "' onto event type. Fallback to kEventT" << '\n';
  return EventType::kEventT;
}

#endif //SIMBRICKS_TRACE_EVENTS_EVENTTYPE_H_
