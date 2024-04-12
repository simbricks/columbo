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

#include "config/config.h"
#include "events/eventType.h"
#include "env/symtable.h"

TEST_CASE("Test TraceEnvConfig", "[TraceEnvConfig]") {

  const std::string config_path = "tests/trace-env-config.yaml";
  TraceEnvConfig trace_env_config;

  REQUIRE_NOTHROW(trace_env_config = TraceEnvConfig::CreateFromYaml(config_path));
  REQUIRE(trace_env_config.GetMaxBackgroundThreads() == 1);
  REQUIRE(trace_env_config.GetMaxCpuThreads() == 2);
  const std::string jaeger_url = "http://jaeger:4318/v1/traces";
  REQUIRE(jaeger_url == trace_env_config.GetJaegerUrl());
  REQUIRE(trace_env_config.GetLineBufferSize() == 1);
  REQUIRE(trace_env_config.GetEventBufferSize() == 60000000);

  const std::set<std::string> driver_func_indi{trace_env_config.BeginDriverFunc(), trace_env_config.EndDriverFunc()};
  REQUIRE(driver_func_indi.size() == 2);
  REQUIRE(driver_func_indi.contains("i40e_lan_xmit_frame"));
  REQUIRE(driver_func_indi.contains("i40e_napi_poll"));

  const std::vector<std::string> kernel_tx_indi{trace_env_config.BeginKernelTx(), trace_env_config.EndKernelTx()};
  REQUIRE(kernel_tx_indi.size() == 1);
  REQUIRE(kernel_tx_indi[0] == "dev_queue_xmit");

  const std::vector<std::string> kernel_rx_indi{trace_env_config.BeginKernelRx(), trace_env_config.EndKernelRx()};
  REQUIRE(kernel_rx_indi.size() == 1);
  REQUIRE(kernel_rx_indi[0] == "ip_list_rcv");

  const std::vector<std::string> pci_write_indi{trace_env_config.BeginPciWrite(), trace_env_config.EndPciWrite()};
  REQUIRE(pci_write_indi.size() == 1);
  REQUIRE(pci_write_indi[0] == "pci_msix_write_vector_ctrl");

  const std::vector<std::string> driver_tx_indi{trace_env_config.BeginDriverTx(), trace_env_config.EndDriverTx()};
  REQUIRE(driver_tx_indi.size() == 1);
  REQUIRE(driver_tx_indi[0] == "i40e_lan_xmit_frame");

  const std::vector<std::string> driver_rx_indi{trace_env_config.BeginDriverRx(), trace_env_config.EndDriverRx()};
  REQUIRE(driver_rx_indi.size() == 1);
  REQUIRE(driver_rx_indi[0] == "i40e_napi_poll");

  const std::vector<std::string>
      blacklist_func_indi{trace_env_config.BeginBlacklistFuncIndicator(), trace_env_config.EndBlacklistFuncIndicator()};
  REQUIRE(blacklist_func_indi.size() == 1);
  REQUIRE(blacklist_func_indi[0] == "sjkdgfkdsjgfjk");

  const std::vector<EventType>
      event_types_to_filter{trace_env_config.BeginTypesToFilter(), trace_env_config.EndTypesToFilter()};
  REQUIRE(event_types_to_filter.size() == 2);
  REQUIRE(event_types_to_filter[0] == EventType::kHostMmioCRT);
  REQUIRE(event_types_to_filter[1] == EventType::kHostMmioCWT);

  const std::set<std::string>
      linux_func_indi{trace_env_config.BeginFuncIndicator(), trace_env_config.EndFuncIndicator()};
  REQUIRE(linux_func_indi.size() == 5);
  REQUIRE(linux_func_indi.contains("entry_SYSCALL_64"));
  REQUIRE(linux_func_indi.contains("netdev_start_xmit"));
  REQUIRE_FALSE(linux_func_indi.contains("i40e_napi_poll"));
  REQUIRE_FALSE(linux_func_indi.contains("i40e_lan_xmit_frame"));

  const std::vector<TraceEnvConfig::SymTableConf>
      sym_table_confs{trace_env_config.BeginSymbolTables(), trace_env_config.EndSymbolTables()};
  REQUIRE(sym_table_confs.size() == 2);
  const TraceEnvConfig::SymTableConf sym_a = sym_table_confs[0];
  REQUIRE(sym_a.GetIdentifier() == "Linuxvm-Symbols");
  REQUIRE(sym_a.GetFilePath() == "tests/linux_dumps/vmlinux-image-syms.dump");
  REQUIRE(sym_a.GetAddressOffset() == 0);
  REQUIRE(sym_a.GetFilterType() == FilterType::kSyms);
  const TraceEnvConfig::SymTableConf sym_b = sym_table_confs[1];
  REQUIRE(sym_b.GetIdentifier() == "Nicdriver-Symbols");
  REQUIRE(sym_b.GetFilePath() == "tests/linux_dumps/i40e-image-syms.dump");
  REQUIRE(sym_b.GetAddressOffset() == 0xffffffffa0000000ULL);
  REQUIRE(sym_b.GetFilterType() == FilterType::kSyms);
}
