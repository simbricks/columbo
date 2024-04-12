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

#ifndef SIMBRICKS_TRACE_COMP_H_
#define SIMBRICKS_TRACE_COMP_H_

#include <set>
#include <string>

class ComponentFilter {
 private:
  const std::string &identifier_;
  std::set<std::string> component_table_;

 public:
  explicit ComponentFilter(const std::string &identifier)
      : identifier_(identifier){};

  inline ComponentFilter &operator()(const std::string &symbol) {
    component_table_.insert(symbol);
    return *this;
  }

  inline const std::set<std::string> &get_component_table() {
    return component_table_;
  }

  bool filter(const std::string &comp) const {
    if (component_table_.empty())
        return true;
        
    auto found = component_table_.find(comp);
    return found != component_table_.end();
  }
 
  inline const std::string &get_ident() {
    return identifier_;
  }
};

#endif  // SIMBRICKS_TRACE_COMP_H_