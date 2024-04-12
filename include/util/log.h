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

#ifndef SIMBRICKS_LOG_H_
#define SIMBRICKS_LOG_H_

#include <stdio.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>

namespace sim_log {

#define SIMLOG 1

enum StdTarget { to_err, to_out, to_file, to_pipe };

class Log;

using log_upt = std::unique_ptr<Log>;

class Log {
 private:
  Log(FILE *file, const StdTarget target) : file_(file), target_(target) {
  }

 public:
  std::mutex file_mutex_;
  FILE *file_;
  const StdTarget target_;

  ~Log() {
    file_mutex_.lock();
    if (file_ != nullptr &&
        (target_ == StdTarget::to_file || target_ == StdTarget::to_pipe)) {
      fclose(file_);
    }
    file_mutex_.unlock();
  }

  static log_upt createLog(StdTarget target) {
    FILE *out;
    if (target == sim_log::StdTarget::to_out) {
      out = stdout;
    } else {
      target = sim_log::StdTarget::to_err;
      out = stderr;
    }
    return sim_log::log_upt(new sim_log::Log{out, target});
  }

  static log_upt createLog(const char *file_path) {
    if (file_path == nullptr) {
      fprintf(stderr, "error: file_path is null, fallback to stderr logging\n");
      return sim_log::Log::createLog(sim_log::StdTarget::to_err);
    }

    FILE *file = fopen(file_path, "w");
    const bool is_pipe = std::filesystem::is_fifo(file_path);
    const StdTarget target = is_pipe ? StdTarget::to_pipe : StdTarget::to_file;
    if (file == nullptr) {
      fprintf(stderr, "error: cannot open file, fallback to stderr logging\n");
      return sim_log::Log::createLog(sim_log::StdTarget::to_err);
    }

    return sim_log::log_upt(new Log{file, target});
  }
};

class Logger {
 private:
  const char *prefix_;

  template<typename... Args>
  inline void log_internal(FILE *out, const char *format, Args... args) {
    fprintf(out, "%s", prefix_);
    fprintf(out, format, args...);
  }

  inline void log_internal(FILE *out, const char *to_print) {
    fprintf(out, "%s", prefix_);
    fprintf(out, "%s", to_print);
  }

  Logger(const char *prefix) : prefix_(prefix) {
  }

 public:
  static Logger &getInfoLogger() {
    static Logger logger("info: ");
    return logger;
  }

  static Logger &getErrorLogger() {
    static Logger logger("error: ");
    return logger;
  }

  static Logger &getWarnLogger() {
    static Logger logger("warn: ");
    return logger;
  }

  template<typename... Args>
  inline void log_stdout_f(const char *format, const Args &...args) {
    this->log_internal(stdout, format, args...);
  }

  template<typename... Args>
  inline void log_stderr_f(const char *format, const Args &...args) {
    this->log_internal(stderr, format, args...);
  }

  inline void log_stdout(const char *to_print) {
    this->log_internal(stdout, to_print);
  }

  inline void log_stderr(const char *to_print) {
    this->log_internal(stderr, to_print);
  }

  template<typename... Args>
  void log_f(log_upt &log, const char *format, const Args &...args) {
    if (log->file_ == nullptr) {
      this->log_stderr("log file is null. it should not be!\n");
      this->log_stderr_f(format, args...);
      return;
    }

    const StdTarget target = log->target_;
    if (target == StdTarget::to_file || target == StdTarget::to_pipe) {
      std::lock_guard<std::mutex>(log->file_mutex_);
      this->log_internal(log->file_, format, args...);
    } else if (log->target_ == StdTarget::to_out) {
      this->log_stdout_f(format, args...);
    } else {
      this->log_stderr_f(format, args...);
    }
  }

  void log(log_upt &log, const char *to_print) {
    if (log->file_ == nullptr) {
      this->log_stderr("log file is null. it should not be!\n");
      this->log_stderr(to_print);
      return;
    }

    const StdTarget target = log->target_;
    if (target == StdTarget::to_file || target == StdTarget::to_pipe) {
      std::lock_guard<std::mutex>(log->file_mutex_);
      this->log_internal(log->file_, to_print);
    } else if (log->target_ == StdTarget::to_out) {
      this->log_stdout(to_print);
    } else {
      this->log_stderr(to_print);
    }
  }
};

#ifdef SIMLOG

#define DFLOGINFLOG(l, fmt, ...)                                 \
  do {                                                           \
    sim_log::Logger::getInfoLogger().log_f(l, fmt, __VA_ARGS__); \
  } while (0);

#define DFLOGWARNLOG(l, fmt, ...)                                \
  do {                                                           \
    sim_log::Logger::getWarnLogger().log_f(l, fmt, __VA_ARGS__); \
  } while (0);

#define DFLOGERRLOG(l, fmt, ...)                                  \
  do {                                                            \
    sim_log::Logger::getErrorLogger().log_f(l, fmt, __VA_ARGS__); \
  } while (0);

#define DFLOGIN(fmt, ...)                                            \
  do {                                                               \
    sim_log::Logger::getInfoLogger().log_stdout_f(fmt, __VA_ARGS__); \
  } while (0);

#define DFLOGWARN(fmt, ...)                                          \
  do {                                                               \
    sim_log::Logger::getWarnLogger().log_stderr_f(fmt, __VA_ARGS__); \
  } while (0);

#define DFLOGERR(fmt, ...)                                            \
  do {                                                                \
    sim_log::Logger::getErrorLogger().log_stderr_f(fmt, __VA_ARGS__); \
  } while (0);

#define DLOGINFLOG(l, tp)                        \
  do {                                           \
    sim_log::Logger::getInfoLogger().log(l, tp); \
  } while (0);

#define DLOGWARNLOG(l, tp)                       \
  do {                                           \
    sim_log::Logger::getWarnLogger().log(l, tp); \
  } while (0);

#define DLOGERRLOG(l, tp)                         \
  do {                                            \
    sim_log::Logger::getErrorLogger().log(l, tp); \
  } while (0);

#define DLOGIN(tp)                                   \
  do {                                               \
    sim_log::Logger::getInfoLogger().log_stdout(tp); \
  } while (0);

#define DLOGWARN(tp)                                 \
  do {                                               \
    sim_log::Logger::getWarnLogger().log_stderr(tp); \
  } while (0);

#define DLOGERR(tp)                                   \
  do {                                                \
    sim_log::Logger::getErrorLogger().log_stderr(tp); \
  } while (0);

#else

#define DFLOGINFLOG(l, fmt, ...) \
  do {                           \
  } while (0)

#define DFLOGWARNLOG(l, fmt, ...) \
  do {                            \
  } while (0)

#define DFLOGERRLOG(l, fmt, ...) \
  do {                           \
  } while (0)

#define DFLOGIN(fmt, ...) \
  do {                    \
  } while (0)

#define DFLOGWARN(fmt, ...) \
  do {                      \
  } while (0)

#define DFLOGERR(fmt, ...) \
  do {                     \
  } while (0)

#define DLOGINFLOG(l, tp) \
  do {                    \
  } while (0)

#define DLOGWARNLOG(l, tp) \
  do {                     \
  } while (0)

#define DLOGERRLOG(l, tp) \
  do {                    \
  } while (0)

#define DLOGIN(tp) \
  do {             \
  } while (0)

#define DLOGWARN(tp) \
  do {               \
  } while (0)

#define DLOGERR(tp) \
  do {              \
  } while (0)

#endif

}  // namespace sim_log

#endif  // SIMBRICKS_LOG_H_
