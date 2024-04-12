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

#include "util/concepts.h"
#include "util/exception.h"
#include "util/string_util.h"

#include <string>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <utility>
#include <fcntl.h>

#ifndef SIMBRICKS_TRACE_CREADER_H_
#define SIMBRICKS_TRACE_CREADER_H_

class LineHandler {
  char *buf_;
  size_t size_;
  size_t cur_reading_pos_ = 0;

 public:
  explicit LineHandler(char *buf, size_t size) : buf_(buf), size_(size) {
    Reset(buf, size_);
  };

  void Reset(char *buf, size_t size) {
    throw_if_empty(buf_, "nullpointer to current line", source_loc::current());
    buf_ = buf;
    size_ = size;
    cur_reading_pos_ = 0;
  }

  inline std::string GetRawLine() {
    return {buf_, buf_ + size_};
  }

  inline void ResetPos() {
    cur_reading_pos_ = 0;
  }

  inline std::string GetCurString() {
    if (cur_reading_pos_ >= size_) {
      return "";
    }
    return {buf_ + cur_reading_pos_, buf_ + size_};
  }

  [[nodiscard]] inline size_t CurLength() const {
    return size_ - cur_reading_pos_;
  }

  [[nodiscard]] inline bool IsEmpty() const {
    return CurLength() <= 0;
  }

  bool MoveForward(size_t steps);

  void TrimL();

  void TrimTillWhitespace();

  std::string ExtractAndSubstrUntil(
      std::function<bool(unsigned char)> &predicate);

  bool ExtractAndSubstrUntilInto(
      std::string &target, std::function<bool(unsigned char)> &predicate) {
    target = ExtractAndSubstrUntil(predicate);
    return not target.empty();
  }

  bool SkipTill(std::function<bool(unsigned char)> &predicate);

  bool SkipTillWhitespace() {
    return SkipTill(sim_string_utils::is_space);
  }

  bool ConsumeAndTrimTillString(const std::string &to_consume);

  bool ConsumeAndTrimString(const std::string &to_consume);

  bool ConsumeAndTrimChar(char to_consume);

  bool ParseUintTrim(int base, uint64_t &target);

  bool ParseInt(int &target);

  bool ParseBoolFromUint(int base, bool &target);

  bool ParseBoolFromStringRepr(bool &target);

  bool ParseBoolFromInt(bool &target);
};

template<size_t BlockSize = 4 * 1024> requires SizeLagerZero<BlockSize>
class ReaderBuffer {

  const std::string name_;

  std::string cur_file_path_;
  FILE *file_ = nullptr;
  static constexpr char kLineEnd = '\n';
  char buffer_[BlockSize]{};
  size_t cur_reading_pos_ = 0;
  size_t size_ = 0;
  size_t next_line_end_ = 0;
  int reached_eof_ = 2;
  LineHandler line_handler_{buffer_, 0};

  [[nodiscard]] inline bool IsStreamStillGood() const {
    int file_descriptor = open(cur_file_path_.c_str(), O_RDONLY | O_NONBLOCK);
    close(file_descriptor);
    return file_descriptor != -1;
  }

  [[nodiscard]] inline int GetValidFileDescriptorCurFile() const {
    int fd = fileno(file_);
    if (fd == -1) {
      spdlog::warn("{}: invalid file descriptor from file handel, errno={}", name_, errno);
      throw_just(source_loc::current(), "invalid file descriptor from file handel");
    }
    return fd;
  }

  void NextBlock() {
    assert(IsStreamStillGood());
    if (reached_eof_ >= 2) {
      return;
    }

    if (cur_reading_pos_ != size_) {
      memmove(buffer_, buffer_ + cur_reading_pos_, size_ - cur_reading_pos_);
    }

    size_ = size_ - cur_reading_pos_;
    size_t amount_to_read = BlockSize - size_;
    assert(size_ + amount_to_read <= BlockSize);

    spdlog::trace("{}: try to read the next block from file {}", name_, cur_file_path_);

    int fd = GetValidFileDescriptorCurFile();
    int64_t actually_read;
    int64_t write_offset;
    do {
      write_offset = size_;
      spdlog::trace("{} try reading block of size {}", name_, amount_to_read);
      actually_read = read(fd, buffer_ + write_offset, amount_to_read);
      spdlog::trace("{} read block of size {}", name_, amount_to_read);
      if (actually_read < 0) {
        spdlog::warn("{}: reading returned with an error, errno={}", name_, errno);
        throw_just(source_loc::current(), "file/pipe reading error occured");
      }
      size_ += actually_read;
      amount_to_read -= actually_read;
      cur_reading_pos_ = 0;
      next_line_end_ = 0;
    } while (FindLineEnd(0, size_) < 0 and amount_to_read > 0 and actually_read > 0);

    spdlog::trace("{}: read the next block", name_);
    cur_reading_pos_ = 0;
    next_line_end_ = 0;
    assert(size_ <= BlockSize);
  }

