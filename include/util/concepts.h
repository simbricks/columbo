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

#include <cstddef>
#include <iostream>

#ifndef SIMBRICKS_TRACE_CONCEPTS_H_
#define SIMBRICKS_TRACE_CONCEPTS_H_

template<size_t Size>
concept SizeLagerZero =  Size > 0;

template<typename Type>
concept ContextInterface = requires(Type con)
{
  { con.HasParent() } -> std::convertible_to<bool>;
  { con.GetTraceId() } -> std::convertible_to<uint64_t>;
  { con.GetParentId() } -> std::convertible_to<uint64_t>;
  { con.GetParentStartingTs() } -> std::convertible_to<uint64_t>;
};

template<typename ToPrint>
concept Printable = requires(std::ostream &out, ToPrint to_print)
{
  out << to_print;
};

#endif //SIMBRICKS_TRACE_CONCEPTS_H_
