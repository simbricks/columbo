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

#include <functional>
#include <iostream>
#include <memory>
#include <string>

#include "parser/parser.h"
#include "util/exception.h"
#include "sync/corobelt.h"
#include "util/string_util.h"
#include "env/traceEnvironment.h"

#ifndef SIMBRICKS_TRACE_EVENT_STREAM_PARSER_H_
#define SIMBRICKS_TRACE_EVENT_STREAM_PARSER_H_

class EventStreamParser : public LogParser {

  static bool ParseIdentNameTs(LineHandler &line_handler, size_t &parser_ident,
                               std::string &parser_name, uint64_t &ts) {
    if (not line_handler.ConsumeAndTrimString(": source_id=") or
        not line_handler.ParseUintTrim(10, parser_ident)) {
      return false;
    }

    if (!line_handler.ConsumeAndTrimString(", source_name=")) {
      return false;
    }

    std::function<bool(unsigned char)> kSourceNamePred = [](unsigned char chara) {
      return sim_string_utils::is_alnum(chara) or chara == '-';
    };
    parser_name = line_handler.ExtractAndSubstrUntil(kSourceNamePred);
    if (parser_name.empty()) {
      return false;
    }

    if (!line_handler.ConsumeAndTrimString(", timestamp=")) {
      return false;
    }
    return line_handler.ParseUintTrim(10, ts);
  }

  std::shared_ptr<NetworkEvent> ParseNetworkEvent(LineHandler &line_handler,
                                                  EventType event_type,
                                                  uint64_t timestamp,
                                                  size_t parser_ident,
                                                  const std::string &parser_name);

 public:
  explicit EventStreamParser(TraceEnvironment &trace_environment, std::string name)
      : LogParser(trace_environment, name) {}

  concurrencpp::result<std::shared_ptr<Event>>
  ParseEvent(LineHandler &line_handler) override;

};

#endif  // SIMBRICKS_TRACE_EVENT_STREAM_PARSER_H_
