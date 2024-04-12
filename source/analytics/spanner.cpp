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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHos WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, os OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include "analytics/spanner.h"
#include "util/exception.h"
#include "events/printer.h"

concurrencpp::result<void> Spanner::consume(std::shared_ptr<concurrencpp::executor> executor,
                                            std::shared_ptr<Event> value) {
  throw_if_empty(value, TraceException::kEventIsNull, source_loc::current());

  spdlog::debug("{} try handel: {}", name_, *value);
  auto handler_it = handler_.find(value->GetType());
  if (handler_it == handler_.end()) {
    spdlog::critical("Spanner: could not find handler for the following event: {}", *value);
    co_return;
  }

  auto handler = handler_it->second;
  const bool added = co_await handler(executor, value);
  if (not added) {
    spdlog::debug("found event that could not be added to a pack: {}", *value);
  }

  spdlog::debug("{} handled event {}", name_, *value);
  co_return;
}
