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

#ifndef SIMBRICKS_TRACE_PARSER_H_
#define SIMBRICKS_TRACE_PARSER_H_

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "spdlog/spdlog.h"

#include "util/exception.h"
#include "util/componenttable.h"
#include "sync/corobelt.h"
#include "events/events.h"
#include "reader/cReader.h"
#include "env/traceEnvironment.h"
#include "analytics/timer.h"
#include "util/utils.h"
#include "events/printer.h"

bool ParseMacAddress(LineHandler &line_handler, NetworkEvent::MacAddress &addr);

bool ParseIpAddress(LineHandler &line_handler, NetworkEvent::Ipv4 &addr);

std::optional<NetworkEvent::EthernetHeader> TryParseEthernetHeader(LineHandler &line_handler);

std::optional<NetworkEvent::ArpHeader> TryParseArpHeader(LineHandler &line_handler);

std::optional<NetworkEvent::Ipv4Header> TryParseIpHeader(LineHandler &line_handler);

class LogParser {

 protected:
  TraceEnvironment &trace_environment_;

 private:
  const std::string name_;
  const uint64_t identifier_;

 protected:
  bool ParseTimestamp(LineHandler &line_handler, uint64_t &timestamp);

  bool ParseAddress(LineHandler &line_handler, uint64_t &address);

 public:
  explicit LogParser(TraceEnvironment &trace_environment,
                     const std::string name)
      : trace_environment_(trace_environment),
        name_(name),
        identifier_(trace_environment_.GetNextParserId()) {
  };

  inline uint64_t GetIdent() const {
    return identifier_;
  }

  inline const std::string &GetName() const {
    return name_;
  }

  virtual concurrencpp::result<std::shared_ptr<Event>>
  ParseEvent(LineHandler &line_handler) = 0;
};

class Gem5Parser : public LogParser {
  const ComponentFilter &component_table_;

 protected:
  std::shared_ptr<Event> ParseGlobalEvent(LineHandler &line_handler, uint64_t timestamp);

  concurrencpp::result<std::shared_ptr<Event>>
  ParseSystemSwitchCpus(LineHandler &line_handler, uint64_t timestamp);

  std::shared_ptr<Event>
  ParseSystemPcPciHost(LineHandler &line_handler, uint64_t timestamp);

  std::shared_ptr<Event>
  ParseSystemPcPciHostInterface(LineHandler &line_handler, uint64_t timestamp);

  std::shared_ptr<Event>
  ParseSystemPcSimbricks(LineHandler &line_handler, uint64_t timestamp);

  std::shared_ptr<Event>
  ParseSimbricksEvent(LineHandler &line_handler, uint64_t timestamp);

 public:
  explicit Gem5Parser(TraceEnvironment &trace_environment,
                      const std::string name,
                      const ComponentFilter &component_table)
      : LogParser(trace_environment, name),
        component_table_(component_table) {
  }

  concurrencpp::result<std::shared_ptr<Event>>
  ParseEvent(LineHandler &line_handler) override;
};

class NicBmParser : public LogParser {
 protected:
  bool ParseOffLenValComma(LineHandler &line_handler, uint64_t &off, size_t &len, uint64_t &val);

  bool ParseOpAddrLenPending(LineHandler &line_handler, uint64_t &op, uint64_t &addr, size_t &len,
                             size_t &pending, bool with_pending);

  bool ParseMacAddress(LineHandler &line_handler, uint64_t &address);

  bool ParseSyncInfo(LineHandler &line_handler, bool &sync_pcie, bool &sync_eth);

 public:
  explicit NicBmParser(TraceEnvironment &trace_environment,
                       const std::string name)
      : LogParser(trace_environment, name) {}

  concurrencpp::result<std::shared_ptr<Event>>
  ParseEvent(LineHandler &line_handler) override;
};

class NS3Parser : public LogParser {

  std::shared_ptr<Event> ParseNetDevice(LineHandler &line_handler,
                                        uint64_t timestamp,
                                        EventType type,
                                        int node,
                                        int device,
                                        NetworkEvent::NetworkDeviceType device_type);

 public:
  explicit NS3Parser(TraceEnvironment &trace_environment,
                     const std::string name)
      : LogParser(trace_environment, name) {}

  concurrencpp::result<std::shared_ptr<Event>>
  ParseEvent(LineHandler &line_handler) override;
};

