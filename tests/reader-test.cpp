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

#include <catch2/catch_all.hpp>

#include "sync/corobelt.h"
#include "reader/cReader.h"

TEST_CASE("Test CLineReader", "[CLineReader]") {
  spdlog::set_level(spdlog::level::trace);
  auto concurren_options = concurrencpp::runtime_options();
  concurren_options.max_background_threads = 1;
  concurren_options.max_cpu_threads = 1;
  const concurrencpp::runtime runtime{concurren_options};

  //CFileLineBufferReader file_line_buffer{"test-reader-buffer"};
  ReaderBuffer<4096> file_line_buffer{"test-reader-buffer"};

  int int_target;
  uint64_t hex_target;
  uint64_t timestamp, addr;
  //std::pair<bool, CLineHandler *> bh_p;
  std::pair<bool, LineHandler *> bh_p;

  REQUIRE_NOTHROW(file_line_buffer.OpenFile("tests/line-reader-test-files/simple.txt"));

  REQUIRE(file_line_buffer.HasStillLine());
  REQUIRE_NOTHROW(bh_p = file_line_buffer.NextHandler());
  REQUIRE(bh_p.first);
  //CLineHandler &line_handler = *bh_p.second;
  LineHandler &line_handler = *bh_p.second;
  REQUIRE(line_handler.ParseInt(int_target));
  REQUIRE(int_target == 10);
  REQUIRE(line_handler.ConsumeAndTrimChar(' '));
  REQUIRE(line_handler.ConsumeAndTrimString("Hallo"));
  REQUIRE(line_handler.ConsumeAndTrimChar(' '));
  REQUIRE(line_handler.ParseInt(int_target));
  REQUIRE(int_target == 327846378);

  REQUIRE(file_line_buffer.HasStillLine());
  REQUIRE_NOTHROW(bh_p = file_line_buffer.NextHandler());
  REQUIRE(bh_p.first);
  line_handler = *bh_p.second;
  REQUIRE(line_handler.ConsumeAndTrimTillString("0x"));
  REQUIRE(line_handler.ParseUintTrim(16, hex_target));
  REQUIRE(hex_target == 0x23645);

  REQUIRE(file_line_buffer.HasStillLine());
  REQUIRE_NOTHROW(bh_p = file_line_buffer.NextHandler());
  REQUIRE(bh_p.first);
  line_handler = *bh_p.second;
  REQUIRE_FALSE(line_handler.ConsumeAndTrimTillString("ks"));

  REQUIRE(file_line_buffer.HasStillLine());
  REQUIRE_NOTHROW(bh_p = file_line_buffer.NextHandler());
  REQUIRE(bh_p.first);
  line_handler = *bh_p.second;
  REQUIRE_FALSE(line_handler.IsEmpty());
  REQUIRE(line_handler.ConsumeAndTrimString("Rathaus"));

  REQUIRE(file_line_buffer.HasStillLine());
  REQUIRE_NOTHROW(bh_p = file_line_buffer.NextHandler());
  REQUIRE(bh_p.first);
  line_handler = *bh_p.second;
  REQUIRE(line_handler.ConsumeAndTrimTillString("Rathaus"));

  REQUIRE(file_line_buffer.HasStillLine());
  REQUIRE_NOTHROW(bh_p = file_line_buffer.NextHandler());
  REQUIRE(bh_p.first);
  line_handler = *bh_p.second;
  REQUIRE(line_handler.ConsumeAndTrimTillString("Rathaus"));

  REQUIRE(file_line_buffer.HasStillLine());
  REQUIRE_NOTHROW(bh_p = file_line_buffer.NextHandler());
  REQUIRE(bh_p.first);
  line_handler = *bh_p.second;
  REQUIRE(line_handler.ConsumeAndTrimTillString("Rathaus"));

  REQUIRE(file_line_buffer.HasStillLine());
  REQUIRE_NOTHROW(bh_p = file_line_buffer.NextHandler());
  REQUIRE(bh_p.first);
  line_handler = *bh_p.second;
  REQUIRE(line_handler.ConsumeAndTrimTillString("Rathaus"));

  // 1710532120875: system.switch_cpus: T0 : 0xffffffff814cf3c2    : mov        rax, GS:[0x1ac00]
  REQUIRE(file_line_buffer.HasStillLine());
  REQUIRE_NOTHROW(bh_p = file_line_buffer.NextHandler());
  REQUIRE(bh_p.first);
  line_handler = *bh_p.second;
  REQUIRE(line_handler.ParseUintTrim(10, timestamp));
  REQUIRE(line_handler.ConsumeAndTrimChar(':'));
  line_handler.TrimL();
  REQUIRE(line_handler.ConsumeAndTrimString("system.switch_cpus:"));
  REQUIRE(line_handler.ConsumeAndTrimTillString("0x"));
  REQUIRE(line_handler.ParseUintTrim(16, addr));
  line_handler.TrimL();
  REQUIRE(line_handler.ConsumeAndTrimChar(':'));
  REQUIRE(1710532120875 == timestamp);
  REQUIRE(0xffffffff814cf3c2 == addr);

  // 1710532121125: system.switch_cpus: T0 : 0xffffffff814cf3cb    : cmpxchg    DS:[rdi], rdx
  REQUIRE(file_line_buffer.HasStillLine());
  REQUIRE_NOTHROW(bh_p = file_line_buffer.NextHandler());
  REQUIRE(bh_p.first);
  line_handler = *bh_p.second;
  REQUIRE(line_handler.ParseUintTrim(10, timestamp));
  REQUIRE(line_handler.ConsumeAndTrimChar(':'));
  line_handler.TrimL();
  REQUIRE(line_handler.ConsumeAndTrimString("system.switch_cpus:"));
  REQUIRE(line_handler.ConsumeAndTrimTillString("0x"));
  REQUIRE(line_handler.ParseUintTrim(16, addr));
  line_handler.TrimL();
  REQUIRE(line_handler.ConsumeAndTrimChar(':'));
  REQUIRE(1710532121125 == timestamp);
  REQUIRE(0xffffffff814cf3cb == addr);

  // 1710969526625: system.switch_cpus: T0 : 0xffffffff81088093    : add     rax, GS:[rip + 0x7ef8d4d5]
  REQUIRE(file_line_buffer.HasStillLine());
  REQUIRE_NOTHROW(bh_p = file_line_buffer.NextHandler());
  REQUIRE(bh_p.first);
  line_handler = *bh_p.second;
  REQUIRE(line_handler.ParseUintTrim(10, timestamp));
  REQUIRE(line_handler.ConsumeAndTrimChar(':'));
  line_handler.TrimL();
  REQUIRE(line_handler.ConsumeAndTrimString("system.switch_cpus:"));
  REQUIRE(line_handler.ConsumeAndTrimTillString("0x"));
  REQUIRE(line_handler.ParseUintTrim(16, addr));
  line_handler.TrimL();
  REQUIRE(line_handler.ConsumeAndTrimChar(':'));
  REQUIRE(1710969526625 == timestamp);
  REQUIRE(0xffffffff81088093 == addr);

  // 1710532121125: system.switch_cpus: T0 : 0xffffffff814cf3cb. 0 :   CMPXCHG_M_R : ldst   t1, DS:[rdi] : MemRead :  D=0xfff
  REQUIRE(file_line_buffer.HasStillLine());
  REQUIRE_NOTHROW(bh_p = file_line_buffer.NextHandler());
  REQUIRE(bh_p.first);
  line_handler = *bh_p.second;
  REQUIRE(line_handler.ParseUintTrim(10, timestamp));
  REQUIRE(line_handler.ConsumeAndTrimChar(':'));
  line_handler.TrimL();
  REQUIRE(line_handler.ConsumeAndTrimString("system.switch_cpus:"));
  REQUIRE(line_handler.ConsumeAndTrimTillString("0x"));
  REQUIRE(line_handler.ParseUintTrim(16, addr));
  REQUIRE(line_handler.ConsumeAndTrimChar('.'));
  REQUIRE(1710532121125 == timestamp);
  REQUIRE(0xffffffff814cf3cb == addr);

  // 1710532121250: system.switch_cpus: T0 : 0xffffffff814cf3cb. 1 :   CMPXCHG_M_R : sub   t0, rax, t1 : IntAlu :  D=0x0000000000
  REQUIRE(file_line_buffer.HasStillLine());
  REQUIRE_NOTHROW(bh_p = file_line_buffer.NextHandler());
  REQUIRE(bh_p.first);
  line_handler = *bh_p.second;
  REQUIRE(line_handler.ParseUintTrim(10, timestamp));
  REQUIRE(line_handler.ConsumeAndTrimChar(':'));
  line_handler.TrimL();
  REQUIRE(line_handler.ConsumeAndTrimString("system.switch_cpus:"));
  REQUIRE(line_handler.ConsumeAndTrimTillString("0x"));
  REQUIRE(line_handler.ParseUintTrim(16, addr));
  REQUIRE(line_handler.ConsumeAndTrimChar('.'));
  REQUIRE(1710532121250 == timestamp);
  REQUIRE(0xffffffff814cf3cb == addr);

  REQUIRE_FALSE(file_line_buffer.HasStillLine());
  REQUIRE_NOTHROW(bh_p = file_line_buffer.NextHandler());
  REQUIRE_FALSE(bh_p.first);
}
