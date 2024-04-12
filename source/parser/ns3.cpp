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

#include "parser/parser.h"

std::shared_ptr<Event> NS3Parser::ParseNetDevice(LineHandler &line_handler,
                                                 uint64_t timestamp,
                                                 EventType type,
                                                 int node,
                                                 int device,
                                                 const NetworkEvent::NetworkDeviceType device_type) {
  line_handler.TrimL();
  NetworkEvent::EventBoundaryType boundary_type = NetworkEvent::EventBoundaryType::kWithinSimulator;
  if (line_handler.ConsumeAndTrimTillString("RxPacketFromAdapter")) {
    boundary_type = NetworkEvent::EventBoundaryType::kFromAdapter;
  } else if (line_handler.ConsumeAndTrimTillString("TxPacketToAdapter")) {
    boundary_type = NetworkEvent::EventBoundaryType::kToAdapter;
  }

  bool interesting = false;
  uint64_t packet_uid = 0;
  if (not line_handler.ConsumeAndTrimTillString("Packet-Uid=") or
      not line_handler.ParseUintTrim(10, packet_uid) or
      not line_handler.ConsumeAndTrimTillString("Intersting=") or
      not line_handler.ParseBoolFromStringRepr(interesting)) {
    return nullptr;
  }

  std::optional<NetworkEvent::EthernetHeader> eth_header;
  eth_header = TryParseEthernetHeader(line_handler);

  std::optional<NetworkEvent::ArpHeader> arp_header;
  arp_header = TryParseArpHeader(line_handler);

  std::optional<NetworkEvent::Ipv4Header> ip_header;
  ip_header = TryParseIpHeader(line_handler);

  size_t payload_size = 0;
  if (line_handler.ConsumeAndTrimTillString("Payload (size=") and not line_handler.ParseUintTrim(10, payload_size)) {
    return nullptr;
  }

  switch (type) {
    case EventType::kNetworkEnqueueT: {
      return create_shared<NetworkEnqueue>("could not create NetworkEnqueue event",
                                           timestamp,
                                           GetIdent(),
                                           GetName(),
                                           node,
                                           device,
                                           device_type,
                                           packet_uid,
                                           interesting,
                                           payload_size,
                                           boundary_type,
                                           eth_header,
                                           arp_header,
                                           ip_header);
    }
    case EventType::kNetworkDequeueT: {
      return create_shared<NetworkDequeue>("could not create NetworkDequeue event",
                                           timestamp,
                                           GetIdent(),
                                           GetName(),
                                           node,
                                           device,
                                           device_type,
                                           packet_uid,
                                           interesting,
                                           payload_size,
                                           boundary_type,
                                           eth_header,
                                           arp_header,
                                           ip_header);
    }
    case EventType::kNetworkDropT: {
      return create_shared<NetworkDrop>("could not create NetworkDequeue event",
                                        timestamp,
                                        GetIdent(),
                                        GetName(),
                                        node,
                                        device,
                                        device_type,
                                        packet_uid,
                                        interesting,
                                        payload_size,
                                        boundary_type,
                                        eth_header,
                                        arp_header,
                                        ip_header);
    }
    default: {
      return nullptr;
    }
  }
}

concurrencpp::result<std::shared_ptr<Event>>
NS3Parser::ParseEvent(LineHandler &line_handler) {
  if (line_handler.IsEmpty()) {
    co_return nullptr;
  }

  std::shared_ptr<Event> event_ptr;
  EventType type;
  if (line_handler.ConsumeAndTrimChar('+')) {
    type = EventType::kNetworkEnqueueT;
  } else if (line_handler.ConsumeAndTrimChar('-')) {
    type = EventType::kNetworkDequeueT;
  } else if (line_handler.ConsumeAndTrimChar('d')) {
    type = EventType::kNetworkDropT;
  } else {
    co_return nullptr;
  }

  uint64_t timestamp = 0;
  line_handler.TrimL();
  if (not ParseTimestamp(line_handler, timestamp)) {
    co_return nullptr;
  }

  int node;
  if (not line_handler.ConsumeAndTrimTillString("NodeList/") or not line_handler.ParseInt(node)) {
    co_return nullptr;
  }

  int device;
  if (not line_handler.ConsumeAndTrimTillString("DeviceList/") or not line_handler.ParseInt(device)) {
    co_return nullptr;
  }

  NetworkEvent::NetworkDeviceType device_type;
  if (line_handler.ConsumeAndTrimTillString("ns3::SimpleNetDevice")) {
    device_type = NetworkEvent::NetworkDeviceType::kSimpleNetDevice;
  } else if (line_handler.ConsumeAndTrimTillString("ns3::CosimNetDevice")) {
    device_type = NetworkEvent::NetworkDeviceType::kCosimNetDevice;
  } else {
    co_return nullptr;
  }

  event_ptr = ParseNetDevice(line_handler, timestamp, type, node, device, device_type);
  co_return event_ptr;
}
