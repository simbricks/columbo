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

#ifndef SIMBRICKS_TRACE_TRACER_H_
#define SIMBRICKS_TRACE_TRACER_H_

#include <list>
#include <memory>
#include <unordered_map>
#include <set>
#include <utility>

#include "sync/corobelt.h"
#include "sync/channel.h"
#include "events/events.h"
#include "analytics/span.h"
#include "analytics/trace.h"
#include "env/traceEnvironment.h"
#include "exporter/exporter.h"
#include "util/factory.h"
#include "analytics/context.h"
#include "util/exception.h"

class Tracer {
  // std::mutex tracer_mutex_;
  concurrencpp::async_lock async_lock_;

  TraceEnvironment &trace_environment_;

  // trace_id -> trace
  std::unordered_map<uint64_t, std::shared_ptr<Trace>> traces_;

  // span ids that were already exported
  std::set<uint64_t> exported_spans_;
  // parent_span_id -> list/vector of spans that wait for the parent to be exported
  std::unordered_map<uint64_t, std::vector<std::shared_ptr<EventSpan>>> waiting_list_;

  std::shared_ptr<simbricks::trace::SpanExporter> exporter_;

  void InsertExportedSpan(uint64_t span_id) {
    throw_on_false(TraceEnvironment::IsValidId(span_id),
                   TraceException::kInvalidId, source_loc::current());
    auto iter = exported_spans_.insert(span_id);
    throw_on_false(iter.second, "could not insert non exported span into set",
                   source_loc::current());
  }

  void InsertTrace(std::shared_ptr<Trace> &new_trace) {
    // NOTE: the lock must be held when calling this method
    assert(new_trace);
    auto iter = traces_.insert({new_trace->GetId(), new_trace});
    throw_on(not iter.second, "could not insert trace into traces map",
             source_loc::current());
  }

  std::shared_ptr<Trace> GetTrace(uint64_t trace_id) {
    // NOTE: the lock must be held when calling this method
    auto iter = traces_.find(trace_id);
    if (iter == traces_.end()) {
      return {};
    }
    std::shared_ptr<Trace> target_trace = iter->second;
    return target_trace;
  }

  void RemoveTrace(uint64_t trace_id) {
    // NOTE: lock must be held when calling this method
    const size_t amount_removed = traces_.erase(trace_id);
    throw_on(amount_removed == 0, "RemoveTrace: nothing was removed",
             source_loc::current());
  }

  void AddSpanToTraceIfTraceExists(uint64_t trace_id, const std::shared_ptr<EventSpan> &span_ptr) {
    // NOTE: lock must be held when calling this method
    assert(span_ptr);
    auto target_trace = GetTrace(trace_id);
    if (nullptr == target_trace) {
      return;
    }
    assert(target_trace);
    target_trace->AddSpan(span_ptr);
  }

  bool SafeToDeleteTrace(uint64_t trace_id) {
    // NOTE: lock must be held when calling this method
    auto trace = GetTrace(trace_id);
    if (nullptr == trace) {
      return false;
    }
    auto span_ids = trace->GetSpanIds();
    const bool all_exported = std::ranges::all_of(span_ids, [&](uint64_t span_id) -> bool {
      return exported_spans_.contains(span_id);
    });
    return all_exported;
  }

  std::shared_ptr<TraceContext>
  RegisterCreateContextParent(uint64_t trace_id, uint64_t parent_id, uint64_t parent_starting_ts) {
    // NOTE: lock must be held when calling this method
    throw_on_false(TraceEnvironment::IsValidId(trace_id),
                   TraceException::kInvalidId, source_loc::current());
    throw_on_false(TraceEnvironment::IsValidId(parent_id),
                   TraceException::kInvalidId, source_loc::current());
    const uint64_t context_id = trace_environment_.GetNextTraceContextId();
    auto trace_context = create_shared<TraceContext>(
        "RegisterCreateContext couldnt create context",
        trace_id, context_id, parent_id, parent_starting_ts);
    return trace_context;
  }

  std::shared_ptr<TraceContext>
  RegisterCreateContext(uint64_t trace_id) {
    // NOTE: lock must be held when calling this method
    const uint64_t context_id = trace_environment_.GetNextTraceContextId();
    auto trace_context = create_shared<TraceContext>(
        "RegisterCreateContext couldnt create context",
        trace_id, context_id);
    return trace_context;
  }

