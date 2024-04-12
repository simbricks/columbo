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
#include <memory>

#include "util/exception.h"
#include "util/factory.h"
#include "env/traceEnvironment.h"
#include "analytics/span.h"

#ifndef SIMBRICKS_TRACE_CONTEXT_QUEUE_H_
#define SIMBRICKS_TRACE_CONTEXT_QUEUE_H_

enum expectation { kTx, kRx, kDma, kMsix, kMmio };

inline std::ostream &operator<<(std::ostream &out, expectation exp) {
  switch (exp) {
    case expectation::kTx:out << "expectation::tx";
      break;
    case expectation::kRx:out << "expectation::rx";
      break;
    case expectation::kDma:out << "expectation::dma";
      break;
    case expectation::kMsix:out << "expectation::msix";
      break;
    case expectation::kMmio:out << "expectation::mmio";
      break;
    default:out << "could not convert given 'expectation'";
      break;
  }
  return out;
}

class Context {
  const uint64_t trace_id_;
  const expectation expectation_;
  const uint64_t parent_id_;
  const bool has_parent_ = true;
  const uint64_t parent_start_ts_;

 public:
  explicit Context(uint64_t trace_id, expectation expectation, uint64_t parent_id, uint64_t parent_start_ts)
      : trace_id_(trace_id), expectation_(expectation), parent_id_(parent_id), parent_start_ts_(parent_start_ts) {
    throw_on_false(TraceEnvironment::IsValidId(trace_id),
                   TraceException::kInvalidId, source_loc::current());
    throw_on_false(TraceEnvironment::IsValidId(parent_id),
                   TraceException::kInvalidId, source_loc::current());
  }

  inline bool HasParent() const {
    return has_parent_;
  }

  inline expectation GetExpectation() const {
    return expectation_;
  }

  inline uint64_t GetParentStartingTs() const {
    return parent_start_ts_;
  }

  inline uint64_t GetTraceId() const {
    return trace_id_;
  }

  inline uint64_t GetParentId() const {
    return parent_id_;
  }

  void Display(std::ostream &out) {
    out << "Context: " << "expectation=" << expectation_;
    out << ", has_parent=" << BoolToString(has_parent_);
    out << ", parent_starting_ts=" << parent_start_ts_;
    out << ", parent_span={" << '\n';
    out << "parent_id=" << parent_id_ << '\n';
    out << "}";
  }

  template<expectation Exp>
  static std::shared_ptr<Context> CreatePassOnContext(const std::shared_ptr<EventSpan> &parent_span) {
    throw_if_empty(parent_span, TraceException::kSpanIsNull, source_loc::current());
    auto context = create_shared<Context>(TraceException::kContextIsNull,
                                          parent_span->GetValidTraceId(),
                                          Exp,
                                          parent_span->GetValidId(),
                                          parent_span->GetStartingTs());
    assert(context);
    return context;
  }

};

inline bool is_expectation(const std::shared_ptr<Context> &con, expectation exp) {
  return con && con->GetExpectation() == exp;
}

inline std::ostream &operator<<(std::ostream &out, Context &con) {
  con.Display(out);
  return out;
}

inline std::ostream &PrintContextPtr(std::ostream &out, std::shared_ptr<Context> &context) {
  out << *context << '\n';
  return out;
}

#endif  // SIMBRICKS_TRACE_CONTEXT_QUEUE_H_
