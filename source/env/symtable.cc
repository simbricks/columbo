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

#include <functional>
#include <utility>

#include "env/symtable.h"

//#define SYMS_DEBUG_ 1
#ifdef SYMS_DEBUG_
#include "util/string_util.h"
#include <cassert>
#endif

bool SymsFilter::ParseAddress(LineHandler &line_handler, uint64_t &address) {
  line_handler.TrimL();

  if (!line_handler.ParseUintTrim(16, address)) {
#ifdef SYMS_DEBUG_
    DFLOGERR("%s: could not parse address out of line '%s'\n",
             component_.c_str(), line_reader_.get_raw_line().c_str());
#endif
    return false;
  }

  return true;
}

bool SymsFilter::ParseName(LineHandler &line_handler, std::string &name) {
  line_handler.TrimL();
  name =
      line_handler.ExtractAndSubstrUntil(sim_string_utils::is_alnum_dot_bar);

  if (name.empty()) {
#ifdef SYMS_DEBUG_
    DFLOGERR("%s: could not parse non empty name\n", component_.c_str());
#endif
    return false;
  }

  return true;
}

bool SymsFilter::AddToSymTable(uint64_t address, const std::string &name,
                               uint64_t address_offset) {
  auto in_set = symbol_filter_.find(name);
  if (!symbol_filter_.empty() && in_set == symbol_filter_.end()) {
#ifdef SYMS_DEBUG_
    DFLOGIN("%s: filter out symbol with name '%s'\n", component_.c_str(),
            name.c_str());
#endif
    return false;
  }

  const std::string *sym_ptr = i_.Internalize(name);
  auto pair =
      symbol_table_.insert(std::make_pair(address_offset + address, sym_ptr));
  if (!pair.second) {
#ifdef SYMS_DEBUG_
    DFLOGWARN("%s: could not insert new symbol table value at address '%u'\n",
              identifier_.c_str(), address);
#endif
    return false;
  }

  return true;
}

const std::string *SymsFilter::Filter(uint64_t address) {
  auto symbol = symbol_table_.find(address);
  if (symbol != symbol_table_.end()) {
    return symbol->second;
  }

  return {};
}

const std::string *SymsFilter::FilterNearestAddressUpper(uint64_t address) {
  auto symbol_iter = symbol_table_.upper_bound(address);
  if (symbol_iter == symbol_table_.begin()) {
    // the very first symbol is larger, hence we return null
    return {};
  }
  symbol_iter--;
  return symbol_iter->second;
}

const std::string *SymsFilter::FilterNearestAddressLower(uint64_t address) {
  auto symbol_iter = symbol_table_.lower_bound(address);
  if (symbol_iter->first == address) {
    return symbol_iter->second;
  }

  if (symbol_iter->first > address) {
    if (symbol_iter == symbol_table_.begin()) {
      return {};
    }
    symbol_iter--;
    return symbol_iter->second;
  }

  return {};
}

bool SymsFilter::SkipSymsFags(LineHandler &line_handler) {
  line_handler.TrimL();
  // flags are devided into 7 groups
  if (line_handler.CurLength() < 8) {
#ifdef SYMS_DEBUG_
    DFLOGWARN(
        "%s: line has not more than 7 chars (flags), hence it is the wrong "
        "format",
        component_.c_str());
#endif
    return false;
  }
  line_handler.MoveForward(7);
  return true;
}

bool SymsFilter::SkipSymsSection(LineHandler &line_handler) {
  line_handler.TrimL();
  line_handler.TrimTillWhitespace();
  return true;
}

bool SymsFilter::SkipSymsAlignment(LineHandler &line_handler) {
  line_handler.TrimL();
  line_handler.TrimTillWhitespace();
  return true;
}

bool SymsFilter::LoadSyms(const std::string &file_path,
                          uint64_t address_offset) {
  reader_buffer_.OpenFile(file_path);

  uint64_t address = 0;
  std::string name = "";
  while (reader_buffer_.HasStillLine()) {
    const std::pair<bool, LineHandler *> bh_p = reader_buffer_.NextHandler();
    if (not bh_p.first) {
      break;
    }
    LineHandler &line_handler = *bh_p.second;
    line_handler.TrimL();

    // parse address
    if (!ParseAddress(line_handler, address)) {
#ifdef SYMS_DEBUG_
      DFLOGWARN("%s: could not parse address from line '%s'\n",
                component_.c_str(), line_reader_.get_raw_line().c_str());
#endif
      continue;
    }

    // skip yet uninteresting values of ELF format
    if (!SkipSymsFags(line_handler) || !SkipSymsSection(line_handler) || !SkipSymsAlignment(line_handler)) {
#ifdef SYMS_DEBUG_
      DFLOGWARN(
          "%s: line '%s' seems to have wrong format regarding flags, section "
          "or alignment\n",
          component_.c_str(), line_reader_.get_raw_line().c_str());
#endif
      continue;
    }

    // parse name
    if (!ParseName(line_handler, name)) {
#ifdef SYMS_DEBUG_
      DFLOGWARN("%s: could not parse name from line '%s'\n", component_.c_str(),
                line_reader_.get_raw_line().c_str());
#endif
      continue;
    }

    if (!AddToSymTable(address, name, address_offset)) {
#ifdef SYMS_DEBUG_
      DFLOGWARN("%s: could not insert new val '[%u] = %s' into sym table.\n",
                component_.c_str(), address, name.c_str());
#endif
    }
  }
  return true;
}

