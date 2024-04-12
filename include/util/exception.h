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

#ifndef SIMBRICKS_TRACE_EXCEPTION_H_
#define SIMBRICKS_TRACE_EXCEPTION_H_

#include "spdlog/spdlog.h"

#include <exception>
#include <stdexcept>
#include <memory>
#include <string>
#include <sstream>
#include <optional>
#include <iostream>
//#include <source_location>
#include <experimental/source_location>
using source_loc = std::experimental::source_location;

class TraceException : public std::exception {
  const std::string error_message_;

  static std::string build_error_msg(const std::string_view location,
                                     const std::string_view message) noexcept {
    std::stringstream err_msg;
    err_msg << "TraceException occured in " << location << ": " << message;
    return err_msg.str();
  }

 public:
  inline static const std::string kResumeExecutorNull{"concurrencpp::executor is null"};
  inline static const std::string kPipelineNull{"Pipeline is null"};
  inline static const std::string kChannelIsNull{"channel<ValueType> is null"};
  inline static const std::string kPipeIsNull{"pipe<ValueType> is null"};
  inline static const std::string kConsumerIsNull{"consumer<ValueType> is null"};
  inline static const std::string kHandlerIsNull{"consumer<ValueType> is null"};
  inline static const std::string kProducerIsNull{"producer<ValueType> is null"};
  inline static const std::string kEventIsNull{"Event is null"};
  inline static const std::string kTraceIsNull{"Trace is null"};
  inline static const std::string kSpanIsNull{"Span is null"};
  inline static const std::string kParserIsNull{"LogParser is null"};
  inline static const std::string kActorIsNull{"EventStreamActor is null"};
  inline static const std::string kPrinterIsNull{"printer is null"};
  inline static const std::string kContextIsNull{"context is null"};
  inline static const std::string kEventStreamParserNull{"EventStreamParser is null"};
  inline static const std::string kSpannerIsNull{"Spanner is null"};
  inline static const std::string kCouldNotPushToContextQueue{"could not push value into context queue"};
  inline static const std::string kQueueIsNull{"ContextQueue<...> is null"};
  inline static const std::string kSpanExporterNull{"SpanExporter is null"};
  inline static const std::string kSpanProcessorNull{"SpanProcessor is null"};
  inline static const std::string kTraceProviderNull{"TracerProvider is null"};
  inline static const std::string kInvalidId{"Invalid Identifier"};
  inline static const std::string kBufferedEventProviderIsNull{"BufferedEventProvider is null"};

  explicit TraceException(const char *location, const char *message)
      : error_message_(build_error_msg(location, message)) {}

  explicit TraceException(const std::string &location, const std::string &message) : error_message_(
      build_error_msg(location, message)) {}

  explicit TraceException(const std::string &&location, const std::string &&message) : error_message_(
      build_error_msg(location, message)) {}

  const char *what() const noexcept override {
    return error_message_.c_str();
  }
};

inline std::string LocationToString(const source_loc &location) {
  std::stringstream msg{location.file_name()};
  msg << "-" << location.function_name() << "(" << location.line() << "," << location.column() << ")";
  return msg.str();
}

template<typename Value>
inline void throw_if_empty(const std::shared_ptr<Value> to_check,
                           const char *message,
                           const source_loc &location) {
  if (not to_check) {
    const TraceException trace_exception{LocationToString(location), message};
    spdlog::critical("{}", trace_exception.what());
    std::terminate();
    // throw TraceException(LocationToString(location), message);
  }
}

template<typename Value>
inline void throw_if_empty(const std::shared_ptr<Value> to_check,
                           const std::string &message,
                           const source_loc &location) {
  throw_if_empty(to_check, message.c_str(), location);
}

template<typename Value>
inline void throw_if_empty(const std::shared_ptr<Value> &to_check,
                           const std::string &&message,
                           const source_loc &location) {
  throw_if_empty(to_check, message.c_str(), location);
}

