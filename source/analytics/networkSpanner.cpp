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

#include <utility>

#include "analytics/spanner.h"
#include "events/printer.h"

concurrencpp::result<bool> NetworkSpanner::HandelNetworkEvent(std::shared_ptr<concurrencpp::executor> resume_executor,
                                                              std::shared_ptr<Event> &event_ptr) {
  assert(event_ptr and "event_ptr is null");

  if (not IsAnyType(event_ptr, std::vector<EventType>{
      EventType::kNetworkEnqueueT, EventType::kNetworkDequeueT, EventType::kNetworkDropT})) {
    spdlog::warn("NetworkSpanner::HandelNetworkEvent wrong event type: {}", *event_ptr);
    co_return false;
  }
  auto network_event = std::static_pointer_cast<NetworkEvent>(event_ptr);

  // Handling events caused by messages marked as interesting that are not! (ARP)
  //       ---> filter out spans and events that end up in devices we are not interested in!
  if (network_event->InterestingFlag() and node_device_filter_.IsNotInterestingNodeDevice(network_event)) {
    spdlog::debug("NetworkSpanner::HandelNetworkEvent filtered interesting event because of node device: {}",
                  *network_event);
    co_return true;
  }

  auto current_device_span = iterate_add_erase(current_active_device_spans_, network_event);
  if (current_device_span) {

    throw_on_false(current_device_span->IsComplete(),
                   "network spanner, after adding event, span must be complete",
                   source_loc::current());

    // we make the connection at the end to sending NIC
    if (current_device_span->ContainsBoundaryType(NetworkEvent::EventBoundaryType::kToAdapter)
        and not current_device_span->IsDrop()) {

      // if we have a "to" adapter event we need to push a context to a host
      throw_on_false(current_device_span->HasIpsSet(), "kToAdapter event has no ip header",
                     source_loc::current());

      const int node = current_device_span->GetNode();
      const int device = current_device_span->GetDevice();
      auto to_host = to_host_channels_.GetValidChannel(node, device);

      spdlog::info("NetworkSpanner::HandelNetworkEvent: try push kToAdapter context event={}", *event_ptr);
      co_await PushPropagateContext<expectation::kRx>(resume_executor, to_host, current_device_span);
      spdlog::info("NetworkSpanner::HandelNetworkEvent: did push kToAdapter context {}", *event_ptr);
    }

    assert(current_device_span->IsComplete());
    last_finished_device_span_ = current_device_span;
    co_await tracer_.MarkSpanAsDone(resume_executor, current_device_span);
    co_return true;
  }

  // this can happen due to the interestingness (ARP) issues...
  if (not IsType(network_event, EventType::kNetworkEnqueueT)) {
    spdlog::debug("NetworkSpanner::HandelNetworkEvent filtered NOT interesting event type {}",
                  *network_event);
    co_return false;
  }

  // Handling events caused by messages started by not interesting devices (ARP)
  //       ---> in case a span is not interesting but ends up in an interesting device attached to am actual simulator,
  //            start a new trace...
  if (not network_event->InterestingFlag()) {
    if (node_device_filter_.IsInterestingNodeDevice(network_event)
        and IsDeviceType(network_event, NetworkEvent::NetworkDeviceType::kCosimNetDevice)) {

      //std::cout << "NetworkSpanner::HandelNetworkEvent: started new trace by network event, arp?!" << std::endl;
      auto current_device_span = co_await tracer_.StartSpan<NetDeviceSpan>(resume_executor, network_event,
                                                                           network_event->GetParserIdent(),
                                                                           name_);
      throw_if_empty(current_device_span, TraceException::kSpanIsNull, source_loc::current());
      current_active_device_spans_.push_back(current_device_span);
      co_return true;
    }
    spdlog::debug(
        "NetworkSpanner::HandelNetworkEvent filtered non interesting potentially starting trace event because of node device: {}",
        *network_event);
    co_return true;
  }

  std::shared_ptr<Context> context_to_connect_with;
  if (IsBoundaryType(network_event, NetworkEvent::EventBoundaryType::kFromAdapter)) {
    throw_on_false(IsDeviceType(network_event, NetworkEvent::NetworkDeviceType::kCosimNetDevice),
                   "trying to create a span depending on a nic side event based on a non cosim device",
                   source_loc::current());

    // is it a fromAdapter event, we need to poll from the host queue to get the parent
    spdlog::info("NetworkSpanner::HandelNetworkEvent: try pop kFromAdapter context {}", *event_ptr);
    const int node = network_event->GetNode();
    const int device = network_event->GetDevice();
    auto from_host_chan = from_host_channels_.GetValidChannel(node, device);
    context_to_connect_with = co_await PopPropagateContext(resume_executor, from_host_chan);
    spdlog::info("NetworkSpanner::HandelNetworkEvent: succesfull pop kFromAdapter context");

    throw_on(not is_expectation(context_to_connect_with, expectation::kRx),
             "received non kRx context", source_loc::current());
  } else {
    assert(last_finished_device_span_);
    context_to_connect_with = Context::CreatePassOnContext<expectation::kRx>(last_finished_device_span_);
  }
  assert(context_to_connect_with);

  auto cur_device_span =
      co_await tracer_.StartSpanByParentPassOnContext<NetDeviceSpan>(resume_executor, context_to_connect_with,
                                                                     network_event,
                                                                     network_event->GetParserIdent(),
                                                                     name_);
  throw_if_empty(cur_device_span, TraceException::kSpanIsNull, source_loc::current());
  current_active_device_spans_.push_back(cur_device_span);
  co_return true;
}

NetworkSpanner::NetworkSpanner(TraceEnvironment &trace_environment,
                               std::string &&name,
                               Tracer &tra,
                               const NodeDeviceToChannelMap &from_host_channels,
                               const NodeDeviceToChannelMap &to_host_channels,
                               const NodeDeviceFilter &node_device_filter)
    : Spanner(trace_environment, std::move(name), tra),
      from_host_channels_(from_host_channels),
      to_host_channels_(to_host_channels),
      node_device_filter_(node_device_filter) {

  auto handel_net_ev = [this](
      std::shared_ptr<concurrencpp::executor> resume_executor,
      EventT &event_ptr) {
    return HandelNetworkEvent(std::move(resume_executor), event_ptr);
  };

  RegisterHandler(EventType::kNetworkEnqueueT, handel_net_ev);
  RegisterHandler(EventType::kNetworkDequeueT, handel_net_ev);
  RegisterHandler(EventType::kNetworkDropT, handel_net_ev);
}
