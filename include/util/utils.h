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

#include <string>
#include <chrono>
#include <filesystem>
#include <exception>
#include <fstream>

#include "concepts.h"

#ifndef SIMBRICKS_TRACE_UTILS_H_
#define SIMBRICKS_TRACE_UTILS_H_

inline void
CreateOpenFile(std::ofstream &out, const std::string &filename, bool allow_override) {
  if (not allow_override and std::filesystem::exists(filename)) {
    std::stringstream error;
    error << "the file " << filename << " already exists, we will not overwrite it";
    throw std::runtime_error(error.str());
  }

  out.open(filename, std::ios::out);
  if (not out.is_open()) {
    std::stringstream error;
    error << "could not open file " << filename;
    throw std::runtime_error(error.str());
  }
}

inline int64_t GetNowOffsetNanoseconds() {
  auto now = std::chrono::system_clock::now();
  auto now_ns = std::chrono::time_point_cast<std::chrono::nanoseconds>(now);
  auto value = now_ns.time_since_epoch();
  return value.count();
}

inline std::string BoolToString(bool boo) {
  return boo ? "true" : "false";
}

inline void WriteIdent(std::ostream &out, unsigned ident) {
  if (ident == 0)
    return;

  for (size_t i = 0; i < ident; i++) {
    out << "\t";
  }
}

template<typename ObjT>
inline std::ostream &operator<<(std::ostream &out, const std::shared_ptr<ObjT> &to_write) {
  if (to_write) {
    out << *to_write;
  } else {
    out << "null";
  }
  return out;
}

template<size_t PageSize = 4096> requires SizeLagerZero<PageSize>
constexpr size_t MultiplePagesBytes(size_t times) {
  return times * PageSize;
}

#endif //SIMBRICKS_TRACE_UTILS_H_
