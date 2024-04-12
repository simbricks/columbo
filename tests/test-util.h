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

#include <optional>

#include "events/events.h"

#ifndef SIMBRICKS_TRACE_TESTS_UTIL_H_
#define SIMBRICKS_TRACE_TESTS_UTIL_H_

inline std::optional<NetworkEvent::EthernetHeader> CreateEthHeader(size_t length_type,
                                                                   unsigned char src_m1,
                                                                   unsigned char src_m2,
                                                                   unsigned char src_m3,
                                                                   unsigned char src_m4,
                                                                   unsigned char src_m5,
                                                                   unsigned char src_m6,
                                                                   unsigned char dst_m1,
                                                                   unsigned char dst_m2,
                                                                   unsigned char dst_m3,
                                                                   unsigned char dst_m4,
                                                                   unsigned char dst_m5,
                                                                   unsigned char dst_m6) {

  const NetworkEvent::MacAddress src{{src_m1, src_m2, src_m3, src_m4, src_m5, src_m6}};
  const NetworkEvent::MacAddress dst{{dst_m1, dst_m2, dst_m3, dst_m4, dst_m5, dst_m6}};
  NetworkEvent::EthernetHeader header;
  header.length_type_ = length_type;
  header.src_mac_ = src;
  header.dst_mac_ = dst;
  return header;
}

inline std::optional<NetworkEvent::ArpHeader> CreateArpHeader(bool is_request,
                                                              uint32_t src_o1,
                                                              uint32_t src_o2,
                                                              uint32_t src_o3,
                                                              uint32_t src_o4,
                                                              uint32_t dst_o1,
                                                              uint32_t dst_o2,
                                                              uint32_t dst_o3,
                                                              uint32_t dst_o4) {
  NetworkEvent::ArpHeader header;
  // const NetworkEvent::MacAddress src_mac{{src_m1, src_m2, src_m3, src_m4, src_m5, src_m6}};
  // header.src_mac_ = src_mac;
  header.is_request_ = is_request;
  const uint32_t src = (src_o1 << 24) | (src_o2 << 16) | (src_o3 << 8) | src_o4;
  const uint32_t dst = (dst_o1 << 24) | (dst_o2 << 16) | (dst_o3 << 8) | dst_o4;
  header.src_ip_ = NetworkEvent::Ipv4{src};
  header.dst_ip_ = NetworkEvent::Ipv4{dst};
  return header;
}

inline std::optional<NetworkEvent::Ipv4Header> CreateIpHeader(size_t length,
                                                              uint32_t src_o1,
                                                              uint32_t src_o2,
                                                              uint32_t src_o3,
                                                              uint32_t src_o4,
                                                              uint32_t dst_o1,
                                                              uint32_t dst_o2,
                                                              uint32_t dst_o3,
                                                              uint32_t dst_o4) {
  NetworkEvent::Ipv4Header header;
  header.length_ = length;
  const uint32_t src = (src_o1 << 24) | (src_o2 << 16) | (src_o3 << 8) | src_o4;
  const uint32_t dst = (dst_o1 << 24) | (dst_o2 << 16) | (dst_o3 << 8) | dst_o4;
  header.src_ip_ = NetworkEvent::Ipv4{src};
  header.dst_ip_ = NetworkEvent::Ipv4{dst};
  return header;
}

#endif // SIMBRICKS_TRACE_TESTS_UTIL_H_
