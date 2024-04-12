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

#include <list>
#include <memory>
#include <utility>

#include "util/exception.h"
#include "env/traceEnvironment.h"
#include "sync/corobelt.h"
#include "events/events.h"
#include "parser/parser.h"
#include "analytics/context.h"
#include "analytics/span.h"
#include "analytics/tracer.h"
#include "analytics/timer.h"
#include "analytics/helper.h"

#ifndef SIMBRICKS_TRACE_SPANNER_H_
#define SIMBRICKS_TRACE_SPANNER_H_

struct Spanner : public Consumer<std::shared_ptr<Event>> {
  TraceEnvironment &trace_environment_;
  uint64_t id_;
  std::string name_;
  Tracer &tracer_;

  using ExecutorT = std::shared_ptr<concurrencpp::executor>;
  using EventT = std::shared_ptr<Event>;
  using ChannelT = std::shared_ptr<CoroChannel<std::shared_ptr<Context>>>;
  using ResultT = concurrencpp::result<bool>;
  using HandlerT = std::function<ResultT(ExecutorT, EventT &)>;
  std::unordered_map<EventType, HandlerT> handler_;

  concurrencpp::result<void> consume(std::shared_ptr<concurrencpp::executor> executor,
                                     std::shared_ptr<Event> value) override;

  explicit Spanner(TraceEnvironment &trace_environment,
                   std::string &&name,
                   Tracer &tra)
      : trace_environment_(trace_environment),
        id_(trace_environment_.GetNextSpannerId()),
        name_(name),
        tracer_(tra) {
  }

  explicit Spanner(TraceEnvironment &trace_environment,
                   std::string &&name,
                   Tracer &tra,
                   std::unordered_map<EventType, HandlerT> &&handler)
      : trace_environment_(trace_environment),
        id_(trace_environment_.GetNextSpannerId()),
        name_(name),
        tracer_(tra),
        handler_(handler) {
  }

  void RegisterHandler(EventType type, HandlerT &&handler) {
    auto it_suc = handler_.insert({type, handler});
    throw_on(not it_suc.second,
             "Spanner::RegisterHandler Could not insert new handler",
             source_loc::current());
  }

  inline uint64_t GetId() const {
    return id_;
  }

 protected:
  template<class St>
  std::shared_ptr<St>
  iterate_add_erase(std::list<std::shared_ptr<St>> &pending,
                    std::shared_ptr<Event> event_ptr) {
    std::shared_ptr<St> pending_span = nullptr;

    for (auto it = pending.begin(); it != pending.end(); it++) {
      pending_span = *it;
      if (pending_span and pending_span->AddToSpan(event_ptr)) {
        if (pending_span->IsComplete()) {
          pending.erase(it);
        }
        return pending_span;
      }
    }

    return nullptr;
  }

  concurrencpp::result<std::shared_ptr<Context>> PopPropagateContext(
      std::shared_ptr<concurrencpp::executor> resume_executor,
      std::shared_ptr<CoroChannel<std::shared_ptr<Context>>> from) const {
    auto con_opt = co_await from->Pop(resume_executor);
    const auto con = OrElseThrow(con_opt, TraceException::kContextIsNull, source_loc::current());
    throw_if_empty(con, TraceException::kContextIsNull, source_loc::current());
//    from->PokeAwaiters();
    co_return con;
  }

  template<expectation exp>
  concurrencpp::result<void> PushPropagateContext(
      std::shared_ptr<concurrencpp::executor> resume_executor,
      std::shared_ptr<CoroChannel<std::shared_ptr<Context>>> to,
      const std::shared_ptr<EventSpan> &parent) const {
    std::shared_ptr<Context> context = Context::CreatePassOnContext<exp>(parent);
    throw_if_empty(context, TraceException::kContextIsNull, source_loc::current());
    const bool could_push = co_await to->Push(resume_executor, context);
    throw_on_false(could_push,
                   TraceException::kCouldNotPushToContextQueue,
                   source_loc::current());
//    to->PokeAwaiters();
    co_return;
  }
};

struct HostSpanner : public Spanner {

  explicit HostSpanner(TraceEnvironment &trace_environment,
                       std::string &&name,
                       Tracer &tra,
                       std::shared_ptr<CoroChannel<std::shared_ptr<Context>>> to_nic,
                       std::shared_ptr<CoroChannel<std::shared_ptr<Context>>> from_nic,
                       std::shared_ptr<CoroChannel<std::shared_ptr<Context>>> from_nic_receives);

 private:
  concurrencpp::result<void> FinishPendingSpan(std::shared_ptr<concurrencpp::executor> resume_executor);

  concurrencpp::result<bool> CreateTraceStartingSpan(std::shared_ptr<concurrencpp::executor> resume_executor,
                                                     std::shared_ptr<Event> &starting_event,
                                                     bool fragmented);

  concurrencpp::result<bool> HandelCall(std::shared_ptr<concurrencpp::executor> resume_executor,
                                        std::shared_ptr<Event> &event_ptr);

