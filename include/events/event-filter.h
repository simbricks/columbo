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

#ifndef SIMBRICKS_TRACE_EVENT_FILTER_H_
#define SIMBRICKS_TRACE_EVENT_FILTER_H_

#include <memory>

#include "sync/corobelt.h"
#include "events/events.h"
#include "util/exception.h"
#include "events/eventTimeBoundary.h"
#include "analytics/helper.h"
#include "events/printer.h"

class EventStreamActor : public Handler<std::shared_ptr<Event>> {
 protected:
  TraceEnvironment &trace_environment_;

  explicit EventStreamActor(TraceEnvironment &trace_environment)
      : Handler<std::shared_ptr<Event>>(), trace_environment_(trace_environment) {}
};

class GenericEventFilter : public EventStreamActor {
  std::function<bool(const std::shared_ptr<Event> &event)> &to_filter_;

 public:
  concurrencpp::result<bool> handel(std::shared_ptr<concurrencpp::executor> executor,
                                    std::shared_ptr<Event> &value) override {
    throw_if_empty(value, TraceException::kEventIsNull, source_loc::current());
    co_return to_filter_(value);
  };

  explicit GenericEventFilter(TraceEnvironment &trace_environment,
                              std::function<bool(const std::shared_ptr<Event> &event)> &to_filter)
      : EventStreamActor(trace_environment), to_filter_(to_filter) {
  }
};

class EventTypeFilter : public EventStreamActor {
  const std::set<EventType> &types_to_filter_;
  bool inverted_;

 public:
  concurrencpp::result<bool> handel(std::shared_ptr<concurrencpp::executor> executor,
                                    std::shared_ptr<Event> &value) override {
    throw_if_empty(value, TraceException::kEventIsNull, source_loc::current());

    spdlog::trace("EventTypeFilter filter act on {}", *value);

    const EventType type = value->GetType();
    if (inverted_) {
      co_return not types_to_filter_.contains(type);
    }

    co_return types_to_filter_.contains(type);
  };

  explicit EventTypeFilter(TraceEnvironment &trace_environment,
                           const std::set<EventType> &types_to_filter,
                           bool invert_filter = false)
      : EventStreamActor(trace_environment),
        types_to_filter_(types_to_filter),
        inverted_(invert_filter) {
  }
};

class EventTimestampFilter : public EventStreamActor {
  std::vector<EventTimeBoundary> &event_time_boundaries_;

 public:
  concurrencpp::result<bool> handel(std::shared_ptr<concurrencpp::executor> executor,
                                    std::shared_ptr<Event> &value) override {
    throw_if_empty(value, TraceException::kEventIsNull, source_loc::current());

    spdlog::trace("EventTimestampFilter filter act on {}", *value);

    const uint64_t timestamp = value->GetTs();

    for (auto &boundary : event_time_boundaries_) {
      if (boundary.lower_bound_ <= timestamp && timestamp <= boundary.upper_bound_) {
        co_return true;
      }
    }
    co_return false;
  };

  explicit EventTimestampFilter(TraceEnvironment &trace_environment,
                                std::vector<EventTimeBoundary> &event_time_boundaries)
      : EventStreamActor(trace_environment),
        event_time_boundaries_(event_time_boundaries) {
  }
};

class HostCallFuncFilter : public EventStreamActor {
  bool blacklist_;
  std::set<const std::string *> list_;

 public:
  concurrencpp::result<bool> handel(std::shared_ptr<concurrencpp::executor> executor,
                                    std::shared_ptr<Event> &value) override {
    throw_if_empty(value, TraceException::kEventIsNull, source_loc::current());

    if (not IsType(value, EventType::kHostCallT)) {
      co_return true;
    }

    const std::shared_ptr<HostCall> &call = std::static_pointer_cast<HostCall>(value);
    if (blacklist_ and list_.contains(call->GetFunc())) {
      co_return false;
    }
    if (not blacklist_ and not list_.contains(call->GetFunc())) {
      co_return false;
    }

    co_return true;
  };

  explicit HostCallFuncFilter(TraceEnvironment &trace_environment,
                              const std::set<std::string> &list,
                              bool blacklist = true)
      : EventStreamActor(trace_environment), blacklist_(blacklist) {
    for (const std::string &str : list) {
      const std::string *sym = this->trace_environment_.InternalizeAdditional(str);
      list_.insert(sym);
    }
  }
};

class NS3EventFilter : public EventStreamActor {
  using EveTyList = std::vector<EventType>;

  const NodeDeviceFilter &node_device_filter_;

 public:
  concurrencpp::result<bool> handel(std::shared_ptr<concurrencpp::executor> executor,
                                    std::shared_ptr<Event> &value) override {
    if (not value) {
      co_return false; // filter null
    }

    if (not IsAnyType(value,
                      EveTyList{EventType::kNetworkEnqueueT, EventType::kNetworkDequeueT,
                                EventType::kNetworkDequeueT})) {
      co_return true; // we only apply this filter on network events
    }

    const auto &network_event = std::static_pointer_cast<NetworkEvent>(value);
    if (network_event->InterestingFlag() and node_device_filter_.IsNotInterestingNodeDevice(network_event)) {
      co_return false;
    }

    if (not network_event->InterestingFlag()) {
      const bool res = node_device_filter_.IsInterestingNodeDevice(network_event)
          and IsDeviceType(network_event, NetworkEvent::NetworkDeviceType::kCosimNetDevice);
      co_return res;
    }

    co_return true;
  };

  explicit NS3EventFilter(TraceEnvironment &trace_environment,
                          const NodeDeviceFilter &node_device_filter)
      : EventStreamActor(trace_environment), node_device_filter_(node_device_filter) {}
};

#endif // SIMBRICKS_TRACE_EVENT_FILTER_H_