  bool WasParentExported(const std::shared_ptr<EventSpan> &child) {
    // NOTE: lock must be held when calling this method
    assert(child and "span is null");
    if (not child->HasParent()) {
      // no parent means trace starting span
      return true;
    }
    const uint64_t parent_id = child->GetValidParentId();
    return exported_spans_.contains(parent_id);
  }

  void MarkSpanAsExported(std::shared_ptr<EventSpan> &span) {
    // NOTE: lock must be held when calling this method
    assert(span and "span is null");
    const uint64_t ident = span->GetId();
    InsertExportedSpan(ident);
  }

  void MarkSpanAsWaitingForParent(std::shared_ptr<EventSpan> &span) {
    // NOTE: lock must be held when calling this method
    assert(span);
    if (not span->HasParent()) {
      // we cannot wait for a non-existing parent...
      return;
    }
    const uint64_t parent_id = span->GetValidParentId();
    auto iter = waiting_list_.find(parent_id);
    if (iter != waiting_list_.end()) {
      auto &wait_vec = iter->second;
      wait_vec.push_back(span);
    } else {
      auto res_p = waiting_list_.insert({parent_id, {span}});
      throw_on(not res_p.second, "MarkSpanAsWaitingForParent: could not insert value",
               source_loc::current());
    }
  }

  concurrencpp::result<void>
  ExportWaitingForParentVec(std::shared_ptr<concurrencpp::executor> executor,
                            std::shared_ptr<EventSpan> parent) {
    throw_if_empty(executor, TraceException::kResumeExecutorNull, source_loc::current());

    // NOTE: lock MUST be held when calling this method

    assert(parent and "span is null");
    const uint64_t parent_id = parent->GetId();
    auto iter = waiting_list_.find(parent_id);
    if (iter == waiting_list_.end()) {
      co_return;
    }
    auto waiters = iter->second;
    for (auto &waiter : waiters) {
      throw_on_false(exported_spans_.contains(waiter->GetValidParentId()),
                     "try to export span whos parent was not exported yet",
                     source_loc::current());
      MarkSpanAsExported(waiter);
      assert(not waiter->HasParent() or exported_spans_.contains(waiter->GetParentId()));

      exporter_->ExportSpan(waiter);
      ExportWaitingForParentVec(executor, waiter);

    }
    waiting_list_.erase(parent_id);
  }

  template<class SpanType, class... Args>
  std::shared_ptr<SpanType>
  StartSpanByParentInternal(uint64_t trace_id,
                            uint64_t parent_id,
                            uint64_t parent_starting_ts,
                            std::shared_ptr<Event> starting_event,
                            Args &&... args) {
    // NOTE: lock must be held when calling this method
    assert(starting_event);
    throw_on_false(TraceEnvironment::IsValidId(trace_id), "invalid id",
                   source_loc::current());
    throw_on_false(TraceEnvironment::IsValidId(parent_id), "invalid id",
                   source_loc::current());

    auto trace_context = RegisterCreateContextParent(trace_id, parent_id, parent_starting_ts);

    auto new_span = create_shared<SpanType>(
        "StartSpanByParentInternal(...) could not create a new span",
        trace_environment_, trace_context, args...);
    const bool was_added = new_span->AddToSpan(starting_event);
    throw_on(not was_added, "StartSpanByParentInternal(...) could not add first event",
             source_loc::current());

    // must add span to trace manually
    AddSpanToTraceIfTraceExists(trace_id, new_span);

    return new_span;
  }

  template<class SpanType, class... Args>
  std::shared_ptr<SpanType>
  StartSpanInternal(std::shared_ptr<Event> starting_event, Args &&... args) {
    // NOTE: lock must be held when calling this method
    assert(starting_event);

    uint64_t trace_id = trace_environment_.GetNextTraceId();
    auto trace_context = RegisterCreateContext(trace_id);

    auto new_span = create_shared<SpanType>(
        "StartSpanInternal(...) could not create a new span",
        trace_environment_, trace_context, args...);
    const bool was_added = new_span->AddToSpan(starting_event);
    throw_on(not was_added, "StartSpanInternal(...) could not add first event",
             source_loc::current());

    auto new_trace = create_shared<Trace>(
        "StartSpanInternal(...) could not create a new trace", trace_id, new_span);
    InsertTrace(new_trace);

    return new_span;
  }