template<bool NamedPipe, size_t LineBufferSizePages = 16>
requires SizeLagerZero<LineBufferSizePages>
inline concurrencpp::result<void>
ResetFillBufferTask(//concurrencpp::executor_tag,
    const std::string name,
    const std::string log_file_path,
    std::shared_ptr<LogParser> log_parser,
    std::shared_ptr<concurrencpp::executor> executor,
    std::shared_ptr<concurrencpp::executor> back,
    std::shared_ptr<CoroBoundedChannel<std::shared_ptr<Event>>> event_buffer_channel) {
  throw_if_empty(log_parser, "parser is null", source_loc::current());
  throw_if_empty(executor, TraceException::kResumeExecutorNull, source_loc::current());
  throw_if_empty(back, TraceException::kResumeExecutorNull, source_loc::current());
  throw_if_empty(event_buffer_channel, TraceException::kChannelIsNull, source_loc::current());

  ReaderBuffer<MultiplePagesBytes(LineBufferSizePages)> line_handler_buffer{name};

  if (not line_handler_buffer.IsOpen()) {
    line_handler_buffer.OpenFile(log_file_path, NamedPipe);
  }

  std::pair<bool, LineHandler *> bh_p;
//  for (bh_p = co_await back->submit([&] { return line_handler_buffer.NextHandler(); });
//       bh_p.first and bh_p.second;
//       bh_p = co_await back->submit([&] { return line_handler_buffer.NextHandler(); })) {
  for (bh_p = line_handler_buffer.NextHandler();
       bh_p.first and bh_p.second;
       bh_p = line_handler_buffer.NextHandler()) {

    LineHandler &line_handler = *bh_p.second;

    spdlog::trace("{} found another line: '{}'", name, line_handler.GetRawLine());
    std::shared_ptr<Event> event = co_await log_parser->ParseEvent(line_handler);
    if (event == nullptr) {
      spdlog::trace("{} was unable to parse event", name);
      continue;
    }
    spdlog::trace("{} parsed another event: {}", name, *event);

    throw_if_empty(event, TraceException::kEventIsNull, source_loc::current());
//    event_buffer_channel->PokeAwaiters();
    co_await event_buffer_channel->Push(executor, std::move(event));
//    event_buffer_channel->PokeAwaiters();
  }

  co_await event_buffer_channel->CloseChannel(executor);
  co_return;
}

template<bool NamedPipe, size_t LineBufferSizePages = 16> requires SizeLagerZero<LineBufferSizePages>
class BufferedEventProvider : public Producer<std::shared_ptr<Event>> {

  TraceEnvironment &trace_environment_;
  const std::string name_;
  const std::string log_file_path_;
  std::shared_ptr<LogParser> log_parser_; // NOTE: only access from within FillBuffer()!!!
  std::shared_ptr<concurrencpp::executor> background_exec_;
  concurrencpp::result<void> fill_buffer_task_;
  bool started_fill_task_ = false;
  std::shared_ptr<CoroBoundedChannel<std::shared_ptr<Event>>> event_buffer_channel_;
  ReaderBuffer<MultiplePagesBytes(LineBufferSizePages)> line_handler_buffer_;

 public:
  explicit BufferedEventProvider(TraceEnvironment &trace_environment,
                                 const std::string name,
                                 const std::string log_file_path,
                                 std::shared_ptr<LogParser> log_parser)
      : Producer<std::shared_ptr<Event>>(),
        trace_environment_(trace_environment),
        name_(name),
        log_file_path_(log_file_path),
        log_parser_(std::move(log_parser)),
        background_exec_(trace_environment_.GetBackgroundPoolExecutor()),
        line_handler_buffer_(name) {
    event_buffer_channel_ = create_shared<CoroBoundedChannel<std::shared_ptr<Event>>>(
        TraceException::kChannelIsNull,
        trace_environment_.GetConfig().GetEventBufferSize());
  };

  ~BufferedEventProvider() = default;

  concurrencpp::result<std::optional<std::shared_ptr<Event>>>
  produce(std::shared_ptr<concurrencpp::executor> executor) override {
    if (not started_fill_task_) {
      auto te = trace_environment_.GetWorkerThreadExecutor();
      te->post(std::bind(ResetFillBufferTask<NamedPipe, LineBufferSizePages>,
                         name_,
                         log_file_path_,
                         log_parser_,
                         te,
                         te,
                         event_buffer_channel_));
      started_fill_task_ = true;
    }

    std::optional<std::shared_ptr<Event>> event = co_await event_buffer_channel_->Pop(executor);
//    event_buffer_channel_->PokeAwaiters();
    co_return event;
  }

//  concurrencpp::result<std::optional<std::shared_ptr<Event>>>
//  produce(std::shared_ptr<concurrencpp::executor> executor) override {
//
//    if (not line_handler_buffer_.IsOpen()) {
//      line_handler_buffer_.OpenFile(log_file_path_, NamedPipe);
//    }
//
//    std::pair<bool, LineHandler *> bh_p;
//    for (bh_p = line_handler_buffer_.NextHandler();
//         bh_p.first and bh_p.second;
//         bh_p = line_handler_buffer_.NextHandler()) {
//
//      LineHandler &line_handler = *bh_p.second;
//      spdlog::trace("{} found another line: '{}'", name_, line_handler.GetRawLine());
//      std::shared_ptr<Event> event = co_await log_parser_->ParseEvent(line_handler);
//      if (event == nullptr) {
//        spdlog::trace("{} was unable to parse event", name_);
//        continue;
//      }
//      spdlog::trace("{} parsed another event: {}", name_, *event);
//
//      throw_if_empty(event, TraceException::kEventIsNull, source_loc::current());
//      co_return event;
//    }
//
//    co_return std::nullopt;
//  }
};

#endif  // SIMBRICKS_TRACE_PARSER_H_