  // NOTE: these two functions will push mmio expectations to the NIC site!!!
  concurrencpp::result<bool> HandelMmio(std::shared_ptr<concurrencpp::executor> resume_executor,
                                        std::shared_ptr<Event> &event_ptr);
  concurrencpp::result<bool> HandelPci(std::shared_ptr<concurrencpp::executor> resume_executor,
                                       std::shared_ptr<Event> &event_ptr);

  concurrencpp::result<bool> HandelDma(std::shared_ptr<concurrencpp::executor> resume_executor,
                                       std::shared_ptr<Event> &event_ptr);

  concurrencpp::result<bool> HandelMsix(std::shared_ptr<concurrencpp::executor> resume_executor,
                                        std::shared_ptr<Event> &event_ptr);

  concurrencpp::result<bool> HandelInt(std::shared_ptr<concurrencpp::executor> resume_executor,
                                       std::shared_ptr<Event> &event_ptr);

  std::shared_ptr<CoroChannel<std::shared_ptr<Context>>> from_nic_queue_;
  std::shared_ptr<CoroChannel<std::shared_ptr<Context>>> from_nic_receives_queue_;
  std::shared_ptr<CoroChannel<std::shared_ptr<Context>>> to_nic_queue_;

  bool pci_write_before_ = false;
  std::shared_ptr<HostCallSpan> last_trace_starting_span_ = nullptr;
  std::shared_ptr<HostCallSpan> pending_host_call_span_ = nullptr;
  std::shared_ptr<HostIntSpan> pending_host_int_span_ = nullptr;
  std::shared_ptr<HostMsixSpan> pending_host_msix_span_ = nullptr;
  std::list<std::shared_ptr<HostDmaSpan>> pending_host_dma_spans_;
  std::list<std::shared_ptr<HostMmioSpan>> pending_host_mmio_spans_;
  std::shared_ptr<HostPciSpan> pending_pci_span_;
};

struct NicSpanner : public Spanner {

  explicit NicSpanner(TraceEnvironment &trace_environment,
                      std::string &&name,
                      Tracer &tra,
                      std::shared_ptr<CoroChannel<std::shared_ptr<Context>>> to_network,
                      std::shared_ptr<CoroChannel<std::shared_ptr<Context>>> from_network,
                      std::shared_ptr<CoroChannel<std::shared_ptr<Context>>> to_host,
                      std::shared_ptr<CoroChannel<std::shared_ptr<Context>>> from_host,
                      std::shared_ptr<CoroChannel<std::shared_ptr<Context>>> to_host_receives);

 private:

  concurrencpp::result<bool> HandelMmio(std::shared_ptr<concurrencpp::executor> resume_executor,
                                        std::shared_ptr<Event> &event_ptr);

  concurrencpp::result<bool> HandelDma(std::shared_ptr<concurrencpp::executor> resume_executor,
                                       std::shared_ptr<Event> &event_ptr);

  concurrencpp::result<bool> HandelTxrx(std::shared_ptr<concurrencpp::executor> resume_executor,
                                        std::shared_ptr<Event> &event_ptr);

  concurrencpp::result<bool> HandelMsix(std::shared_ptr<concurrencpp::executor> resume_executor,
                                        std::shared_ptr<Event> &event_ptr);

  std::shared_ptr<CoroChannel<std::shared_ptr<Context>>> to_network_queue_;
  std::shared_ptr<CoroChannel<std::shared_ptr<Context>>> from_network_queue_;
  std::shared_ptr<CoroChannel<std::shared_ptr<Context>>> to_host_queue_;
  std::shared_ptr<CoroChannel<std::shared_ptr<Context>>> from_host_queue_;
  std::shared_ptr<CoroChannel<std::shared_ptr<Context>>> to_host_receives_;

  std::shared_ptr<Context> last_host_context_ = nullptr;
  std::shared_ptr<EventSpan> last_causing_ = nullptr;

  std::list<std::shared_ptr<NicDmaSpan>> pending_nic_dma_spans_;
};

struct NetworkSpanner : public Spanner {

  explicit NetworkSpanner(TraceEnvironment &trace_environment,
                          std::string &&name,
                          Tracer &tra,
                          const NodeDeviceToChannelMap &from_host_channels,
                          const NodeDeviceToChannelMap &to_host_channels,
                          const NodeDeviceFilter &node_device_filter);

 private:

  concurrencpp::result<bool> HandelNetworkEvent(ExecutorT resume_executor,
                                                std::shared_ptr<Event> &event_ptr);

  // TODO: may need this to be a vector as well
  std::shared_ptr<NetDeviceSpan> last_finished_device_span_ = nullptr;
  std::list<std::shared_ptr<NetDeviceSpan>> current_active_device_spans_;
  //std::shared_ptr<NetDeviceSpan> current_device_span_ = nullptr;

  const NodeDeviceToChannelMap &from_host_channels_;
  const NodeDeviceToChannelMap &to_host_channels_;
  const NodeDeviceFilter &node_device_filter_;
};

#endif  // SIMBRICKS_TRACE_SPANNER_H_
