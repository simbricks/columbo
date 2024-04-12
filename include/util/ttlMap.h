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

#include <unordered_map>
#include <queue>
#include <chrono>

#include "util/concepts.h"

#ifndef SIMBRICKS_TRACE_TTLMAP_H_
#define SIMBRICKS_TRACE_TTLMAP_H_

// TODO: make this an lru cache with ttl --> when something is accessed, its ttl will be re-calculated!!
template<typename KeyType, typename ValueType, uint64_t TTLSeconds> requires SizeLagerZero<TTLSeconds>
class LazyTtLMap {

  const std::chrono::seconds kTTL_{TTLSeconds};

 public:
  bool Insert(KeyType key, ValueType value) {
    CollectGarbage();

    auto iter = map_storage_.insert({key, value});
    if (not iter.second) {
      return false;
    }
    const uint64_t now = GetNow();
    timer_queue_.push({now, key});
    return true;
  }

  std::optional<ValueType> Find(KeyType key) {
    CollectGarbage();

    auto iter = map_storage_.find(key);
    if (iter != map_storage_.end()) {
      assert(map_expiration_times_.contains(key));
      const uint64_t now = GetNow();
      map_expiration_times_.find(key)->second = now;
      return iter->second;
    }
    return std::nullopt;
  }

  void Remove(KeyType key) {
    CollectGarbage();

    map_storage_.erase(key);
  }

  void CollectGarbage() {
    const uint64_t now = GetNow();
    while (not timer_queue_.empty()) {
      const auto &oldest = timer_queue_.front();
      if (oldest.first + kTTL_.count() < now) {
        break;
      }

      KeyType key = oldest.second;
      timer_queue_.pop();
      auto iter = map_expiration_times_.find(key);
      if (iter != map_expiration_times_.end() and iter->second + kTTL_.count() < now) {
        continue;
      }
      map_storage_.erase(key);
      map_expiration_times_.erase(key);
    }
  }

 private:
  uint64_t GetNow() {
    auto now = std::chrono::system_clock::now();
    return now.time_since_epoch().count();
  }

  std::unordered_map<KeyType, ValueType> map_storage_;
  std::unordered_map<KeyType, uint64_t> map_expiration_times_;
  std::queue<std::pair<uint64_t, KeyType>> timer_queue_;
};

#endif //SIMBRICKS_TRACE_TTLMAP_H_