bool SymsFilter::LoadS(const std::string &file_path, uint64_t address_offset) {
  reader_buffer_.OpenFile(file_path);

  uint64_t address = 0;
  std::string symbol;
  while (reader_buffer_.HasStillLine()) {
    const std::pair<bool, LineHandler *> bh_p = reader_buffer_.NextHandler();
    if (not bh_p.first) {
      break;
    }
    LineHandler &line_handler = *bh_p.second;
#ifdef SYMS_DEBUG_
    DFLOGIN("%s: found line: %s\n", component_.c_str(),
            line_reader_.get_raw_line().c_str());
#endif
    line_handler.TrimL();

    // parse address
    if (!ParseAddress(line_handler, address)) {
#ifdef SYMS_DEBUG_
      DFLOGWARN("%s: could not parse address from line '%s'\n",
                component_.c_str(), line_reader_.get_raw_line().c_str());
#endif
      continue;
    }

    if (!line_handler.ConsumeAndTrimString(" <") || !ParseName(line_handler, symbol) ||
        !line_handler.ConsumeAndTrimChar('>') ||
        !line_handler.ConsumeAndTrimChar(':')) {
#ifdef SYMS_DEBUG_
      DFLOGERR("%s: could not parse label from line '%s'\n", component_.c_str(),
               line_reader_.get_raw_line().c_str());
#endif
      continue;
    }

    if (!AddToSymTable(address, symbol, address_offset)) {
#ifdef SYMS_DEBUG_
      DFLOGWARN("%s: could not insert new val '[%u] = %s' into sym table\n",
                component_.c_str(), address, symbol.c_str());
#endif
    }
  }
  return true;
}

/*
Symbol table '.symtab' contains 72309 entries:
Num:    Value             Size  Type      Bind    Vis      Ndx  Name
0:      0000000000000000     0  NOTYPE    LOCAL   DEFAULT  UND
1:      ffffffff81000000     0  SECTION   LOCAL   DEFAULT    1
*/
bool SymsFilter::LoadElf(const std::string &file_path,
                         uint64_t address_offset) {
  reader_buffer_.OpenFile(file_path);

  // the first 3 lines do not contain interesting information
  for (int i = 0; i < 3 and reader_buffer_.HasStillLine(); i++) {
    reader_buffer_.NextHandler();
  }

  uint64_t address = 0;
  std::string label = "";
  while (reader_buffer_.HasStillLine()) {
    const std::pair<bool, LineHandler *> bh_p = reader_buffer_.NextHandler();
    if (not bh_p.first) {
      break;
    }
    LineHandler &line_handler = *bh_p.second;
    line_handler.TrimL();
    if (!line_handler.SkipTillWhitespace()) {  // Num
      continue;
    }

    // parse address
    if (!ParseAddress(line_handler, address)) {  // Value
#ifdef SYMS_DEBUG_
      DFLOGWARN("%s: could not parse address from line '%s'\n",
                component_.c_str(), line_reader_.get_raw_line().c_str());
#endif
      continue;
    }

    // skip yet uninteresting values of ELF format
    line_handler.TrimL();
    line_handler.SkipTillWhitespace();  // Size
    line_handler.TrimL();
    if (line_handler.ConsumeAndTrimString("FILE") ||
        line_handler.ConsumeAndTrimString(
            "OBJECT")) {  // no files/objects in table
      continue;
    } else {
      line_handler.SkipTillWhitespace();  // Type
    }
    line_handler.TrimL();
    line_handler.SkipTillWhitespace();  // Bind
    line_handler.TrimL();
    line_handler.SkipTillWhitespace();  // Vis
    line_handler.TrimL();
    line_handler.SkipTillWhitespace();  // Ndx
    line_handler.TrimL();

    // parse name
    if (!ParseName(line_handler, label)) {  // Name
#ifdef SYMS_DEBUG_
      DFLOGWARN("%s: could not parse name from line '%s'\n", component_.c_str(),
                line_reader_.get_raw_line().c_str());
#endif
      continue;
    }

    if (!AddToSymTable(address, label, address_offset)) {
#ifdef SYMS_DEBUG_
      DFLOGWARN("%s: could not insert new val '[%u] = %s' into sym table.\n",
                component_.c_str(), address, label.c_str());
#endif
    }
  }
  return true;
}

std::shared_ptr<SymsFilter> SymsFilter::Create(
    uint64_t id, const std::string component,
    const std::string &file_path,
    uint64_t address_offset, FilterType type,
    std::set<std::string> symbol_filter, StringInternalizer &i) {
  std::shared_ptr<SymsFilter> filter{
      new SymsFilter{id, component, std::move(symbol_filter), i}};
  if (not filter) {
    return nullptr;
  }

  switch (type) {
    case kS:
      if (not filter->LoadS(file_path, address_offset)) {
        return nullptr;
      }
      break;
    case kElf:
      if (not filter->LoadElf(file_path, address_offset)) {
        return nullptr;
      }
      break;
    case kSyms:
    default:
      if (not filter->LoadSyms(file_path, address_offset)) {
        return nullptr;
      }
      break;
  }

  return filter;
}
