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

#include "test-util.h"
#include "util/componenttable.h"
#include "parser/parser.h"
#include "util/factory.h"
#include "events/events.h"
#include "util/exception.h"

TEST_CASE("Test ns3 parser produces expected event stream", "[NS3Parser]") {
  const std::string test_file_path{"tests/raw-logs/ns3-raw-log.txt"};
  const std::string parser_name{"NS3Parser-test-parser"};

  const TraceEnvConfig trace_env_config = TraceEnvConfig::CreateFromYaml("tests/trace-env-config.yaml");
  TraceEnvironment trace_environment{trace_env_config};

  //ReaderBuffer<10> reader_buffer{"test-reader", true};
  ReaderBuffer<4096> reader_buffer{"test-reader"};
  REQUIRE_NOTHROW(reader_buffer.OpenFile(test_file_path));

  auto ns3_parser = create_shared<NS3Parser>(TraceException::kParserIsNull,
                                             trace_environment,
                                             parser_name);
  const uint64_t ident = ns3_parser->GetIdent();

  const NetworkEvent::NetworkDeviceType cosim_net_dev = NetworkEvent::NetworkDeviceType::kCosimNetDevice;
  const NetworkEvent::NetworkDeviceType simple_net_dev = NetworkEvent::NetworkDeviceType::kSimpleNetDevice;

  const auto within = NetworkEvent::EventBoundaryType::kWithinSimulator;
  const auto from = NetworkEvent::EventBoundaryType::kFromAdapter;
  const auto to = NetworkEvent::EventBoundaryType::kToAdapter;

  const std::vector<std::shared_ptr<Event>> to_match{
      std::make_shared<NetworkEnqueue>(1905164778000, ident, parser_name, 1, 2, cosim_net_dev, 0, true, 42, from, CreateEthHeader(0x806, 0xb0, 0x9a, 0xac, 0x67, 0x3c, 0x98, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff), std::nullopt, std::nullopt),
      std::make_shared<NetworkDequeue>(1905164778000, ident, parser_name, 1, 2, cosim_net_dev, 0, true, 42, within, CreateEthHeader(0x806, 0xb0, 0x9a, 0xac, 0x67, 0x3c, 0x98, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff), std::nullopt, std::nullopt),
      std::make_shared<NetworkEnqueue>(1905164778000, ident, parser_name, 1, 1, simple_net_dev, 0, true, 42, within, CreateEthHeader(0x3c98, 0x00, 0x01, 0xb0, 0x9a, 0xac, 0x67, 0x00, 0x01, 0x08, 0x00, 0x06, 0x04), CreateArpHeader(true, 192, 168, 64, 1, 192, 168, 64, 0), std::nullopt),
      std::make_shared<NetworkDequeue>(1905164778000, ident, parser_name, 1, 1, simple_net_dev, 0, true, 42, within, CreateEthHeader(0x3c98, 0x00, 0x01, 0xb0, 0x9a, 0xac, 0x67, 0x00, 0x01, 0x08, 0x00, 0x06, 0x04), CreateArpHeader(true, 192, 168, 64, 1, 192, 168, 64, 0), std::nullopt),
      std::make_shared<NetworkEnqueue>(1905164778000, ident, parser_name, 1, 3, cosim_net_dev, 0, true, 42, within, CreateEthHeader(0x3c98, 0x00, 0x01, 0xb0, 0x9a, 0xac, 0x67, 0x00, 0x01, 0x08, 0x00, 0x06, 0x04), CreateArpHeader(true, 192, 168, 64, 1, 192, 168, 64, 0), std::nullopt),
      std::make_shared<NetworkDequeue>(1905164778000, ident, parser_name, 1, 3, cosim_net_dev, 0, true, 42, to, CreateEthHeader(0x806, 0xb0, 0x9a, 0xac, 0x67, 0x3c, 0x98, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff), std::nullopt, std::nullopt),
      std::make_shared<NetworkEnqueue>(1905164778000, ident, parser_name, 0, 1, simple_net_dev, 0, true, 42, within, CreateEthHeader(0x3c98, 0x00, 0x01, 0xb0, 0x9a, 0xac, 0x67, 0x00, 0x01, 0x08, 0x00, 0x06, 0x04), CreateArpHeader(true, 192, 168, 64, 1, 192, 168, 64, 0), std::nullopt),
      std::make_shared<NetworkDequeue>(1905164778000, ident, parser_name, 0, 1, simple_net_dev, 0, true, 42, within, CreateEthHeader(0x3c98, 0x00, 0x01, 0xb0, 0x9a, 0xac, 0x67, 0x00, 0x01, 0x08, 0x00, 0x06, 0x04), CreateArpHeader(true, 192, 168, 64, 1, 192, 168, 64, 0), std::nullopt),
      std::make_shared<NetworkEnqueue>(1905164778000, ident, parser_name, 0, 2, cosim_net_dev, 0, true, 42, within, CreateEthHeader(0x3c98, 0x00, 0x01, 0xb0, 0x9a, 0xac, 0x67, 0x00, 0x01, 0x08, 0x00, 0x06, 0x04), CreateArpHeader(true, 192, 168, 64, 1, 192, 168, 64, 0), std::nullopt),
      std::make_shared<NetworkDequeue>(1905164778000, ident, parser_name, 0, 2, cosim_net_dev, 0, true, 42, to, CreateEthHeader(0x806, 0xb0, 0x9a, 0xac, 0x67, 0x3c, 0x98, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff), std::nullopt, std::nullopt),
      std::make_shared<NetworkEnqueue>(1905164778000, ident, parser_name, 0, 3, cosim_net_dev, 0, true, 42, within, CreateEthHeader(0x3c98, 0x00, 0x01, 0xb0, 0x9a, 0xac, 0x67, 0x00, 0x01, 0x08, 0x00, 0x06, 0x04), CreateArpHeader(true, 192, 168, 64, 1, 192, 168, 64, 0), std::nullopt),
      std::make_shared<NetworkDequeue>(1905164778000, ident, parser_name, 0, 3, cosim_net_dev, 0, true, 42, to, CreateEthHeader(0x806, 0xb0, 0x9a, 0xac, 0x67, 0x3c, 0x98, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff), std::nullopt, std::nullopt),
      std::make_shared<NetworkEnqueue>(1940008895000, ident, parser_name, 1, 3, cosim_net_dev, 3, false, 42, from, CreateEthHeader(0x806, 0xa8, 0x32, 0x06, 0x8c, 0x52, 0xb1, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff), std::nullopt, std::nullopt),
      std::make_shared<NetworkDequeue>(1940008895000, ident, parser_name, 1, 3, cosim_net_dev, 3, false, 42, within, CreateEthHeader(0x806, 0xa8, 0x32, 0x06, 0x8c, 0x52, 0xb1, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff), std::nullopt, std::nullopt),
      std::make_shared<NetworkEnqueue>(1940008895000, ident, parser_name, 1, 1, simple_net_dev, 3, false, 42, within, CreateEthHeader(0x52b1, 0x00, 0x01, 0xa8, 0x32, 0x06, 0x8c, 0x00, 0x01, 0x08, 0x00, 0x06, 0x04), CreateArpHeader(true, 192, 168, 64, 3, 192, 168, 64, 2), std::nullopt),
      std::make_shared<NetworkDequeue>(1940008895000, ident, parser_name, 1, 1, simple_net_dev, 3, false, 42, within, CreateEthHeader(0x52b1, 0x00, 0x01, 0xa8, 0x32, 0x06, 0x8c, 0x00, 0x01, 0x08, 0x00, 0x06, 0x04), CreateArpHeader(true, 192, 168, 64, 3, 192, 168, 64, 2), std::nullopt),
      std::make_shared<NetworkEnqueue>(1940008895000, ident, parser_name, 1, 2, cosim_net_dev, 3, false, 42, within, CreateEthHeader(0x52b1, 0x00, 0x01, 0xa8, 0x32, 0x06, 0x8c, 0x00, 0x01, 0x08, 0x00, 0x06, 0x04), CreateArpHeader(true, 192, 168, 64, 3, 192, 168, 64, 2), std::nullopt),
      std::make_shared<NetworkDequeue>(1940008895000, ident, parser_name, 1, 2, cosim_net_dev, 3, false, 42, to, CreateEthHeader(0x806, 0xa8, 0x32, 0x06, 0x8c, 0x52, 0xb1, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff), std::nullopt, std::nullopt),
      std::make_shared<NetworkEnqueue>(1940008895000, ident, parser_name, 0, 1, simple_net_dev, 3, false, 42, within, CreateEthHeader(0x52b1, 0x00, 0x01, 0xa8, 0x32, 0x06, 0x8c, 0x00, 0x01, 0x08, 0x00, 0x06, 0x04), CreateArpHeader(true, 192, 168, 64, 3, 192, 168, 64, 2), std::nullopt),
      std::make_shared<NetworkDequeue>(1940008895000, ident, parser_name, 0, 1, simple_net_dev, 3, false, 42, within, CreateEthHeader(0x52b1, 0x00, 0x01, 0xa8, 0x32, 0x06, 0x8c, 0x00, 0x01, 0x08, 0x00, 0x06, 0x04), CreateArpHeader(true, 192, 168, 64, 3, 192, 168, 64, 2), std::nullopt),
      std::make_shared<NetworkEnqueue>(1940008895000, ident, parser_name, 0, 2, cosim_net_dev, 3, false, 42, within, CreateEthHeader(0x52b1, 0x00, 0x01, 0xa8, 0x32, 0x06, 0x8c, 0x00, 0x01, 0x08, 0x00, 0x06, 0x04), CreateArpHeader(true, 192, 168, 64, 3, 192, 168, 64, 2), std::nullopt),
      std::make_shared<NetworkDequeue>(1940008895000, ident, parser_name, 0, 2, cosim_net_dev, 3, false, 42, to, CreateEthHeader(0x806, 0xa8, 0x32, 0x06, 0x8c, 0x52, 0xb1, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff), std::nullopt, std::nullopt),
      std::make_shared<NetworkEnqueue>(1940008895000, ident, parser_name, 0, 3, cosim_net_dev, 3, false, 42, within, CreateEthHeader(0x52b1, 0x00, 0x01, 0xa8, 0x32, 0x06, 0x8c, 0x00, 0x01, 0x08, 0x00, 0x06, 0x04), CreateArpHeader(true, 192, 168, 64, 3, 192, 168, 64, 2), std::nullopt),
      std::make_shared<NetworkDequeue>(1940008895000, ident, parser_name, 0, 3, cosim_net_dev, 3, false, 42, to, CreateEthHeader(0x806, 0xa8, 0x32, 0x06, 0x8c, 0x52, 0xb1, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff), std::nullopt, std::nullopt),
      std::make_shared<NetworkEnqueue>(2965590800000, ident, parser_name, 1, 2, cosim_net_dev, 6, true, 42, from, CreateEthHeader(0x806, 0xb0, 0x9a, 0xac, 0x67, 0x3c, 0x98, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff), std::nullopt, std::nullopt),
      std::make_shared<NetworkDequeue>(2965590800000, ident, parser_name, 1, 2, cosim_net_dev, 6, true, 42, within, CreateEthHeader(0x806, 0xb0, 0x9a, 0xac, 0x67, 0x3c, 0x98, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff), std::nullopt, std::nullopt),
      std::make_shared<NetworkEnqueue>(2965590800000, ident, parser_name, 1, 1, simple_net_dev, 6, true, 42, within, CreateEthHeader(0x3c98, 0x00, 0x01, 0xb0, 0x9a, 0xac, 0x67, 0x00, 0x01, 0x08, 0x00, 0x06, 0x04), CreateArpHeader(true, 192, 168, 64, 1, 192, 168, 64, 0), std::nullopt),
      std::make_shared<NetworkDequeue>(2965590800000, ident, parser_name, 1, 1, simple_net_dev, 6, true, 42, within, CreateEthHeader(0x3c98, 0x00, 0x01, 0xb0, 0x9a, 0xac, 0x67, 0x00, 0x01, 0x08, 0x00, 0x06, 0x04), CreateArpHeader(true, 192, 168, 64, 1, 192, 168, 64, 0), std::nullopt),
      std::make_shared<NetworkEnqueue>(2965590800000, ident, parser_name, 1, 3, cosim_net_dev, 6, true, 42, within, CreateEthHeader(0x3c98, 0x00, 0x01, 0xb0, 0x9a, 0xac, 0x67, 0x00, 0x01, 0x08, 0x00, 0x06, 0x04), CreateArpHeader(true, 192, 168, 64, 1, 192, 168, 64, 0), std::nullopt),
      std::make_shared<NetworkDequeue>(2965590800000, ident, parser_name, 1, 3, cosim_net_dev, 6, true, 42, to, CreateEthHeader (0x806, 0xb0, 0x9a, 0xac, 0x67, 0x3c, 0x98, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff), std::nullopt, std::nullopt),
      std::make_shared<NetworkEnqueue>(2965590800000, ident, parser_name, 0, 1, simple_net_dev, 6, true, 42, within, CreateEthHeader(0x3c98, 0x00, 0x01, 0xb0, 0x9a, 0xac, 0x67, 0x00, 0x01, 0x08, 0x00, 0x06, 0x04), CreateArpHeader(true, 192, 168, 64, 1, 192, 168, 64, 0), std::nullopt),
      std::make_shared<NetworkDequeue>(2965590800000, ident, parser_name, 0, 1, simple_net_dev, 6, true, 42, within, CreateEthHeader(0x3c98, 0x00, 0x01, 0xb0, 0x9a, 0xac, 0x67, 0x00, 0x01, 0x08, 0x00, 0x06, 0x04), CreateArpHeader(true, 192, 168, 64, 1, 192, 168, 64, 0), std::nullopt),
      std::make_shared<NetworkEnqueue>(2965590800000, ident, parser_name, 0, 2, cosim_net_dev, 6, true, 42, within, CreateEthHeader(0x3c98, 0x00, 0x01, 0xb0, 0x9a, 0xac, 0x67, 0x00, 0x01, 0x08, 0x00, 0x06, 0x04), CreateArpHeader(true, 192, 168, 64, 1, 192, 168, 64, 0), std::nullopt),
      std::make_shared<NetworkDequeue>(2965590800000, ident, parser_name, 0, 2, cosim_net_dev, 6, true, 42, to, CreateEthHeader(0x806, 0xb0, 0x9a, 0xac, 0x67, 0x3c, 0x98, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff), std::nullopt, std::nullopt),
      std::make_shared<NetworkEnqueue>(2965590800000, ident, parser_name, 0, 3, cosim_net_dev, 6, true, 42, within, CreateEthHeader(0x3c98, 0x00, 0x01, 0xb0, 0x9a, 0xac, 0x67, 0x00, 0x01, 0x08, 0x00, 0x06, 0x04), CreateArpHeader(true, 192, 168, 64, 1, 192, 168, 64, 0), std::nullopt),
      std::make_shared<NetworkDequeue>(2965590800000, ident, parser_name, 0, 3, cosim_net_dev, 6, true, 42, to, CreateEthHeader(0x806, 0xb0, 0x9a, 0xac, 0x67, 0x3c, 0x98, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff), std::nullopt, std::nullopt),
      std::make_shared<NetworkEnqueue>(2966110520000, ident, parser_name, 0, 2, cosim_net_dev, 9, true, 42, from, CreateEthHeader(0x806, 0x98, 0x59, 0x72, 0xd0, 0x60, 0xb3, 0xb0, 0x9a, 0xac, 0x67, 0x3c, 0x98), std::nullopt, std::nullopt),
      std::make_shared<NetworkDequeue>(2966110520000, ident, parser_name, 0, 2, cosim_net_dev, 9, true, 42, within, CreateEthHeader(0x806, 0x98, 0x59, 0x72, 0xd0, 0x60, 0xb3, 0xb0, 0x9a, 0xac, 0x67, 0x3c, 0x98), std::nullopt, std::nullopt),
      std::make_shared<NetworkEnqueue>(2966110520000, ident, parser_name, 0, 1, simple_net_dev, 9, true, 42, within, CreateEthHeader(0x60b3, 0x00, 0x02, 0x98, 0x59, 0x72, 0xd0, 0x00, 0x01, 0x08, 0x00, 0x06, 0x04), CreateArpHeader(false, 192, 168, 64, 0, 192, 168, 64, 1), std::nullopt),
      std::make_shared<NetworkDequeue>(2966110520000, ident, parser_name, 0, 1, simple_net_dev, 9, true, 42, within, CreateEthHeader(0x60b3, 0x00, 0x02, 0x98, 0x59, 0x72, 0xd0, 0x00, 0x01, 0x08, 0x00, 0x06, 0x04), CreateArpHeader(false, 192, 168, 64, 0, 192, 168, 64, 1), std::nullopt),
      std::make_shared<NetworkEnqueue>(2966110520000, ident, parser_name, 1, 1, simple_net_dev, 9, true, 42, within, CreateEthHeader(0x60b3, 0x00, 0x02, 0x98, 0x59, 0x72, 0xd0, 0x00, 0x01, 0x08, 0x00, 0x06, 0x04), CreateArpHeader(false, 192, 168, 64, 0, 192, 168, 64, 1), std::nullopt),
      std::make_shared<NetworkDequeue>(2966110520000, ident, parser_name, 1, 1, simple_net_dev, 9, true, 42, within, CreateEthHeader(0x60b3, 0x00, 0x02, 0x98, 0x59, 0x72, 0xd0, 0x00, 0x01, 0x08, 0x00, 0x06, 0x04), CreateArpHeader(false, 192, 168, 64, 0, 192, 168, 64, 1), std::nullopt),
      std::make_shared<NetworkEnqueue>(2966110520000, ident, parser_name, 1, 2, cosim_net_dev, 9, true, 42, within, CreateEthHeader(0x60b3, 0x00, 0x02, 0x98, 0x59, 0x72, 0xd0, 0x00, 0x01, 0x08, 0x00, 0x06, 0x04), CreateArpHeader(false, 192, 168, 64, 0, 192, 168, 64, 1), std::nullopt),
      std::make_shared<NetworkDequeue>(2966110520000, ident, parser_name, 1, 2, cosim_net_dev, 9, true, 42, to, CreateEthHeader(0x806, 0x98, 0x59, 0x72, 0xd0, 0x60, 0xb3, 0xb0, 0x9a, 0xac, 0x67, 0x3c, 0x98), std::nullopt, std::nullopt),

      std::make_shared<NetworkEnqueue>(2966621239000, ident, parser_name, 1, 2, cosim_net_dev, 12, true, 98, from, CreateEthHeader(0x800, 0xb0, 0x9a, 0xac, 0x67, 0x3c, 0x98, 0x98, 0x59, 0x72, 0xd0, 0x60, 0xb3), std::nullopt, std::nullopt),
      std::make_shared<NetworkDequeue>(2966621239000, ident, parser_name, 1, 2, cosim_net_dev, 12, true, 98, within, CreateEthHeader(0x800, 0xb0, 0x9a, 0xac, 0x67, 0x3c, 0x98, 0x98, 0x59, 0x72, 0xd0, 0x60, 0xb3), std::nullopt, std::nullopt),
      std::make_shared<NetworkEnqueue>(2966621239000, ident, parser_name, 1, 1, simple_net_dev, 12, true, 98, within, CreateEthHeader(0xc0a8, 0x40, 0x00, 0x40, 0x01, 0x13, 0x99, 0x45, 0x00, 0x00, 0x54, 0x25, 0xbe), std::nullopt, CreateIpHeader(84, 192, 168, 64, 1, 192, 168, 64, 0)),
      std::make_shared<NetworkDequeue>(2966621239000, ident, parser_name, 1, 1, simple_net_dev, 12, true, 98, within, CreateEthHeader(0xc0a8, 0x40, 0x00, 0x40, 0x01, 0x13, 0x99, 0x45, 0x00, 0x00, 0x54, 0x25, 0xbe), std::nullopt, CreateIpHeader(84, 192, 168, 64, 1, 192, 168, 64, 0)),
      std::make_shared<NetworkEnqueue>(2966621239000, ident, parser_name, 0, 1, simple_net_dev, 12, true, 98, within, CreateEthHeader(0xc0a8, 0x40, 0x00, 0x40, 0x01, 0x13, 0x99, 0x45, 0x00, 0x00, 0x54, 0x25, 0xbe), std::nullopt, CreateIpHeader(84, 192, 168, 64, 1, 192, 168, 64, 0)),
      std::make_shared<NetworkDequeue>(2966621239000, ident, parser_name, 0, 1, simple_net_dev, 12, true, 98, within, CreateEthHeader(0xc0a8, 0x40, 0x00, 0x40, 0x01, 0x13, 0x99, 0x45, 0x00, 0x00, 0x54, 0x25, 0xbe), std::nullopt, CreateIpHeader(84, 192, 168, 64, 1, 192, 168, 64, 0)),
      std::make_shared<NetworkEnqueue>(2966621239000, ident, parser_name, 0, 2, cosim_net_dev, 12, true, 98, within, CreateEthHeader(0xc0a8, 0x40, 0x00, 0x40, 0x01, 0x13, 0x99, 0x45, 0x00, 0x00, 0x54, 0x25, 0xbe), std::nullopt, CreateIpHeader(84, 192, 168, 64, 1, 192, 168, 64, 0)),
      std::make_shared<NetworkDequeue>(2966621239000, ident, parser_name, 0, 2, cosim_net_dev, 12, true, 98, to, CreateEthHeader(0x800, 0xb0, 0x9a, 0xac, 0x67, 0x3c, 0x98, 0x98, 0x59, 0x72, 0xd0, 0x60, 0xb3), std::nullopt, CreateIpHeader(84, 192, 168, 64, 1, 192, 168, 64, 0)),
      std::make_shared<NetworkEnqueue>(2967153066000, ident, parser_name, 0, 2, cosim_net_dev, 15, true, 98, from, CreateEthHeader(0x800, 0x98, 0x59, 0x72, 0xd0, 0x60, 0xb3, 0xb0, 0x9a, 0xac, 0x67, 0x3c, 0x98), std::nullopt, std::nullopt),
      std::make_shared<NetworkDequeue>(2967153066000, ident, parser_name, 0, 2, cosim_net_dev, 15, true, 98, within, CreateEthHeader(0x800, 0x98, 0x59, 0x72, 0xd0, 0x60, 0xb3, 0xb0, 0x9a, 0xac, 0x67, 0x3c, 0x98), std::nullopt, std::nullopt),
      std::make_shared<NetworkEnqueue>(2967153066000, ident, parser_name, 0, 1, simple_net_dev, 15, true, 98, within, CreateEthHeader(0xc0a8, 0x00, 0x00, 0x40, 0x01, 0xfb, 0x6e, 0x45, 0x00, 0x00, 0x54, 0x7d, 0xe8), std::nullopt, CreateIpHeader(84, 192, 168, 64, 0, 192, 168, 64, 1)),
      std::make_shared<NetworkDequeue>(2967153066000, ident, parser_name, 0, 1, simple_net_dev, 15, true, 98, within, CreateEthHeader(0xc0a8, 0x00, 0x00, 0x40, 0x01, 0xfb, 0x6e, 0x45, 0x00, 0x00, 0x54, 0x7d, 0xe8), std::nullopt, CreateIpHeader(84, 192, 168, 64, 0, 192, 168, 64, 1)),
      std::make_shared<NetworkEnqueue>(2967153066000, ident, parser_name, 1, 1, simple_net_dev, 15, true, 98, within, CreateEthHeader(0xc0a8, 0x00, 0x00, 0x40, 0x01, 0xfb, 0x6e, 0x45, 0x00, 0x00, 0x54, 0x7d, 0xe8), std::nullopt, CreateIpHeader(84, 192, 168, 64, 0, 192, 168, 64, 1)),
      std::make_shared<NetworkDequeue>(2967153066000, ident, parser_name, 1, 1, simple_net_dev, 15, true, 98, within, CreateEthHeader(0xc0a8, 0x00, 0x00, 0x40, 0x01, 0xfb, 0x6e, 0x45, 0x00, 0x00, 0x54, 0x7d, 0xe8), std::nullopt, CreateIpHeader(84, 192, 168, 64, 0, 192, 168, 64, 1)),
      std::make_shared<NetworkEnqueue>(2967153066000, ident, parser_name, 1, 2, cosim_net_dev, 15, true, 98, within, CreateEthHeader(0xc0a8, 0x00, 0x00, 0x40, 0x01, 0xfb, 0x6e, 0x45, 0x00, 0x00, 0x54, 0x7d, 0xe8), std::nullopt, CreateIpHeader(84, 192, 168, 64, 0, 192, 168, 64, 1)),
      std::make_shared<NetworkDequeue>(2967153066000, ident, parser_name, 1, 2, cosim_net_dev, 15, true, 98, to, CreateEthHeader(0x800, 0x98, 0x59, 0x72, 0xd0, 0x60, 0xb3, 0xb0, 0x9a, 0xac, 0x67, 0x3c, 0x98), std::nullopt, CreateIpHeader(84, 192, 168, 64, 0, 192, 168, 64, 1)),

      std::make_shared<NetworkEnqueue>(2992470011000, ident, parser_name, 1, 3, cosim_net_dev, 18, false, 42, from, CreateEthHeader(0x806, 0xa8, 0x32, 0x06, 0x8c, 0x52, 0xb1, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff), std::nullopt, std::nullopt),
      std::make_shared<NetworkDequeue>(2992470011000, ident, parser_name, 1, 3, cosim_net_dev, 18, false, 42, within, CreateEthHeader(0x806, 0xa8, 0x32, 0x06, 0x8c, 0x52, 0xb1, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff), std::nullopt, std::nullopt),
      std::make_shared<NetworkEnqueue>(2992470011000, ident, parser_name, 1, 1, simple_net_dev, 18, false, 42, within, CreateEthHeader(0x52b1, 0x00, 0x01, 0xa8, 0x32, 0x06, 0x8c, 0x00, 0x01, 0x08, 0x00, 0x06, 0x04), CreateArpHeader(true, 192, 168, 64, 3, 192, 168, 64, 2), std::nullopt),
      std::make_shared<NetworkDequeue>(2992470011000, ident, parser_name, 1, 1, simple_net_dev, 18, false, 42, within, CreateEthHeader(0x52b1, 0x00, 0x01, 0xa8, 0x32, 0x06, 0x8c, 0x00, 0x01, 0x08, 0x00, 0x06, 0x04), CreateArpHeader(true, 192, 168, 64, 3, 192, 168, 64, 2), std::nullopt),
      std::make_shared<NetworkEnqueue>(2992470011000, ident, parser_name, 1, 2, cosim_net_dev, 18, false, 42, within, CreateEthHeader(0x52b1, 0x00, 0x01, 0xa8, 0x32, 0x06, 0x8c, 0x00, 0x01, 0x08, 0x00, 0x06, 0x04), CreateArpHeader(true, 192, 168, 64, 3, 192, 168, 64, 2), std::nullopt),
      std::make_shared<NetworkDequeue>(2992470011000, ident, parser_name, 1, 2, cosim_net_dev, 18, false, 42, to, CreateEthHeader(0x806, 0xa8, 0x32, 0x06, 0x8c, 0x52, 0xb1, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff), std::nullopt, std::nullopt),
      std::make_shared<NetworkEnqueue>(2992470011000, ident, parser_name, 0, 1, simple_net_dev, 18, false, 42, within, CreateEthHeader(0x52b1, 0x00, 0x01, 0xa8, 0x32, 0x06, 0x8c, 0x00, 0x01, 0x08, 0x00, 0x06, 0x04), CreateArpHeader(true, 192, 168, 64, 3, 192, 168, 64, 2), std::nullopt),
      std::make_shared<NetworkDequeue>(2992470011000, ident, parser_name, 0, 1, simple_net_dev, 18, false, 42, within, CreateEthHeader(0x52b1, 0x00, 0x01, 0xa8, 0x32, 0x06, 0x8c, 0x00, 0x01, 0x08, 0x00, 0x06, 0x04), CreateArpHeader(true, 192, 168, 64, 3, 192, 168, 64, 2), std::nullopt),
      std::make_shared<NetworkEnqueue>(2992470011000, ident, parser_name, 0, 2, cosim_net_dev, 18, false, 42, within, CreateEthHeader(0x52b1, 0x00, 0x01, 0xa8, 0x32, 0x06, 0x8c, 0x00, 0x01, 0x08, 0x00, 0x06, 0x04), CreateArpHeader(true, 192, 168, 64, 3, 192, 168, 64, 2), std::nullopt),
      std::make_shared<NetworkDequeue>(2992470011000, ident, parser_name, 0, 2, cosim_net_dev, 18, false, 42, to, CreateEthHeader(0x806, 0xa8, 0x32, 0x06, 0x8c, 0x52, 0xb1, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff), std::nullopt, std::nullopt),
      std::make_shared<NetworkEnqueue>(2992470011000, ident, parser_name, 0, 3, cosim_net_dev, 18, false, 42, within, CreateEthHeader(0x52b1, 0x00, 0x01, 0xa8, 0x32, 0x06, 0x8c, 0x00, 0x01, 0x08, 0x00, 0x06, 0x04), CreateArpHeader(true, 192, 168, 64, 3, 192, 168, 64, 2), std::nullopt),
      std::make_shared<NetworkDequeue>(2992470011000, ident, parser_name, 0, 3, cosim_net_dev, 18, false, 42, to, CreateEthHeader(0x806, 0xa8, 0x32, 0x06, 0x8c, 0x52, 0xb1, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff), std::nullopt, std::nullopt),
      std::make_shared<NetworkEnqueue>(2992989915000, ident, parser_name, 0, 3, cosim_net_dev, 21, false, 42, from, CreateEthHeader(0x806, 0x78, 0xd2, 0x22, 0xc4, 0xb0, 0xa9, 0xa8, 0x32, 0x06, 0x8c, 0x52, 0xb1), std::nullopt, std::nullopt),
      std::make_shared<NetworkDequeue>(2992989915000, ident, parser_name, 0, 3, cosim_net_dev, 21, false, 42, within, CreateEthHeader(0x806, 0x78, 0xd2, 0x22, 0xc4, 0xb0, 0xa9, 0xa8, 0x32, 0x06, 0x8c, 0x52, 0xb1), std::nullopt, std::nullopt),
      std::make_shared<NetworkEnqueue>(2992989915000, ident, parser_name, 0, 1, simple_net_dev, 21, false, 42, within, CreateEthHeader(0xb0a9, 0x00, 0x02, 0x78, 0xd2, 0x22, 0xc4, 0x00, 0x01, 0x08, 0x00, 0x06, 0x04), CreateArpHeader(false, 192, 168, 64, 2, 192, 168, 64, 3), std::nullopt),
      std::make_shared<NetworkDequeue>(2992989915000, ident, parser_name, 0, 1, simple_net_dev, 21, false, 42, within, CreateEthHeader(0xb0a9, 0x00, 0x02, 0x78, 0xd2, 0x22, 0xc4, 0x00, 0x01, 0x08, 0x00, 0x06, 0x04), CreateArpHeader(false, 192, 168, 64, 2, 192, 168, 64, 3), std::nullopt),
      std::make_shared<NetworkEnqueue>(2992989915000, ident, parser_name, 1, 1, simple_net_dev, 21, false, 42, within, CreateEthHeader(0xb0a9, 0x00, 0x02, 0x78, 0xd2, 0x22, 0xc4, 0x00, 0x01, 0x08, 0x00, 0x06, 0x04), CreateArpHeader(false, 192, 168, 64, 2, 192, 168, 64, 3), std::nullopt),
      std::make_shared<NetworkDequeue>(2992989915000, ident, parser_name, 1, 1, simple_net_dev, 21, false, 42, within, CreateEthHeader(0xb0a9, 0x00, 0x02, 0x78, 0xd2, 0x22, 0xc4, 0x00, 0x01, 0x08, 0x00, 0x06, 0x04), CreateArpHeader(false, 192, 168, 64, 2, 192, 168, 64, 3), std::nullopt),
      std::make_shared<NetworkEnqueue>(2992989915000, ident, parser_name, 1, 3, cosim_net_dev, 21, false, 42, within, CreateEthHeader(0xb0a9, 0x00, 0x02, 0x78, 0xd2, 0x22, 0xc4, 0x00, 0x01, 0x08, 0x00, 0x06, 0x04), CreateArpHeader(false, 192, 168, 64, 2, 192, 168, 64, 3), std::nullopt),
      std::make_shared<NetworkDequeue>(2992989915000, ident, parser_name, 1, 3, cosim_net_dev, 21, false, 42, to, CreateEthHeader(0x806, 0x78, 0xd2, 0x22, 0xc4, 0xb0, 0xa9, 0xa8, 0x32, 0x06, 0x8c, 0x52, 0xb1), std::nullopt, std::nullopt),

      std::make_shared<NetworkEnqueue>(2993500468000, ident, parser_name, 1, 3, cosim_net_dev, 24, false, 98, from, CreateEthHeader(0x800,  0xa8, 0x32, 0x06, 0x8c, 0x52, 0xb1, 0x78, 0xd2, 0x22, 0xc4, 0xb0, 0xa9), std::nullopt, std::nullopt),
      std::make_shared<NetworkDequeue>(2993500468000, ident, parser_name, 1, 3, cosim_net_dev, 24, false, 98, within, CreateEthHeader(0x800,  0xa8, 0x32, 0x06, 0x8c, 0x52, 0xb1, 0x78, 0xd2, 0x22, 0xc4, 0xb0, 0xa9), std::nullopt, std::nullopt),
      std::make_shared<NetworkEnqueue>(2993500468000, ident, parser_name, 1, 1, simple_net_dev, 24, false, 98, within, CreateEthHeader(0xc0a8, 0x40, 0x00, 0x40, 0x01, 0xf4, 0xd9, 0x45, 0x00, 0x00, 0x54, 0x44, 0x79), std::nullopt, CreateIpHeader(84, 192, 168, 64, 3, 192, 168, 64, 2)),
      std::make_shared<NetworkDequeue>(2993500468000, ident, parser_name, 1, 1, simple_net_dev, 24, false, 98, within, CreateEthHeader(0xc0a8, 0x40, 0x00, 0x40, 0x01, 0xf4, 0xd9, 0x45, 0x00, 0x00, 0x54, 0x44, 0x79), std::nullopt, CreateIpHeader(84, 192, 168, 64, 3, 192, 168, 64, 2)),
      std::make_shared<NetworkEnqueue>(2993500468000, ident, parser_name, 0, 1, simple_net_dev, 24, false, 98, within, CreateEthHeader(0xc0a8, 0x40, 0x00, 0x40, 0x01, 0xf4, 0xd9, 0x45, 0x00, 0x00, 0x54, 0x44, 0x79), std::nullopt, CreateIpHeader(84, 192, 168, 64, 3, 192, 168, 64, 2)),
      std::make_shared<NetworkDequeue>(2993500468000, ident, parser_name, 0, 1, simple_net_dev, 24, false, 98, within, CreateEthHeader(0xc0a8, 0x40, 0x00, 0x40, 0x01, 0xf4, 0xd9, 0x45, 0x00, 0x00, 0x54, 0x44, 0x79), std::nullopt, CreateIpHeader(84, 192, 168, 64, 3, 192, 168, 64, 2)),
      std::make_shared<NetworkEnqueue>(2993500468000, ident, parser_name, 0, 3, cosim_net_dev, 24, false, 98, within, CreateEthHeader(0xc0a8, 0x40, 0x00, 0x40, 0x01, 0xf4, 0xd9, 0x45, 0x00, 0x00, 0x54, 0x44, 0x79), std::nullopt, CreateIpHeader(84, 192, 168, 64, 3, 192, 168, 64, 2)),
      std::make_shared<NetworkDequeue>(2993500468000, ident, parser_name, 0, 3, cosim_net_dev, 24, false, 98, to, CreateEthHeader(0x800,  0xa8, 0x32, 0x06, 0x8c, 0x52, 0xb1, 0x78, 0xd2, 0x22, 0xc4, 0xb0, 0xa9), std::nullopt, CreateIpHeader(84, 192, 168, 64, 3, 192, 168, 64, 2)),
      std::make_shared<NetworkEnqueue>(2994032999000, ident, parser_name, 0, 3, cosim_net_dev, 27, false, 98, from, CreateEthHeader(0x800,  0x78, 0xd2, 0x22, 0xc4, 0xb0, 0xa9, 0xa8, 0x32, 0x06, 0x8c, 0x52, 0xb1), std::nullopt, std::nullopt),
      std::make_shared<NetworkDequeue>(2994032999000, ident, parser_name, 0, 3, cosim_net_dev, 27, false, 98, within, CreateEthHeader(0x800,  0x78, 0xd2, 0x22, 0xc4, 0xb0, 0xa9, 0xa8, 0x32, 0x06, 0x8c, 0x52, 0xb1), std::nullopt, std::nullopt),
      std::make_shared<NetworkEnqueue>(2994032999000, ident, parser_name, 0, 1, simple_net_dev, 27, false, 98, within, CreateEthHeader(0xc0a8, 0x00, 0x00, 0x40, 0x01, 0xb4, 0xd6, 0x45, 0x00, 0x00, 0x54, 0xc4, 0x7c), std::nullopt, CreateIpHeader(84, 192, 168, 64, 2, 192, 168, 64, 3)),
      std::make_shared<NetworkDequeue>(2994032999000, ident, parser_name, 0, 1, simple_net_dev, 27, false, 98, within, CreateEthHeader(0xc0a8, 0x00, 0x00, 0x40, 0x01, 0xb4, 0xd6, 0x45, 0x00, 0x00, 0x54, 0xc4, 0x7c), std::nullopt, CreateIpHeader(84, 192, 168, 64, 2, 192, 168, 64, 3)),
      std::make_shared<NetworkEnqueue>(2994032999000, ident, parser_name, 1, 1, simple_net_dev, 27, false, 98, within, CreateEthHeader(0xc0a8, 0x00, 0x00, 0x40, 0x01, 0xb4, 0xd6, 0x45, 0x00, 0x00, 0x54, 0xc4, 0x7c), std::nullopt, CreateIpHeader(84, 192, 168, 64, 2, 192, 168, 64, 3)),
      std::make_shared<NetworkDequeue>(2994032999000, ident, parser_name, 1, 1, simple_net_dev, 27, false, 98, within, CreateEthHeader(0xc0a8, 0x00, 0x00, 0x40, 0x01, 0xb4, 0xd6, 0x45, 0x00, 0x00, 0x54, 0xc4, 0x7c), std::nullopt, CreateIpHeader(84, 192, 168, 64, 2, 192, 168, 64, 3)),
      std::make_shared<NetworkEnqueue>(2994032999000, ident, parser_name, 1, 3, cosim_net_dev, 27, false, 98, within, CreateEthHeader(0xc0a8, 0x00, 0x00, 0x40, 0x01, 0xb4, 0xd6, 0x45, 0x00, 0x00, 0x54, 0xc4, 0x7c), std::nullopt, CreateIpHeader(84, 192, 168, 64, 2, 192, 168, 64, 3)),
      std::make_shared<NetworkDequeue>(2994032999000, ident, parser_name, 1, 3, cosim_net_dev, 27, false, 98, to, CreateEthHeader(0x800,  0x78, 0xd2, 0x22, 0xc4, 0xb0, 0xa9, 0xa8, 0x32, 0x06, 0x8c, 0x52, 0xb1), std::nullopt, CreateIpHeader(84, 192, 168, 64, 2, 192, 168, 64, 3))
  };

  std::shared_ptr<Event> parsed_event;
  std::pair<bool, LineHandler *> bh_p;
  for (const auto &match : to_match) {
    REQUIRE(reader_buffer.HasStillLine());
    REQUIRE_NOTHROW(bh_p = reader_buffer.NextHandler());
    REQUIRE(bh_p.first);
    LineHandler &line_handler = *bh_p.second;
    parsed_event = ns3_parser->ParseEvent(line_handler).get();
    REQUIRE(parsed_event);
    REQUIRE(parsed_event->Equal(*match));
  }

  REQUIRE_FALSE(reader_buffer.HasStillLine());
  REQUIRE_NOTHROW(bh_p = reader_buffer.NextHandler());
  REQUIRE_FALSE(bh_p.first);
  REQUIRE(bh_p.second == nullptr);
}