template<typename Value>
inline void throw_if_empty(const std::unique_ptr<Value> &to_check,
                           const char *message,
                           const source_loc &location) {
  if (not to_check) {
    const TraceException trace_exception{LocationToString(location), message};
    spdlog::critical("{}", trace_exception.what());
    std::terminate();
    //throw TraceException(LocationToString(location), message);
  }
}

template<typename Value>
inline void throw_if_empty(const std::unique_ptr<Value> &to_check,
                           const std::string &message,
                           const source_loc &location) {
  throw_if_empty(to_check, message.c_str(), location);
}

template<typename Value>
inline void throw_if_empty(const std::unique_ptr<Value> &to_check,
                           const std::string &&message,
                           const source_loc location) {
  throw_if_empty(to_check, message.c_str(), location);
}

template<typename Value>
inline void throw_if_empty(const Value *to_check,
                           const char *message,
                           const source_loc &location) {
  if (not to_check) {
    const TraceException trace_exception{LocationToString(location), message};
    spdlog::critical("{}", trace_exception.what());
    std::terminate();
    // throw TraceException(LocationToString(location), message);
  }
}

template<typename Value>
inline void throw_if_empty(const Value *to_check, const std::string &message, const source_loc &location) {
  throw_if_empty(to_check, message.c_str(), location);
}

template<typename Value>
inline void throw_if_empty(const Value *to_check, const std::string &&message, const source_loc &location) {
  throw_if_empty(to_check, message.c_str(), location);
}

inline void throw_on(bool should_throw, const char *message, const source_loc &location) {
  if (should_throw) {
    const TraceException trace_exception{LocationToString(location), message};
    spdlog::critical("{}", trace_exception.what());
    std::terminate();
    //throw trace_exception;
  }
}

inline void throw_on(bool should_throw, const std::string &message, const source_loc &location) {
  throw_on(should_throw, message.c_str(), location);
}

inline void throw_on(bool should_throw, const std::string &&message, const source_loc &location) {
  throw_on(should_throw, message.c_str(), location);
}

inline void throw_on_false(bool should_throw, const char *message, const source_loc &location) {
  throw_on(not should_throw, message, location);
}

inline void throw_on_false(bool should_throw, const std::string &message, const source_loc &location) {
  throw_on(not should_throw, message.c_str(), location);
}

inline void throw_on_false(bool should_throw, const std::string &&message, const source_loc &location) {
  throw_on(not should_throw, message.c_str(), location);
}

template<typename ValueType>
ValueType OrElseThrow(std::optional<ValueType> &val_opt, const char *message, const source_loc &location) {
  throw_on(not val_opt.has_value(), message, location);
  return *val_opt;
}

template<typename ValueType>
ValueType OrElseThrow(std::optional<ValueType> &val_opt, const std::string &message, const source_loc &location) {
  return OrElseThrow(val_opt, message.c_str(), location);
}

template<typename ValueType>
ValueType OrElseThrow(std::optional<ValueType> &val_opt, const std::string &&message, const source_loc &location) {
  return OrElseThrow(val_opt, message.c_str(), location);
}

template<typename ValueType>
ValueType OrElseThrow(std::optional<ValueType> &&val_opt, const std::string &&message, const source_loc &location) {
  return OrElseThrow(val_opt, message.c_str(), location);
}

template<typename ...Args>
inline void throw_just(const source_loc &location, Args &&... args) {
  std::stringstream message_builder;
  ([&] {
    message_builder << args;
  }(), ...);
  std::cout << "exception thrown" << std::endl;
  const std::string message{message_builder.str()};
  const TraceException trace_exception{LocationToString(location), message};
  spdlog::critical("{}", trace_exception.what());
  std::terminate();
  // throw TraceException(LocationToString(location), message);
}

#endif //SIMBRICKS_TRACE_EXCEPTION_H_
