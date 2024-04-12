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

#ifndef SIM_TRACE_CONFIG_VARS_H_
#define SIM_TRACE_CONFIG_VARS_H_

#include <memory>
#include <set>
#include <string>
#include <vector>
#include <shared_mutex>

#include "config/config.h"
#include "events/events.h"
#include "env/stringInternalizer.h"
#include "env/symtable.h"

class TraceEnvironment {
  std::shared_mutex trace_env_reader_writer_mutex_;

  const TraceEnvConfig &trace_env_config_;

  StringInternalizer internalizer_;

  std::set<const std::string *> linux_net_func_indicator_;

  std::set<const std::string *> driver_func_indicator_;

  std::set<const std::string *> kernel_tx_indicator_;

  std::set<const std::string *> kernel_rx_indicator_;

  std::set<const std::string *> pci_write_indicators_;

  std::set<const std::string *> driver_tx_indicator_;

  std::set<const std::string *> driver_rx_indicator_;

  std::set<const std::string *> sys_entry_;

  std::set<const std::string *> blacklist_func_indicator_;

  std::set<EventType> types_to_filter_;

  std::vector<std::shared_ptr<SymsFilter>> symbol_tables_;

  concurrencpp::runtime runtime_;

  void InternalizeStrings(TraceEnvConfig::IndicatorContainer::const_iterator begin,
                          TraceEnvConfig::IndicatorContainer::const_iterator end,
                          std::set<const std::string *> &into) {
    for (; begin != end; begin++) {
      const std::string *symbol_ptr = internalizer_.Internalize(*begin);
      auto suc = into.insert(symbol_ptr);
      assert(suc.second);
    }
  }

  bool AddSymbolTableInternal(const std::string identifier,
                              const std::string &file_path,
                              uint64_t address_offset, FilterType type,
                              std::set<std::string> symbol_filter);

 protected:
  using PoolExecutor = concurrencpp::thread_pool_executor;
  using ThreadExecutor = concurrencpp::thread_executor;
  using PoolExecutorPtr = std::shared_ptr<PoolExecutor>;
  using ThreadExecutorPtr = std::shared_ptr<ThreadExecutor>;
  using WorkerThreadExecutorPtr = std::shared_ptr<concurrencpp::worker_thread_executor>;

  const std::string *GetCallFunc(const std::shared_ptr<Event> &event_ptr);

 public:
  explicit TraceEnvironment(const TraceEnvConfig &trace_env_config);

  ~TraceEnvironment() {
    runtime_.thread_executor()->shutdown();
    runtime_.thread_pool_executor()->shutdown();
    runtime_.inline_executor()->shutdown();
    runtime_.background_executor()->shutdown();
  }

  PoolExecutorPtr GetPoolExecutor() {
    const std::shared_lock reader_lock(trace_env_reader_writer_mutex_);
    auto executor = runtime_.thread_pool_executor();
    throw_if_empty(executor,
                   TraceException::kResumeExecutorNull,
                   source_loc::current());
    return executor;
  }

  PoolExecutorPtr GetBackgroundPoolExecutor() {
    const std::shared_lock reader_lock(trace_env_reader_writer_mutex_);
    auto executor = runtime_.background_executor();
    throw_if_empty(executor, TraceException::kResumeExecutorNull, source_loc::current());
    return executor;
  }

  ThreadExecutorPtr GetThreadExecutor() {
    const std::shared_lock reader_lock(trace_env_reader_writer_mutex_);
    auto executor = runtime_.thread_executor();
    throw_if_empty(executor, TraceException::kResumeExecutorNull, source_loc::current());
    return executor;
  }

  WorkerThreadExecutorPtr GetWorkerThreadExecutor() {
    const std::shared_lock reader_lock(trace_env_reader_writer_mutex_);
    auto executor = runtime_.make_worker_thread_executor();
    throw_if_empty(executor, TraceException::kResumeExecutorNull, source_loc::current());
    return executor;
  }

  TraceEnvConfig GetConfig() {
    const std::shared_lock reader_lock(trace_env_reader_writer_mutex_);
    return trace_env_config_;
  }

  inline StringInternalizer &GetInternalizer() {
    return internalizer_;
  }

  inline std::vector<std::shared_ptr<SymsFilter>> &GetSymtables() {
    return symbol_tables_;
  }

  static inline constexpr uint64_t GetDefaultId() {
    return 0;
  }

  static constexpr uint64_t kInvalidId = 0;

  static inline bool IsValidId(uint64_t ident) {
    return ident != kInvalidId;
  }

  inline uint64_t GetNextParserId() {
    const std::unique_lock writer_lock(trace_env_reader_writer_mutex_);
    static uint64_t next_id = 1;
    return next_id++;
  }

  inline uint64_t GetNextSpanId() {
    const std::unique_lock writer_lock(trace_env_reader_writer_mutex_);
    static uint64_t next_id = 1;
    return next_id++;
  }

  inline uint64_t GetNextSpannerId() {
    const std::unique_lock writer_lock(trace_env_reader_writer_mutex_);
    static uint64_t next_id = 1;
    return next_id++;
  }

  inline uint64_t GetNextTraceId() {
    const std::unique_lock writer_lock(trace_env_reader_writer_mutex_);
    static uint64_t next_id = 1;
    return next_id++;
  }

  inline uint64_t GetNextTraceContextId() {
    const std::unique_lock writer_lock(trace_env_reader_writer_mutex_);
    static uint64_t next_id = 1;
    return next_id++;
  }

  inline const std::string *InternalizeAdditional(const std::string &symbol) {
    const std::unique_lock writer_lock(trace_env_reader_writer_mutex_);
    return internalizer_.Internalize(symbol);
  }

  bool AddSymbolTable(const std::string identifier,
                      const std::string &file_path,
                      uint64_t address_offset, FilterType type,
                      std::set<std::string> symbol_filter);

  bool AddSymbolTable(const std::string identifier,
                      const std::string &file_path,
                      uint64_t address_offset, FilterType type);

  std::pair<const std::string *, const std::string *> SymtableFilter(uint64_t address);

  bool IsTypeToFilter(const std::shared_ptr<Event> &event_ptr);

  bool IsBlacklistedFunctionCall(const std::shared_ptr<Event> &event_ptr);

  bool IsBlacklistedFunctionCall(const std::string *func_name);

  bool IsDriverTx(const std::shared_ptr<Event> &event_ptr);

  bool IsDriverRx(const std::shared_ptr<Event> &event_ptr);

  bool IsPciMsixDescAddr(const std::shared_ptr<Event> &event_ptr);

  bool is_pci_write(const std::shared_ptr<Event> &event_ptr);

  bool IsKernelTx(const std::shared_ptr<Event> &event_ptr);

  bool IsKernelRx(const std::shared_ptr<Event> &event_ptr);

  bool IsKernelOrDriverTx(const std::shared_ptr<Event> &event_ptr);

  bool IsKernelOrDriverRx(const std::shared_ptr<Event> &event_ptr);

  bool IsSocketConnect(const std::shared_ptr<Event> &event_ptr);

  bool IsSysEntry(const std::shared_ptr<Event> &event_ptr);

  bool IsMsixNotToDeviceBarNumber(int bar);

  bool IsToDeviceBarNumber(int bar);
};

#endif  // SIM_TRACE_CONFIG_VARS_H_