TEST_CASE("test non specified Payload (size=", "[test-no-payload]") {

  const TraceEnvConfig trace_env_config = TraceEnvConfig::CreateFromYaml("tests/trace-env-config.yaml");
  TraceEnvironment trace_environment{trace_env_config};
  NS3Parser ns3_parser{trace_environment,"test parser"};

  std::vector<std::string> test_lines{
    "+  1001000000000 /$ns3::NodeListPriv/NodeList/0/$ns3::Node/DeviceList/1/$ns3::SimpleNetDevice/TxQueue/Enqueue Packet-Uid=2 Intersting=false ns3::EthernetHeader( length/type=0x9, source=00:01:00:00:00:00, destination=00:01:08:00:06:04) ns3::ArpHeader(request source mac: 00-06-00:00:00:00:00:09 source ipv4: 192.168.64.4 dest ipv4: 192.168.64.6) ns3::ArpHeader (request source mac: 00-06-00:00:00:00:00:09 source ipv4: 192.168.64.4 dest ipv4: 192.168.64.6)",
    "-  1001000000000 /$ns3::NodeListPriv/NodeList/0/$ns3::Node/DeviceList/1/$ns3::SimpleNetDevice/TxQueue/Dequeue Packet-Uid=2 Intersting=false ns3::EthernetHeader( length/type=0x9, source=00:01:00:00:00:00, destination=00:01:08:00:06:04) ns3::ArpHeader(request source mac: 00-06-00:00:00:00:00:09 source ipv4: 192.168.64.4 dest ipv4: 192.168.64.6) ns3::ArpHeader (request source mac: 00-06-00:00:00:00:00:09 source ipv4: 192.168.64.4 dest ipv4: 192.168.64.6)",
    "+  1001000000000 /$ns3::NodeListPriv/NodeList/0/$ns3::Node/DeviceList/2/$ns3::CosimNetDevice/RxPacketFromNetwork Packet-Uid=2 Intersting=false ns3::EthernetHeader( length/type=0x9, source=00:01:00:00:00:00, destination=00:01:08:00:06:04) ns3::ArpHeader(request source mac: 00-06-00:00:00:00:00:09 source ipv4: 192.168.64.4 dest ipv4: 192.168.64.6) ns3::ArpHeader (request source mac: 00-06-00:00:00:00:00:09 source ipv4: 192.168.64.4 dest ipv4: 192.168.64.6)",
    "-  1001000000000 /$ns3::NodeListPriv/NodeList/0/$ns3::Node/DeviceList/2/$ns3::CosimNetDevice/TxPacketToAdapter Packet-Uid=2 Intersting=false ns3::EthernetHeader ( length/type=0x806, source=00:00:00:00:00:09, destination=ff:ff:ff:ff:ff:ff) ns3::ArpHeader (request source mac: 00-06-00:00:00:00:00:09 source ipv4: 192.168.64.4 dest ipv4: 192.168.64.6)"
  };

  for (auto &str : test_lines) {
    LineHandler line_handler(str.data(), str.size());
    auto event = ns3_parser.ParseEvent(line_handler).get();
    REQUIRE(event != nullptr);
  }
}