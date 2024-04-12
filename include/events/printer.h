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

#ifndef SIMBRICKS_TRACE_EVENTS_PRINTER_H_
#define SIMBRICKS_TRACE_EVENTS_PRINTER_H_

#include <iostream>

#include "events/events.h"
#include "sync/channel.h"
#include "sync/corobelt.h"

template<typename Etype>
concept SubclassEvent = std::is_base_of<Event, Etype>::value;

template<typename Etype> requires SubclassEvent<Etype>
struct fmt::formatter<Etype> : fmt::formatter<std::string> {
  auto format(Etype &event, format_context &ctx) const -> decltype(ctx.out()) {
    std::stringstream event_string;
    event_string << event;
    return format_to(ctx.out(), "{}", event_string.str());
  }
};

class EventPrinter : public Consumer<std::shared_ptr<Event>>,
                     public Handler<std::shared_ptr<Event>> {
  std::ostream &out_;

  inline void print(const std::shared_ptr<Event> &event) {
    throw_if_empty(event, TraceException::kEventIsNull, source_loc::current());
    out_ << *event << '\n';
    out_.flush();
  }

 public:

  explicit EventPrinter(std::ostream &out) : out_(out) {
  }

  concurrencpp::result<void> consume(std::shared_ptr<concurrencpp::executor> executor,
                                     std::shared_ptr<Event> value) override {
    print(value);
    co_return;
  }

  concurrencpp::result<bool> handel(std::shared_ptr<concurrencpp::executor> executor,
                                    std::shared_ptr<Event> &value) override {
    print(value);
    co_return true;
  };
};

#endif // SIMBRICKS_TRACE_EVENTS_PRINTER_H_
