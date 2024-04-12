/*
 * Copyright 2022 Max Planck Institute for Software Systems, and
 * National University of Singapore
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software withos restriction, including
 * withos limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHos WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, os OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <vector>
#include <string>
#include <set>

#include "spdlog/spdlog.h"
#include "concurrencpp/runtime/runtime.h"
#include "util/utils.h"
#include "util/exception.h"
#include "yaml-cpp/yaml.h"
#include "events/events.h"
#include "env/symtable.h"

#ifndef SIMBRICKS_TRACE_CONFIG_H_
#define SIMBRICKS_TRACE_CONFIG_H_

class TraceEnvConfig {
 public:
  using IndicatorType = std::string;
  using IndicatorContainer = std::set<IndicatorType>;
  using TypeContainer = std::set<EventType>;

  inline static void CheckKey(const char *key, const YAML::Node &node) {
    throw_on(not node[key],
             "TraceEnvConfig::CheckKeyAndType node doesnt contain specified key",
             source_loc::current());
  }

  template<YAML::NodeType::value Type>
  inline static void CheckKeyAndType(const char *key, const YAML::Node &node) {
    CheckKey(key, node);
    throw_on(node[key].Type() != Type,
             "TraceEnvConfig::CheckKeyAndType node at key doesnt have specified type",
             source_loc::current());
  }

  inline static void IterateAddValue(const YAML::Node &node, IndicatorContainer &container_to_fill) {
    for (const auto &index : node) {
      container_to_fill.insert(index.as<std::string>());
    }
  }

  inline static void IterateAddValue(const YAML::Node &node, TypeContainer &container_to_fill) {
    for (const auto &index : node) {
      const EventType type = EventTypeFromString(index.as<std::string>());
      container_to_fill.insert(type);
    }
  }

  inline static void CheckEmptiness(IndicatorContainer &container_to_check) {
    throw_on(container_to_check.empty(),
             "TraceEnvConfig::CheckEmptiness: the container to check is empty",
             source_loc::current());
  }

  inline static void CheckEmptiness(const std::string &to_check) {
    throw_on(to_check.empty(), "string to check is empty", source_loc::current());
  }

  inline static spdlog::level::level_enum ResolveLogLevel(const std::string &level_name) {
    auto level = spdlog::level::from_str(level_name);
    if (level == spdlog::level::off) {
      level = spdlog::level::info;
    }
    return level;
  }

  class SymTableConf {
   public:
    inline const std::string &GetIdentifier() const {
      return identifier_;
    }

    inline const std::string &GetFilePath() const {
      return path_;
    }

    inline uint64_t GetAddressOffset() const {
      return address_offset_;
    }

    inline FilterType GetFilterType() const {
      return filter_type_;
    }

    static SymTableConf CreateSymTableConf(const YAML::Node &node) {
      SymTableConf sym_conf;
      CheckKey(kIdentifierKey, node);
      sym_conf.identifier_ = node[kIdentifierKey].as<std::string>();
      CheckEmptiness(sym_conf.identifier_);

      CheckKey(kPathKey, node);
      sym_conf.path_ = node[kPathKey].as<std::string>();
      CheckEmptiness(sym_conf.path_);

      CheckKey(kAddressOffsetKey, node);
      sym_conf.address_offset_ = node[kAddressOffsetKey].as<uint64_t>();

      CheckKey(kFilterTypeKey, node);
      sym_conf.filter_type_ = FilterTypeFromString(node[kFilterTypeKey].as<std::string>());

      return sym_conf;
    }

   private:

    explicit SymTableConf() = default;
    constexpr static const char *kIdentifierKey{"Identifier"};

    std::string identifier_;
    constexpr static const char *kPathKey{"Path"};
    std::string path_;
    constexpr static const char *kAddressOffsetKey{"AddressOffset"};
    uint64_t address_offset_;
    constexpr static const char *kFilterTypeKey{"Type"};
    FilterType filter_type_;
  };

  using SymTableContainer = std::vector<SymTableConf>;

  inline static void IterateAddValue(const YAML::Node &node, SymTableContainer &container_to_fill) {
    for (const auto &index : node) {
      const SymTableConf sym_conf = SymTableConf::CreateSymTableConf(index);
      container_to_fill.push_back(sym_conf);
    }
  }

  inline static void CheckEmptiness(SymTableContainer &container_to_check) {
    throw_on(container_to_check.empty(),
             "TraceEnvConfig::CheckEmptiness: the container to check is empty",
             source_loc::current());
  }

  explicit TraceEnvConfig() = default;

  ~TraceEnvConfig() = default;

  inline static TraceEnvConfig CreateFromYaml(const std::string &config_path) {
    spdlog::debug("TraceEnvConfig start CreateFromYaml");

    TraceEnvConfig trace_config;

    YAML::Node config_root = YAML::LoadFile(config_path);

    CheckKeyAndType<YAML::NodeType::Sequence>(kLinuxNetFuncIndicatorKey, config_root);
    const YAML::Node net_func_ind = config_root[kLinuxNetFuncIndicatorKey];
    IterateAddValue(net_func_ind, trace_config.linux_net_func_indicator_);

    CheckKeyAndType<YAML::NodeType::Sequence>(kDriverFuncIndicatorKey, config_root);
    const YAML::Node driver_func_ind = config_root[kDriverFuncIndicatorKey];
    IterateAddValue(driver_func_ind, trace_config.driver_func_indicator_);

    CheckKeyAndType<YAML::NodeType::Sequence>(kErnelTxIndicatorKey, config_root);
    IterateAddValue(config_root[kErnelTxIndicatorKey], trace_config.kernel_tx_indicator_);
    IterateAddValue(config_root[kErnelTxIndicatorKey], trace_config.linux_net_func_indicator_);

    CheckKeyAndType<YAML::NodeType::Sequence>(kErnelRxIndicatorKey, config_root);
    IterateAddValue(config_root[kErnelRxIndicatorKey], trace_config.kernel_rx_indicator_);
    IterateAddValue(config_root[kErnelRxIndicatorKey], trace_config.linux_net_func_indicator_);

    CheckKeyAndType<YAML::NodeType::Sequence>(kPciWriteIndicatorsKey, config_root);
    IterateAddValue(config_root[kPciWriteIndicatorsKey], trace_config.pci_write_indicators_);
    IterateAddValue(config_root[kPciWriteIndicatorsKey], trace_config.linux_net_func_indicator_);

    CheckKeyAndType<YAML::NodeType::Sequence>(kDriverTxIndicatorKey, config_root);
    IterateAddValue(config_root[kDriverTxIndicatorKey], trace_config.driver_tx_indicator_);
    IterateAddValue(config_root[kDriverTxIndicatorKey], trace_config.driver_func_indicator_);

    CheckKeyAndType<YAML::NodeType::Sequence>(kDriverRxIndicatorKey, config_root);
    IterateAddValue(config_root[kDriverRxIndicatorKey], trace_config.driver_rx_indicator_);
    IterateAddValue(config_root[kDriverRxIndicatorKey], trace_config.driver_func_indicator_);

    CheckKeyAndType<YAML::NodeType::Sequence>(kSysEntryKey, config_root);
    IterateAddValue(config_root[kSysEntryKey], trace_config.sys_entry_);
    IterateAddValue(config_root[kSysEntryKey], trace_config.linux_net_func_indicator_);

    CheckKeyAndType<YAML::NodeType::Sequence>(kBlacklistFuncIndicatorKey, config_root);
    IterateAddValue(config_root[kBlacklistFuncIndicatorKey], trace_config.blacklist_func_indicator_);

    CheckKeyAndType<YAML::NodeType::Sequence>(kTypesToFilterKey, config_root);
    IterateAddValue(config_root[kTypesToFilterKey], trace_config.types_to_filter_);

    CheckKeyAndType<YAML::NodeType::Sequence>(kSymbolTablesKey, config_root);
    IterateAddValue(config_root[kSymbolTablesKey], trace_config.symbol_tables_);

    CheckEmptiness(trace_config.driver_tx_indicator_);
    CheckEmptiness(trace_config.sys_entry_);
    CheckEmptiness(trace_config.linux_net_func_indicator_);
    CheckEmptiness(trace_config.driver_func_indicator_);
    CheckEmptiness(trace_config.symbol_tables_);

    CheckKeyAndType<YAML::NodeType::Scalar>(kMaxBackgroundThreadsKey, config_root);
    trace_config.max_background_threads_ = config_root[kMaxBackgroundThreadsKey].as<size_t>();
    throw_on(trace_config.max_background_threads_ == 0,
             "TraceEnvConfig::Create: max_background_threads_ is 0",
             source_loc::current());

    CheckKeyAndType<YAML::NodeType::Scalar>(kMaxCpuThreadsKey, config_root);
    trace_config.max_cpu_threads_ = config_root[kMaxCpuThreadsKey].as<size_t>();
    throw_on(trace_config.max_cpu_threads_ == 0,
             "TraceEnvConfig::Create: max_cpu_threads_ is 0",
             source_loc::current());

    CheckKey(kJaegerUrlKey, config_root);
    trace_config.jaeger_url_ = config_root[kJaegerUrlKey].as<std::string>();
    throw_on(trace_config.jaeger_url_.empty(),
             "no jaeger url given",
             source_loc::current());

    CheckKeyAndType<YAML::NodeType::Scalar>(kLineBufferSizeKey, config_root);
    trace_config.line_buffer_size_ = config_root[kLineBufferSizeKey].as<size_t>();
    throw_on(trace_config.line_buffer_size_ == 0, "line buffer size 0", source_loc::current());

    CheckKeyAndType<YAML::NodeType::Scalar>(kEventBufferSize, config_root);
    trace_config.event_buffer_size_ = config_root[kEventBufferSize].as<size_t>();
    throw_on(trace_config.event_buffer_size_ == 0, "event buffer size 0", source_loc::current());

    CheckKey(kLogLevelKey, config_root);
    trace_config.log_level_ = ResolveLogLevel(config_root[kLogLevelKey].as<std::string>());

    spdlog::debug("TraceEnvConfig finished CreateFromYaml");

    return trace_config;
  }

  [[nodiscard]] inline IndicatorContainer::const_iterator BeginFuncIndicator() const {
    return linux_net_func_indicator_.cbegin();
  }

  [[nodiscard]] inline IndicatorContainer::const_iterator EndFuncIndicator() const {
    return linux_net_func_indicator_.cend();
  }

  [[nodiscard]] inline IndicatorContainer::const_iterator BeginDriverFunc() const {
    return driver_func_indicator_.cbegin();
  }

  [[nodiscard]] inline IndicatorContainer::const_iterator EndDriverFunc() const {
    return driver_func_indicator_.cend();
  }

  [[nodiscard]] inline IndicatorContainer::const_iterator BeginKernelTx() const {
    return kernel_tx_indicator_.cbegin();
  }

  [[nodiscard]] inline IndicatorContainer::const_iterator EndKernelTx() const {
    return kernel_tx_indicator_.cend();
  }

  [[nodiscard]] inline IndicatorContainer::const_iterator BeginKernelRx() const {
    return kernel_rx_indicator_.cbegin();
  }

  [[nodiscard]] inline IndicatorContainer::const_iterator EndKernelRx() const {
    return kernel_rx_indicator_.cend();
  }

  [[nodiscard]] inline IndicatorContainer::const_iterator BeginPciWrite() const {
    return pci_write_indicators_.cbegin();
  }

  [[nodiscard]] inline IndicatorContainer::const_iterator EndPciWrite() const {
    return pci_write_indicators_.cend();
  }

  [[nodiscard]] inline IndicatorContainer::const_iterator BeginDriverTx() const {
    return driver_tx_indicator_.cbegin();
  }

  [[nodiscard]] inline IndicatorContainer::const_iterator EndDriverTx() const {
    return driver_tx_indicator_.cend();
  }

  [[nodiscard]] inline IndicatorContainer::const_iterator BeginDriverRx() const {
    return driver_rx_indicator_.cbegin();
  }

  [[nodiscard]] inline IndicatorContainer::const_iterator EndDriverRx() const {
    return driver_rx_indicator_.cend();
  }

  [[nodiscard]] inline IndicatorContainer::const_iterator BeginSysEntry() const {
    return sys_entry_.cbegin();
  }

  [[nodiscard]] inline IndicatorContainer::const_iterator EndSysEntry() const {
    return sys_entry_.cend();
  }

  [[nodiscard]] inline IndicatorContainer::const_iterator BeginBlacklistFuncIndicator() const {
    return blacklist_func_indicator_.begin();
  }

  [[nodiscard]] inline IndicatorContainer::const_iterator EndBlacklistFuncIndicator() const {
    return blacklist_func_indicator_.cend();
  }

  [[nodiscard]] inline TypeContainer::const_iterator BeginTypesToFilter() const {
    return types_to_filter_.cbegin();
  }

  [[nodiscard]] inline TypeContainer::const_iterator EndTypesToFilter() const {
    return types_to_filter_.cend();
  }

  [[nodiscard]] inline SymTableContainer::const_iterator BeginSymbolTables() const {
    return symbol_tables_.cbegin();
  }

  [[nodiscard]] inline SymTableContainer::const_iterator EndSymbolTables() const {
    return symbol_tables_.cend();
  }

  [[nodiscard]] inline const std::string &GetJaegerUrl() const {
    return jaeger_url_;
  }

  [[nodiscard]] inline size_t GetMaxBackgroundThreads() const {
    return max_background_threads_;
  }

  [[nodiscard]] inline size_t GetMaxCpuThreads() const {
    return max_cpu_threads_;
  }

  [[nodiscard]] inline size_t GetLineBufferSize() const {
    return line_buffer_size_;
  }

  [[nodiscard]] inline size_t GetEventBufferSize() const {
    return event_buffer_size_;
  }

  [[nodiscard]] inline spdlog::level::level_enum GetLogLevel() const {
    return log_level_;
  };

  [[nodiscard]] concurrencpp::runtime_options GetRuntimeOptions() const {
    auto concurren_options = concurrencpp::runtime_options();
    concurren_options.max_cpu_threads = GetMaxCpuThreads();
    concurren_options.max_background_threads = GetMaxBackgroundThreads();
//    concurren_options.max_background_executor_waiting_time = std::chrono::milliseconds(6);
//    concurren_options.max_thread_pool_executor_waiting_time = std::chrono::milliseconds(6);
    return concurren_options;
  }

 private:
  constexpr static const char *kMaxBackgroundThreadsKey{"MaxBackgroundThreads"};
  size_t max_background_threads_ = 1;
  constexpr static const char *kMaxCpuThreadsKey{"MaxCpuThreads"};
  size_t max_cpu_threads_ = 1;
  constexpr static const char *kLinuxNetFuncIndicatorKey{"LinuxFuncIndicator"};
  IndicatorContainer linux_net_func_indicator_;
  constexpr static const char *kDriverFuncIndicatorKey{"DriverFuncIndicator"};
  IndicatorContainer driver_func_indicator_;
  constexpr static const char *kErnelTxIndicatorKey{"KernelTxIndicator"};
  IndicatorContainer kernel_tx_indicator_;
  constexpr static const char *kErnelRxIndicatorKey{"KernelRxIndicator"};
  IndicatorContainer kernel_rx_indicator_;
  constexpr static const char *kPciWriteIndicatorsKey{"PciWriteIndicator"};
  IndicatorContainer pci_write_indicators_;
  constexpr static const char *kDriverTxIndicatorKey{"DriverTxIndicator"};
  IndicatorContainer driver_tx_indicator_;
  constexpr static const char *kDriverRxIndicatorKey{"DriverRxIndicator"};
  IndicatorContainer driver_rx_indicator_;
  constexpr static const char *kSysEntryKey{"SysEntryIndicator"};
  IndicatorContainer sys_entry_;
  constexpr static const char *kBlacklistFuncIndicatorKey{"BlacklistFunctions"};
  IndicatorContainer blacklist_func_indicator_;
  constexpr static const char *kTypesToFilterKey{"TypesToFilter"};
  TypeContainer types_to_filter_;
  constexpr static const char *kSymbolTablesKey{"SymbolTables"};
  SymTableContainer symbol_tables_;
  constexpr static const char *kJaegerUrlKey{"JaegerUrl"};
  std::string jaeger_url_;
  constexpr static const char *kLineBufferSizeKey{"LineBufferSize"};
  size_t line_buffer_size_ = 0;
  constexpr static const char *kEventBufferSize{"EventBufferSize"};
  size_t event_buffer_size_ = 0;
  constexpr static const char *kLogLevelKey{"LogLevel"};
  spdlog::level::level_enum log_level_ = spdlog::level::info;
};

#endif // SIMBRICKS_TRACE_CONFIG_H_
