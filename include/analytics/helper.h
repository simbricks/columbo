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

#include <set>
#include <memory>
#include <map>

#include "events/events.h"
#include "analytics/context.h"
#include "sync/channel.h"

#ifndef SIMBRICKS_TRACE_INCLUDE_ANALYTICS_HELPER_H_
#define SIMBRICKS_TRACE_INCLUDE_ANALYTICS_HELPER_H_

class NodeDeviceToChannelMap {
  using ChanT = std::shared_ptr<CoroChannel<std::shared_ptr<Context>>>;
  std::map<std::pair<int, int>, ChanT> mapping_;

  void AddMapping(const std::pair<int, int> &key, const ChanT &channel) {
    auto suc = mapping_.insert({key, channel});
    throw_on_false(suc.second, "NetworkSpanner ip address already mapped",
                   source_loc::current());
  }

  ChanT GetChannel(const std::pair<int, int> &node_device) const {
    auto iter = mapping_.find(node_device);
    if (iter != mapping_.end()) {
      return iter->second;
    }
    return nullptr;
  }

 public:
  explicit NodeDeviceToChannelMap() = default;

  void AddMapping(int node, int device, const ChanT &channel) {
    auto key = std::make_pair(node, device);
    AddMapping(key, channel);
  }

  ChanT GetValidChannel(int node, int device) const {
    auto result = GetChannel(std::make_pair(node, device));
    throw_if_empty(result, TraceException::kChannelIsNull, source_loc::current());
    return result;
  }
};

class NodeDeviceFilter {
  std::set<std::pair<int, int>> interesting_node_device_pairs_;

 public:
  explicit NodeDeviceFilter() = default;

  void AddNodeDevice(const std::pair<int, int> &node_device) {
    interesting_node_device_pairs_.insert(node_device);
  }

  void AddNodeDevice(int node, int device) {
    interesting_node_device_pairs_.insert({node, device});
  }

  void AddNodeDevice(const std::shared_ptr<NetworkEvent> &event) {
    if (not event) {
      return;
    }
    const int node = event->GetNode();
    const int device = event->GetDevice();
    interesting_node_device_pairs_.insert({node, device});
  }

  bool IsInterestingNodeDevice(int node, int device) const {
    return interesting_node_device_pairs_.contains({node, device});
  }

  bool IsInterestingNodeDevice(const std::shared_ptr<NetworkEvent> &event) const {
    if (not event) {
      return false;
    }
    const int node = event->GetNode();
    const int device = event->GetDevice();
    return IsInterestingNodeDevice(node, device);
  }

  bool IsNotInterestingNodeDevice(const std::shared_ptr<NetworkEvent> &event) const {
    return not IsInterestingNodeDevice(event);
  }
};

#endif //SIMBRICKS_TRACE_INCLUDE_ANALYTICS_HELPER_H_
