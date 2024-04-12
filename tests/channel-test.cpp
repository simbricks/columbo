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
#include <memory>
#include "sync/corobelt.h"

TEST_CASE("Test BoundedChannel", "[BoundedChannel]") {
  auto concurren_options = concurrencpp::runtime_options();
  concurren_options.max_background_threads = 0;
  concurren_options.max_cpu_threads = 1;
  const concurrencpp::runtime runtime{concurren_options};
  const auto thread_pool_executor = runtime.thread_pool_executor();

  const size_t capacity = 3;
  CoroBoundedChannel<int> channel_to_test{capacity};

  SECTION("can push into channel") {
    REQUIRE(channel_to_test.Push(thread_pool_executor, 1).run().get());
    REQUIRE(channel_to_test.Push(thread_pool_executor, 2).run().get());
    REQUIRE(channel_to_test.Push(thread_pool_executor, 3).run().get());

    REQUIRE_FALSE(channel_to_test.TryPush(thread_pool_executor, 4).run().get());
  }

  SECTION("channel does not change order") {
    REQUIRE(channel_to_test.Push(thread_pool_executor, 1).run().get());
    REQUIRE(channel_to_test.Push(thread_pool_executor, 2).run().get());
    REQUIRE(channel_to_test.Push(thread_pool_executor, 3).run().get());

    REQUIRE(channel_to_test.Pop(thread_pool_executor).run().get().value_or(-1) == 1);
    REQUIRE(channel_to_test.Pop(thread_pool_executor).run().get().value_or(-1) == 2);
    REQUIRE(channel_to_test.Pop(thread_pool_executor).run().get().value_or(-1) == 3);
  }

  SECTION("cannot pull from empty channel") {
    REQUIRE_FALSE(channel_to_test.TryPop(thread_pool_executor).run().get());
  }

  SECTION("can read from and not write to closed channel") {
    REQUIRE(channel_to_test.Push(thread_pool_executor, 1).run().get());

    channel_to_test.CloseChannel(thread_pool_executor).get();

    REQUIRE(channel_to_test.Pop(thread_pool_executor).run().get().value_or(-1) == 1);
    REQUIRE_FALSE(channel_to_test.TryPush(thread_pool_executor, 2).run().get());
  }

  SECTION("cannot read from or write to poisened channel") {
    channel_to_test.PoisenChannel(thread_pool_executor).get();

    REQUIRE_FALSE(channel_to_test.TryPop(thread_pool_executor).run().get().has_value());
    REQUIRE_FALSE(channel_to_test.TryPush(thread_pool_executor, 2).run().get());
  }

  SECTION("fill channel, read, fill again, read") {
    for (size_t i = 0; i < capacity; i++) {
      REQUIRE(channel_to_test.Push(thread_pool_executor, i).run().get());
    }

    for (size_t i = 0; i < capacity; i++) {
      REQUIRE(channel_to_test.Pop(thread_pool_executor).run().get().value_or(capacity + 1) == i);
    }

    for (size_t i = 0; i < capacity; i++) {
      REQUIRE(channel_to_test.Push(thread_pool_executor, i).run().get());
    }

    for (size_t i = 0; i < capacity; i++) {
      REQUIRE(channel_to_test.Pop(thread_pool_executor).run().get().value_or(capacity + 1) == i);
    }
  }
}

