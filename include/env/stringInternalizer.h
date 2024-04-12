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

#ifndef SIM_TRACE_STRING_INTERNALIZER_H_
#define SIM_TRACE_STRING_INTERNALIZER_H_

#include <string>
#include <unordered_set>
#include <iostream>

class StringInternalizer {
  std::unordered_set<std::string> symbol_set_;

 public:
  StringInternalizer() = default;
  
  ~StringInternalizer() = default;

  const std::string * Internalize(const std::string& symbol) {
    const auto& it_b = symbol_set_.emplace(symbol);
    return std::addressof(*it_b.first);
  }

  const std::string * Internalize(const char *symbol) {
    const auto& it_b = symbol_set_.emplace(symbol);
    return std::addressof(*it_b.first);
  }

  void Display(std::ostream &os) {
    os << std::endl;
    os << std::endl;
    os << "StringInternalizer: " << std::endl;
    for (const std::string &internal : symbol_set_) {
      os <<  internal << std::endl;
    }
  }
};

inline std::ostream &operator<<(std::ostream &os, StringInternalizer &internalizer) {
  internalizer.Display(os);
  return os;
}

#endif // SIM_TRACE_STRING_INTERNALIZER_H_