 public:
  concurrencpp::result<void>
  MarkSpanAsDone(std::shared_ptr<concurrencpp::executor> executor, std::shared_ptr<EventSpan> span) {
    throw_if_empty(executor, TraceException::kResumeExecutorNull, source_loc::current());

    // guard potential access using a lock guard
    concurrencpp::scoped_async_lock guard = co_await async_lock_.lock(executor);

    throw_if_empty(span, "MarkSpanAsDone, span is null", source_loc::current());
    auto context = span->GetContext();
    throw_if_empty(context, "MarkSpanAsDone context is null", source_loc::current());
    const uint64_t trace_id = context->GetTraceId();

    if (WasParentExported(span)) {
      MarkSpanAsExported(span);
      assert(not span->HasParent() or exported_spans_.contains(span->GetParentId()));

      exporter_->ExportSpan(span);
      co_await ExportWaitingForParentVec(executor, span);

      auto trace = GetTrace(trace_id);
      if (nullptr != trace and SafeToDeleteTrace(trace_id)) {
        RemoveTrace(trace_id);
      }

      co_return;
    }

    assert(span->HasParent());
    MarkSpanAsWaitingForParent(span);
  }

  concurrencpp::result<void>
  AddParentLazily(std::shared_ptr<concurrencpp::executor> executor,
                  const std::shared_ptr<EventSpan> &span,
                  const std::shared_ptr<Context> &parent_context) {
    throw_if_empty(executor, TraceException::kResumeExecutorNull, source_loc::current());
    throw_if_empty(span, TraceException::kSpanIsNull, source_loc::current());
    throw_if_empty(parent_context, TraceException::kContextIsNull, source_loc::current());

    // guard potential access using a lock guard
    concurrencpp::scoped_async_lock guard = co_await async_lock_.lock(executor);

    auto new_trace_id = parent_context->GetTraceId();
    auto old_context = span->GetContext();
    throw_if_empty(old_context, TraceException::kContextIsNull, source_loc::current());

    // NOTE: as this case shall only happen when this span is the trace root, we expect the old trace
    //       definitely to exist
    auto old_trace = GetTrace(old_context->GetTraceId());
    throw_if_empty(old_trace, TraceException::kTraceIsNull, source_loc::current());

    old_context->SetTraceId(new_trace_id);
    const uint64_t parent_id = parent_context->GetParentId();
    const uint64_t parent_start_ts = parent_context->GetParentStartingTs();
    old_context->SetParentIdAndTs(parent_id, parent_start_ts);
    AddSpanToTraceIfTraceExists(new_trace_id, span);

    auto children = old_trace->GetSpansAndRemoveSpans();
    for (const auto &iter : children) {
      if (iter->GetId() == span->GetId()) {
        continue;
      }
      iter->GetContext()->SetTraceId(new_trace_id);
      AddSpanToTraceIfTraceExists(new_trace_id, iter);
    }

    RemoveTrace(old_trace->GetId());
  }

  // will create and add a new span to a trace using the context
  template<class SpanType, class... Args>
  concurrencpp::result<std::shared_ptr<SpanType>>
  StartSpanByParent(
      std::shared_ptr<concurrencpp::executor> executor,
      std::shared_ptr<EventSpan> parent_span,
      std::shared_ptr<Event> starting_event,
      Args &&... args) {
    throw_if_empty(executor, TraceException::kResumeExecutorNull, source_loc::current());
    throw_if_empty(starting_event, TraceException::kEventIsNull, source_loc::current());
    throw_if_empty(parent_span, TraceException::kSpanIsNull, source_loc::current());

    const uint64_t trace_id = parent_span->GetValidTraceId();
    const uint64_t parent_id = parent_span->GetValidId();
    const uint64_t parent_starting_ts = parent_span->GetStartingTs();

    // guard potential access using a lock guard
    concurrencpp::scoped_async_lock guard = co_await async_lock_.lock(executor);

    std::shared_ptr<SpanType> new_span;
    new_span = StartSpanByParentInternal<SpanType, Args...>(
        trace_id, parent_id, parent_starting_ts, starting_event, std::forward<Args>(args)...);

    assert(new_span);
    co_return new_span;
  }