  int64_t FindLineEnd(int64_t start, size_t end) {
    throw_on(start < 0, "invalid find end start", source_loc::current());
    for (; start < end; start++) {
      if (buffer_[start] == kLineEnd) {
        if (start == cur_reading_pos_) {
          ++cur_reading_pos_;
          continue;
        }
        return start;
      }
    }
    return -1;
  }

  void CalculateNextLineEnd() {
    if (next_line_end_ > 0) {
      return;
    }

    int64_t tmp = FindLineEnd(cur_reading_pos_, size_);
    if (tmp > 0) {
      next_line_end_ = tmp;
    } else {
      if (std::feof(file_)) {
        next_line_end_ = size_;
        ++reached_eof_;
        spdlog::trace("{} found feof", name_);
        return;
      }
      next_line_end_ = 0;
    }
  }

  [[nodiscard]] bool HasStillLineEnd() {
    CalculateNextLineEnd();
    return reached_eof_ < 2
        and size_ > 0
        and cur_reading_pos_ < size_
        and next_line_end_ > 0
        and cur_reading_pos_ < next_line_end_;
  }

  inline void Close() {
    if (file_) {
      fclose(file_);
      file_ = nullptr;
    }
  }

 public:
  explicit ReaderBuffer(std::string name)
      : name_(std::move(name)) {
  }

  ~ReaderBuffer() {
    Close();
  }

  [[nodiscard]] inline bool IsOpen() const {
    return file_ != nullptr and IsStreamStillGood();
  }

  [[nodiscard]] bool HasStillLine() {
    while (cur_reading_pos_ < size_ and buffer_[cur_reading_pos_] == kLineEnd) {
      ++cur_reading_pos_;
      if (HasStillLineEnd()) {
        return true;
      }
    }

    if (HasStillLineEnd()) {
      return true;
    }
    if (not IsStreamStillGood()) {
      spdlog::trace("{}: input stream is no longer good!", name_);
      return false;
    }
    NextBlock();

    return HasStillLineEnd();
  }

  std::pair<bool, LineHandler *> NextHandler() {
    if (not HasStillLine()) {
      spdlog::trace("{}: no line is left", name_);
      return {false, nullptr};
    }

    line_handler_.Reset(buffer_ + cur_reading_pos_, next_line_end_ - cur_reading_pos_);
    cur_reading_pos_ = next_line_end_ + 1;
    next_line_end_ = 0;

    return {true, &line_handler_};
  }

  void OpenFile(const std::string &file_path, bool is_named_pipe = false) {
    cur_file_path_ = file_path;
    if (!std::filesystem::exists(file_path)) {
      throw_just(source_loc::current(),
                 "ReaderBuffer: the file path'", file_path, "' does not exist");
    }
    throw_on(file_, "ReaderBuffer:OpenFile: already opened file to read",
             source_loc::current());

    spdlog::debug("try open file path: {}", file_path);
    file_ = fopen(file_path.c_str(), "r");
    throw_if_empty(file_, "ReaderBuffer: could not open file path", source_loc::current());
    reached_eof_ = 0;

    if (is_named_pipe) {
      const int file_descriptor = GetValidFileDescriptorCurFile();
      const int suc = fcntl(file_descriptor, F_SETPIPE_SZ, BlockSize);
      if (suc != BlockSize) {
        spdlog::warn("ReaderBuffer: could not change '{}' size to {}, returned size is {} {}",
                     file_path,
                     BlockSize,
                     suc,
                     errno);
      } else {
        spdlog::debug("ReaderBuffer: changed size successfully");
      }
    }
    spdlog::debug("successfully opened file path: {}", file_path);
  }

};

#endif // SIMBRICKS_TRACE_CREADER_H_