TEST_CASE("Test UnBoundedChannel", "[UnBoundedChannel]") {
  auto concurren_options = concurrencpp::runtime_options();
  concurren_options.max_background_threads = 0;
  concurren_options.max_cpu_threads = 1;
  const concurrencpp::runtime runtime{concurren_options};
  const auto thread_pool_executor = runtime.thread_pool_executor();

  const size_t to_test_size{10};
  CoroUnBoundedChannel<int> channel_to_test;

  SECTION("channel does not change order and size is correct") {
    REQUIRE(channel_to_test.Push(thread_pool_executor, 1).run().get());
    REQUIRE(channel_to_test.Push(thread_pool_executor, 2).run().get());
    REQUIRE(channel_to_test.Push(thread_pool_executor, 3).run().get());
    REQUIRE(channel_to_test.GetSize(thread_pool_executor).get() == 3);
    REQUIRE_FALSE(channel_to_test.Empty(thread_pool_executor).get());

    REQUIRE(channel_to_test.Pop(thread_pool_executor).run().get().value_or(-1) == 1);
    REQUIRE(channel_to_test.Pop(thread_pool_executor).run().get().value_or(-1) == 2);
    REQUIRE(channel_to_test.Pop(thread_pool_executor).run().get().value_or(-1) == 3);
    REQUIRE(channel_to_test.GetSize(thread_pool_executor).get() == 0);
    REQUIRE(channel_to_test.Empty(thread_pool_executor).get());
  }

  SECTION("cannot pull from empty channel") {
    REQUIRE_FALSE(channel_to_test.TryPop(thread_pool_executor).run().get());
  }

  SECTION("cannot read from or write to poisened channel") {
    channel_to_test.PoisenChannel(thread_pool_executor).get();

    REQUIRE_FALSE(channel_to_test.TryPop(thread_pool_executor).run().get().has_value());
    REQUIRE_FALSE(channel_to_test.TryPush(thread_pool_executor, 2).run().get());
  }

  SECTION("fill channel, read, fill again, read") {
    for (size_t i = 0; i < to_test_size; i++) {
      REQUIRE(channel_to_test.Push(thread_pool_executor, i).run().get());
    }

    for (size_t i = 0; i < to_test_size; i++) {
      REQUIRE(channel_to_test.Pop(thread_pool_executor).run().get().value_or(to_test_size + 1) == i);
    }

    for (size_t i = 0; i < to_test_size; i++) {
      REQUIRE(channel_to_test.Push(thread_pool_executor, i).run().get());
    }

    for (size_t i = 0; i < to_test_size; i++) {
      REQUIRE(channel_to_test.Pop(thread_pool_executor).run().get().value_or(to_test_size + 1) == i);
    }
  }
}

concurrencpp::result<void>
fill_task(int bound,
          std::shared_ptr<concurrencpp::executor> executor,
          std::shared_ptr<CoroBoundedChannel<int>> chan) {
  for (int i = 0; i <= bound; i++) {
    co_await chan->Push(executor, i);
  }
  co_return;
}

concurrencpp::result<void>
fill_task_parallel(concurrencpp::executor_tag,
                   int bound,
                   std::shared_ptr<concurrencpp::executor> executor,
                   std::shared_ptr<CoroBoundedChannel<int>> chan) {
  co_return co_await fill_task(bound, executor, chan);
}

concurrencpp::result<int>
drain_task(int bound,
           std::shared_ptr<concurrencpp::executor> executor,
           std::shared_ptr<CoroBoundedChannel<int>> chan) {
  int akkumulator = 0;
  for (int i = 0; i <= bound; i++) {
    auto opt = co_await chan->Pop(executor);
    int read = opt.value_or(0);
    akkumulator += read;
    sleep(1);
  }
  co_return akkumulator;
}

concurrencpp::result<int>
drain_task_parallel(concurrencpp::executor_tag,
                    int bound,
                    std::shared_ptr<concurrencpp::executor> executor,
                    std::shared_ptr<CoroBoundedChannel<int>> chan) {
  co_return co_await drain_task(bound, executor, chan);
}

TEST_CASE("filling the channel", "[filling-chan]") {
  auto concurren_options = concurrencpp::runtime_options();
  concurren_options.max_background_threads = 6;
  concurren_options.max_cpu_threads = 6;
  const concurrencpp::runtime runtime{concurren_options};
  const auto thread_pool_executor = runtime.thread_pool_executor();
//  const auto thread_pool_executor = runtime.thread_executor();

  const size_t capacity = 3;
  auto channel_to_test = std::make_shared<CoroBoundedChannel<int>>(capacity);

  int bound = 10;

  auto cons = drain_task_parallel({}, bound, thread_pool_executor, channel_to_test);
  auto prod = fill_task_parallel({}, bound, thread_pool_executor, channel_to_test);
  int akkumulator = cons.get();
  prod.get();
  REQUIRE(akkumulator == (bound * (bound + 1)) / 2);
}
