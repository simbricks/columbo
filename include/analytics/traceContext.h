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

#ifndef SIMBRICKS_TRACE_TRACE_CONTEXT_H_
#define SIMBRICKS_TRACE_TRACE_CONTEXT_H_

#include <memory>
#include "env/traceEnvironment.h"

class TraceContext {
  uint64_t trace_id_;
  uint64_t id_;
  // if parent_id_==0 a.k.a has_parent==false, it is a trace starting span
  uint64_t parent_id_;
  bool has_parent_;
  uint64_t parent_start_ts_;

  std::mutex trace_context_mutex_;

 public:
  explicit TraceContext(uint64_t trace_id,
                        uint64_t trace_context_id)
      : trace_id_(trace_id),
        id_(trace_context_id),
        parent_id_(TraceEnvironment::GetDefaultId()),
        has_parent_(false),
        parent_start_ts_(UINT64_MAX) {
  }

  explicit TraceContext(uint64_t trace_id,
                        uint64_t trace_context_id,
                        uint64_t parent_id,
                        uint64_t parent_start_ts)
      : trace_id_(trace_id),
        id_(trace_context_id),
        parent_id_(parent_id),
        has_parent_(true),
        parent_start_ts_(parent_start_ts) {
    throw_on_false(TraceEnvironment::IsValidId(parent_id),
                   "invalid parent id", source_loc::current());
  }

  TraceContext(const TraceContext &other) {
    trace_id_ = other.trace_id_;
    id_ = other.id_;
    parent_id_ = other.parent_id_;
    has_parent_ = other.has_parent_;
  }

  bool HasParent() {
    const std::lock_guard<std::mutex> guard(trace_context_mutex_);
    assert((has_parent_ and parent_id_ != 0) or (not has_parent_ and parent_id_ == 0));
    return has_parent_;
  }

  uint64_t GetParentId() {
    const std::lock_guard<std::mutex> guard(trace_context_mutex_);
    return parent_id_;
  }

  uint64_t GetParentStartingTs() {
    const std::lock_guard<std::mutex> guard(trace_context_mutex_);
    return parent_start_ts_;
  }

  uint64_t GetTraceId() {
    const std::lock_guard<std::mutex> guard(trace_context_mutex_);
    return trace_id_;
  }

  uint64_t GetId() {
    const std::lock_guard<std::mutex> guard(trace_context_mutex_);
    return id_;
  }

  void SetTraceId(uint64_t new_id) {
    const std::lock_guard<std::mutex> guard(trace_context_mutex_);
    trace_id_ = new_id;
  }

  void SetParentIdAndTs(uint64_t parent_id, uint64_t parent_start_ts) {
    throw_on_false(TraceEnvironment::IsValidId(parent_id),
                   "invalid parent id", source_loc::current());
    const std::lock_guard<std::mutex> guard(trace_context_mutex_);
    parent_id_ = parent_id;
    has_parent_ = true;
    parent_start_ts_ = parent_start_ts;
  }

};

inline std::shared_ptr<TraceContext> clone_shared(const std::shared_ptr<TraceContext> &other) {
  throw_if_empty(other, TraceException::kContextIsNull, source_loc::current());
  auto new_con = create_shared<TraceContext>(TraceException::kContextIsNull, *other);
  return new_con;
}

#endif // SIMBRICKS_TRACE_TRACE_CONTEXT_H_
