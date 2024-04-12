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

#include <memory>
#include "exception.h"

#ifndef SIMBRICKS_TRACE_INCLUDE_UTIL_FACTORY_H_
#define SIMBRICKS_TRACE_INCLUDE_UTIL_FACTORY_H_

template<class T, typename ...Args>
std::shared_ptr<T> create_shared(const char *error_msg, Args &&... args) {
  auto result = std::make_shared<T>(std::forward<Args>(args)...);
  throw_if_empty(result, error_msg, source_loc::current());
  return result;
}

template<class T, typename ...Args>
std::shared_ptr<T> create_shared(const std::string &error_msg, Args &&... args) {
  auto result = std::make_shared<T>(std::forward<Args>(args)...);
  throw_if_empty(result, error_msg, source_loc::current());
  return result;
}

template<typename T>
std::shared_ptr<T> copy_shared(const char *error_msg, std::shared_ptr<T> &other) {
  auto result = std::make_shared<T>(*other);
  throw_if_empty(result, error_msg, source_loc::current());
  return result;
}

template<class T, typename ...Args>
std::unique_ptr<T> create_unique(const char *error_msg, Args &&... args) {
  auto result = std::make_unique<T>(std::forward<Args>(args)...);
  throw_if_empty(result, error_msg, source_loc::current());
  return result;
}

template<class T, typename ...Args>
std::unique_ptr<T> create_unique(const std::string &error_msg, Args &&... args) {
  auto result = std::make_unique<T>(std::forward<Args>(args)...);
  throw_if_empty(result, error_msg, source_loc::current());
  return result;
}

template<typename T>
std::unique_ptr<T> copy_unique(const char *error_msg, std::unique_ptr<T> &other) {
  auto result = std::make_unique<T>(*other);
  throw_if_empty(result, error_msg, source_loc::current());
  return result;
}

template<class T, typename ...Args>
T *create_raw(const char *error_msg, Args &&... args) {
  auto result = new T(std::forward<Args>(args)...);
  throw_if_empty(result, error_msg, source_loc::current());
  return result;
}

template<class T, typename ...Args>
T *create_raw(const std::string &error_msg, Args &&... args) {
  auto result = new T(std::forward<Args>(args)...);
  throw_if_empty(result, error_msg, source_loc::current());
  return result;
}

template<typename T>
T *copy_raw(const char *error_msg, T *other) {
  auto result = new T(*other);
  throw_if_empty(result, error_msg, source_loc::current());
  return result;
}

#endif //SIMBRICKS_TRACE_INCLUDE_UTIL_FACTORY_H_