  // will create and add a new span to a trace using the context
  template<class SpanType, class... Args>
  concurrencpp::result<std::shared_ptr<SpanType>>
  StartSpanByParentPassOnContext(std::shared_ptr<concurrencpp::executor> executor,
                                 const std::shared_ptr<Context> &parent_context,
                                 std::shared_ptr<Event> starting_event,
                                 Args &&... args) {
    throw_if_empty(executor, TraceException::kResumeExecutorNull, source_loc::current());
    throw_if_empty(parent_context, TraceException::kContextIsNull, source_loc::current());
    throw_if_empty(starting_event, TraceException::kEventIsNull, source_loc::current());
    throw_on_false(parent_context->HasParent(), "Context has no parent",
                   source_loc::current());

    // guard potential access using a lock guard
    concurrencpp::scoped_async_lock guard = co_await async_lock_.lock(executor);

    const uint64_t trace_id = parent_context->GetTraceId();
    const uint64_t parent_id = parent_context->GetParentId();
    const uint64_t parent_starting_ts = parent_context->GetParentStartingTs();
    std::shared_ptr<SpanType> new_span;
    new_span = StartSpanByParentInternal<SpanType, Args...>(
        trace_id, parent_id, parent_starting_ts, starting_event, std::forward<Args>(args)...);

    assert(new_span);
    co_return new_span;
  }

  // will start and create a new trace creating a new context
  template<class SpanType, class... Args>
  concurrencpp::result<std::shared_ptr<SpanType>>
  StartSpan(std::shared_ptr<concurrencpp::executor> executor,
            std::shared_ptr<Event> starting_event,
            Args &&... args) {
    throw_if_empty(executor, TraceException::kResumeExecutorNull, source_loc::current());

    // guard potential access using a lock guard
    concurrencpp::scoped_async_lock guard = co_await async_lock_.lock(executor);

    throw_if_empty(starting_event, "StartSpan(...) starting_event is null",
                   source_loc::current());
    std::shared_ptr<SpanType> new_span = nullptr;
    new_span = StartSpanInternal<SpanType, Args...>(starting_event, std::forward<Args>(args)...);
    assert(new_span);
    co_return new_span;
  }

  template<class ContextType>
  requires ContextInterface<ContextType>
  concurrencpp::result<void>
  StartSpanSetParentContext(std::shared_ptr<concurrencpp::executor> executor,
                            std::shared_ptr<EventSpan> &span_to_register,
                            const std::shared_ptr<ContextType> &parent_context) {
    throw_if_empty(executor, TraceException::kResumeExecutorNull, source_loc::current());
    throw_if_empty(parent_context, TraceException::kContextIsNull, source_loc::current());
    // guard potential access using a lock guard
    concurrencpp::scoped_async_lock guard = co_await async_lock_.lock(executor);

    const uint64_t trace_id = parent_context->GetTraceId();
    const uint64_t parent_id = parent_context->GetParentId();
    const uint64_t parent_starting_ts = parent_context->GetParentStartingTs();
    auto trace_context = RegisterCreateContextParent(trace_id, parent_id, parent_starting_ts);
    const bool could_set = span_to_register->SetContext(trace_context, true);
    throw_on_false(could_set, "StartSpanSetParentContext could not set context",
                   source_loc::current());

    // must add span to trace manually
    AddSpanToTraceIfTraceExists(trace_id, span_to_register);
  }

  // will create a new span not belonging to any trace, must be made manually by using one of the methods above
  template<class SpanType, class... Args>
  std::shared_ptr<SpanType> StartOrphanSpan(std::shared_ptr<Event> starting_event, Args &&... args) {
    // NOTE: As we create an orphan, we do not make any bookkeeping, as a result we
    //       don't need to take ownership of the lock!!!!!!
    throw_if_empty(starting_event, "StartOrphanSpan(...) starting_event is null",
                   source_loc::current());

    auto invalid_trace_context = create_shared<TraceContext>(
        "StartOrphanSpan couldnt create context",
        TraceEnvironment::kInvalidId, TraceEnvironment::kInvalidId);

    std::shared_ptr<SpanType> new_span = create_shared<SpanType>(
        "StartOrphanSpan(...) could not create a new span",
        trace_environment_, invalid_trace_context, std::forward<Args>(args)...);
    const bool was_added = new_span->AddToSpan(starting_event);
    throw_on(not was_added, "StartOrphanSpan(...) could not add first event",
             source_loc::current());

    // NOTE: span is not added to any trace!!!
    assert(new_span);
    return new_span;
  }

  explicit Tracer(TraceEnvironment &trace_environment,
                  std::shared_ptr<simbricks::trace::SpanExporter> exporter)
      : trace_environment_(trace_environment), exporter_(std::move(exporter)) {

    throw_if_empty(exporter_, TraceException::kSpanExporterNull, source_loc::current());
  };

  void FinishExport() {
    exporter_->ForceFlush();
  }

  ~Tracer() {
    FinishExport();
  };
};

#endif  // SIMBRICKS_TRACE_TRACER_H_
