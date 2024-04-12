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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHos WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, os OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <memory>
#include <queue>
#include <functional>
#include <set>

#include "sync/corobelt.h"
#include "events/events.h"

#ifndef SIMBRICKS_TRACE_TIMER_H_
#define SIMBRICKS_TRACE_TIMER_H_

class Timer {
  using ExecutorT = std::shared_ptr<concurrencpp::executor>;
  using ResultT = concurrencpp::result<void>;

 private:
  concurrencpp::async_lock timer_lock_;
  concurrencpp::async_condition_variable timer_cv_;
  size_t amount_waiters_ = 0;
  uint64_t cur_maximum_ = 0;
  size_t waiters_that_reached_maximum_ = 0;
  std::priority_queue<uint64_t, std::vector<uint64_t>, std::greater<>> waiters_;

 public:
  explicit Timer(size_t amount_waiters) : amount_waiters_(amount_waiters) {}

  ResultT Done(ExecutorT resume_executor) {
    {
      concurrencpp::scoped_async_lock guard = co_await timer_lock_.lock(resume_executor);
      --amount_waiters_;
    }

    timer_cv_.notify_all();
  }

  ResultT MoveForward(ExecutorT resume_executor, uint64_t timestamp) {
    {
      concurrencpp::scoped_async_lock guard = co_await timer_lock_.lock(resume_executor);
      if (timestamp <= cur_maximum_) {
        co_return;
      }

      ++waiters_that_reached_maximum_;
      waiters_.push(timestamp);
    }

    timer_cv_.notify_all();

    {
      concurrencpp::scoped_async_lock guard = co_await timer_lock_.lock(resume_executor);
      co_await timer_cv_.await(resume_executor, guard, [this, timestamp]() {
        return (waiters_that_reached_maximum_ >= amount_waiters_ and waiters_.top() == timestamp)
            or (cur_maximum_ >= timestamp);
      });
      if (timestamp > cur_maximum_) {
        cur_maximum_ = timestamp;
      }
      --waiters_that_reached_maximum_;
      waiters_.pop();
    }

    timer_cv_.notify_all();
  }
};

class WeakTimer {
  using ExecutorT = std::shared_ptr<concurrencpp::executor>;
  using KeyResT = concurrencpp::result<size_t>;
  using VoidResT = concurrencpp::result<void>;

 private:
  concurrencpp::async_lock timer_lock_;
  concurrencpp::async_condition_variable timer_cv_;
  size_t amount_waiters_;
  size_t next_key_ = 0;
  std::vector<uint64_t> waiters_;
  uint64_t cur_minimum_ = UINT64_MAX;
  size_t registered_ = 0;
  size_t waiters_that_reached_min_ = 0;
  size_t still_needed_waiters_;
  std::set<size_t> active_waiters_;

  void ComputeMin() {
    // NOTE: lock must be held when calling this method
    cur_minimum_ = UINT64_MAX;
    // O(n)... however probably much more efficient for small
    // amount of waiters than "fancy" data structures
    for (size_t index = 0; index < amount_waiters_; index++) {
      if (waiters_[index] < cur_minimum_) {
        cur_minimum_ = waiters_[index];
      }
    }
  }

 public:
  explicit WeakTimer(size_t amount_waiters) : amount_waiters_(amount_waiters),
                                              still_needed_waiters_(amount_waiters) {
    throw_on(amount_waiters < 2,
             "WeakTimer: must use more than one waiter, otherwise the timer is useless",
             source_loc::current());
    waiters_.resize(amount_waiters);
  };

  KeyResT Register(ExecutorT resume_executor) {
    size_t key;
    bool last_to_register;
    {
      concurrencpp::scoped_async_lock guard = co_await timer_lock_.lock(resume_executor);
      throw_on(registered_ >= amount_waiters_,
               "Timer::Register: already AmountWaiters many waiters registered",
               source_loc::current());
      ++registered_;
      key = next_key_++;
      waiters_[key] = UINT64_MAX;
      last_to_register = registered_ == amount_waiters_;
    }

    if (last_to_register) {
      std::cout << "WeakTimer::Register: all waiters registered!!" << std::endl;
      timer_cv_.notify_all();
    }

    co_return key;
  }

  VoidResT Done(ExecutorT resume_executor, size_t key) {
    {
      concurrencpp::scoped_async_lock guard = co_await timer_lock_.lock(resume_executor);
      --still_needed_waiters_;
      waiters_[key] = UINT64_MAX;
      ComputeMin();
      std::cout << "WeakTimer::Done: only " << still_needed_waiters_ << " waiters left" << std::endl;
    }

    timer_cv_.notify_all();
  }

  VoidResT MoveForward(ExecutorT resume_executor, size_t key, uint64_t timestamp) {
    {
      concurrencpp::scoped_async_lock guard = co_await timer_lock_.lock(resume_executor);
      throw_on(key >= amount_waiters_,
               "Timer::Register: try moving forward with illegal key",
               source_loc::current());

      // wait for all waiters to at least register
      //co_await timer_cv_.await(resume_executor, guard, [this]() {
      //  return registered_ == amount_waiters_;
      //});
      if (not active_waiters_.contains(key)) {
        const auto marked_active = active_waiters_.insert(key);
        throw_on(not marked_active.second,
                 "WeakTimer::MoveForward: could not mark waiter as active",
                 source_loc::current());
      }
      waiters_[key] = timestamp;
      // move forward if smaller than the current minimum/when being in the past
      if (timestamp < cur_minimum_) {
        guard.unlock();
        timer_cv_.notify_all();
        co_return;
      }

      ++waiters_that_reached_min_;
      // find the minimum when all are waiting currently
      ComputeMin();
      //if (waiters_that_reached_min_ == still_needed_waiters_) {
      //  ComputeMin();
      //  if (timestamp <= cur_minimum_) {
      //    // if minimum return
      //    --waiters_that_reached_min_;
      //    guard.unlock();
      //    timer_cv_.notify_all();
      //    co_return;
      //  }
      //}
    }

    timer_cv_.notify_all();

    {
      concurrencpp::scoped_async_lock guard = co_await timer_lock_.lock(resume_executor);
      // await being the minimum and notify others
      co_await timer_cv_.await(resume_executor, guard, [this, timestamp]() {
        return cur_minimum_ >= timestamp and waiters_that_reached_min_ == active_waiters_.size();
      });
      --waiters_that_reached_min_;
    }
    timer_cv_.notify_all();
    co_return;
  }

};

#endif // SIMBRICKS_TRACE_TIMER_H_
