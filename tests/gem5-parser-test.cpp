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

#include <catch2/catch_all.hpp>
#include <vector>
#include <memory>

#include "util/componenttable.h"
#include "parser/parser.h"
#include "util/factory.h"
#include "events/events.h"
#include "util/exception.h"

TEST_CASE("Test gem5 parser produces expected event stream", "[Gem5Parser]") {

  const std::string test_file_path{"./tests/raw-logs/gem5-events-test.txt"};
  const std::string parser_name{"Gem5ClientParser"};

  const TraceEnvConfig trace_env_config = TraceEnvConfig::CreateFromYaml("tests/trace-env-config.yaml");
  TraceEnvironment trace_environment{trace_env_config};

  ComponentFilter comp_filter_client("ComponentFilter-Server");

  //ReaderBuffer<10> reader_buffer{"test-reader", true};
  ReaderBuffer<10000> reader_buffer{"test-reader"};
  REQUIRE_NOTHROW(reader_buffer.OpenFile(test_file_path));

  auto gem5 = create_shared<Gem5Parser>(TraceException::kParserIsNull,
                                        trace_environment,
                                        parser_name,
                                        comp_filter_client);

  std::shared_ptr<Event> parsed_event;

  const std::vector<std::shared_ptr<Event>> to_match{
      std::make_shared<HostMmioR>(1869691991749,
                                  gem5->GetIdent(), parser_name, 94469181196688, 0xc0080300, 4, 0, 0x80300),
      std::make_shared<HostMmioR>(1869693118999,
                                  gem5->GetIdent(), parser_name, 94469181196688, 0xc0080300, 4, 0, 0x80300),
      std::make_shared<HostMmioR>(1869699347625, gem5->GetIdent(), parser_name, 94469181901728, 0xc040000c, 4, 3, 0xc),
      std::make_shared<HostMmioR>(1869699662249, gem5->GetIdent(), parser_name, 94469181901920, 0xc040001c, 4, 3, 0x1c),
      std::make_shared<HostMmioW>(1967474400999,
                                  gem5->GetIdent(),
                                  parser_name,
                                  94469376954304,
                                  0xc0400010,
                                  4,
                                  0,
                                  0,
                                  true),
      std::make_shared<HostMmioW>(1967474526999,
                                  gem5->GetIdent(),
                                  parser_name,
                                  94469376954304,
                                  0xc0400014,
                                  4,
                                  0,
                                  0,
                                  false),
  };

  std::pair<bool, LineHandler *> bh_p;
  for (const auto &match : to_match) {
    REQUIRE(reader_buffer.HasStillLine());
    REQUIRE_NOTHROW(bh_p = reader_buffer.NextHandler());
    REQUIRE(bh_p.first);
    LineHandler &line_handler = *bh_p.second;
    parsed_event = gem5->ParseEvent(line_handler).get();
    REQUIRE(parsed_event);
    REQUIRE(parsed_event->Equal(*match));
  }

  REQUIRE_FALSE(reader_buffer.HasStillLine());
  REQUIRE_NOTHROW(bh_p = reader_buffer.NextHandler());
  REQUIRE_FALSE(bh_p.first);
}
