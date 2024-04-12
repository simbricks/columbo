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
#include <iostream>
#include <memory>
#include <queue>
#include <charconv>
#include <sys/stat.h>

#include "sync/corobelt.h"
#include "util/exception.h"
#include "reader/cReader.h"

class ProducerInt : public Producer<int> {
  int start = 0;
  int end = 1;

 public:
  ProducerInt(int s, int e) : Producer<int>(), start(s), end(e) {
  }

  concurrencpp::result<std::optional<int>> produce(std::shared_ptr<concurrencpp::executor> executor) override {
    if (start >= end) {
      co_return std::nullopt;
    }

    int res = start;
    for (int i = 0; i < 100'000; i++) {
      res += 1;
    }
    for (int i = 0; i < 100'000; i++) {
      res -= 1;
    }
    start++;
    co_return res;
  }
};

class IntProducerParser : public Producer<int> {
  const std::string filepath_;
  ReaderBuffer<MultiplePagesBytes(16)> line_handler_buffer{"test reader buffer"};

 public:
  explicit IntProducerParser(const std::string &filepath) : Producer<int>(), filepath_(filepath) {}

  concurrencpp::result<std::optional<int>> produce(std::shared_ptr<concurrencpp::executor> executor) override {
    if (not line_handler_buffer.IsOpen()) {
      line_handler_buffer.OpenFile(filepath_, true);
    }

    std::pair<bool, LineHandler *> bh_p = line_handler_buffer.NextHandler();
    if (bh_p.first) {
      assert(bh_p.second != nullptr);
      int result = 0;
      bool res = bh_p.second->ParseInt(result);
      if (res) {
        co_return result;
      }
    }

    co_return std::nullopt;
  }
};

class AdderInt : public Handler<int> {
 public:
  explicit AdderInt() : Handler<int>() {}

  concurrencpp::result<bool> handel(std::shared_ptr<concurrencpp::executor> executor, int &value) override {
    for (int i = 0; i < 100'000; i++) {
      value += 1;
    }
    for (int i = 0; i < 100'000; i++) {
      value -= 1;
    }
    value += 1;
    co_return true;
  }
};

class PrinterInt : public Consumer<int> {
  std::ostream &out_;

 public:

  explicit PrinterInt(std::ostream &out) : Consumer<int>(), out_(out) {}

  concurrencpp::result<void> consume(std::shared_ptr<concurrencpp::executor> executor, int value) override {
    out_ << "consumed: " << value << '\n';
    co_return;
  }

};

class IntSum : public Consumer<int> {
  uint64_t sum_ = 0;

 public:

  explicit IntSum() : Consumer<int>() {}

  concurrencpp::result<void> consume(std::shared_ptr<concurrencpp::executor> executor, int value) override {
    for (int i = 0; i < 100'000; i++) {
      value += 1;
    }
    for (int i = 0; i < 100'000; i++) {
      value -= 1;
    }
    sum_ += value;
    co_return;
  }

  uint64_t GetSum() const {
    return sum_;
  }

};

#define CREATE_PIPELINE(name, start, end, amount_adder, ss) \
  auto prod_##name = std::make_shared<ProducerInt>(start, end); \
  auto adders_##name = std::make_shared<std::vector<std::shared_ptr<Handler<int>>>>(amount_adder); \
  for (size_t index = 0; index < amount_adder; index++) { \
    (*adders_##name)[index] = std::make_shared<AdderInt>(); \
  } \
  auto cons_##name = std::make_shared<PrinterInt>(ss); \
  auto pipeline_##name = std::make_shared<Pipeline<int>>(prod_##name, adders_##name, cons_##name);

std::string CreateExpectation(int start, int end) {
  std::stringstream ss;
  for (int i = start; i < end; i++) {
    ss << "consumed: " << i << '\n';
  }
  return std::move(ss.str());
}

TEST_CASE("Test NEW pipeline wrapper construct", "[run_pipeline]") {
  auto concurren_options = concurrencpp::runtime_options();
  concurren_options.max_background_threads = 0;
  concurren_options.max_cpu_threads = 5;
  const concurrencpp::runtime runtime{concurren_options};
  const auto thread_pool_executor = runtime.thread_pool_executor();

  std::stringstream ss_a;
  std::stringstream ss_b;

  SECTION("simple pipeline with handler") {
    CREATE_PIPELINE(simple_a, 0, 3, 30, ss_a);

    REQUIRE_NOTHROW(RunPipeline<int>(thread_pool_executor, pipeline_simple_a));

    REQUIRE(ss_a.str() == CreateExpectation(30, 33));
  }

  SECTION("multiple pipelines with handler") {
    CREATE_PIPELINE(simple_a, 0, 3, 30, ss_a);
    CREATE_PIPELINE(simple_b, 100, 103, 30, ss_b);

    auto pipelines = std::make_shared<std::vector<std::shared_ptr<Pipeline<int>>>>();
    pipelines->push_back(pipeline_simple_a);
    pipelines->push_back(pipeline_simple_b);

    REQUIRE_NOTHROW(RunPipelines<int>(thread_pool_executor, pipelines));

    REQUIRE(ss_a.str() == CreateExpectation(30, 33));
    REQUIRE(ss_b.str() == CreateExpectation(130, 133));
  }

  SECTION("run long pipeline") {
    std::stringstream ss_c;
    CREATE_PIPELINE(simple_c, 0, 3, 90, ss_c);

    REQUIRE_NOTHROW(RunPipeline<int>(thread_pool_executor, pipeline_simple_c));

    REQUIRE(ss_c.str() == CreateExpectation(90, 93));
  }

  SECTION("multiple long pipelines with handler") {
    std::stringstream ss_d;
    std::stringstream ss_e;
    CREATE_PIPELINE(simple_d, 0, 3, 90, ss_d);
    CREATE_PIPELINE(simple_e, 100, 103, 90, ss_e);

    auto pipelines = std::make_shared<std::vector<std::shared_ptr<Pipeline<int>>>>();
    pipelines->push_back(pipeline_simple_d);
    pipelines->push_back(pipeline_simple_e);

    REQUIRE_NOTHROW(RunPipelines<int>(thread_pool_executor, pipelines));

    REQUIRE(ss_d.str() == CreateExpectation(90, 93));
    REQUIRE(ss_e.str() == CreateExpectation(190, 193));
  }

//  SECTION("Very long Pipeline") {
//    auto prod = std::make_shared<ProducerInt>(0, 100'000'000);
//    auto adders = std::make_shared<std::vector<std::shared_ptr<Handler<int>>>>(1);
//    (*adders)[0] = std::make_shared<AdderInt>();
//    auto cons = std::make_shared<IntSum>();
//    auto pipeline = std::make_shared<Pipeline<int>>(prod, adders, cons);
//
//    REQUIRE_NOTHROW(RunPipeline<int>(thread_pool_executor, pipeline));
//
//    REQUIRE(cons->GetSum() == 0);
//  }
}

TEST_CASE("test named pipe reading alongside pipeline", "[named-pipe]") {
  auto concurren_options = concurrencpp::runtime_options();
  concurren_options.max_background_threads = 0;
  concurren_options.max_cpu_threads = 5;
  const concurrencpp::runtime runtime{concurren_options};
  const auto thread_pool_executor = runtime.thread_pool_executor();
  spdlog::set_level(spdlog::level::debug);

  const size_t amount = 1'000;
  const size_t amount_adder = 100;
  const std::string named_pipe_file = "/tmp/named_pipe";

  mkfifo(named_pipe_file.data(), 0666);

  std::function<void()> fillNamedPipe = [&]() {
    sleep(15);
    std::ofstream to{named_pipe_file};
    for (size_t i = 0; i < amount; i++) {
      to << i << std::endl;
    }
    return;
  };

  auto prod = std::make_shared<IntProducerParser>(named_pipe_file);
  auto adders = std::make_shared<std::vector<std::shared_ptr<Handler<int>>>>(amount_adder);
  for (size_t index = 0; index < amount_adder; index++) {
    (*adders)[index] = std::make_shared<AdderInt>();
  }
  auto cons = std::make_shared<PrinterInt>(std::cout);
  auto pipeline = std::make_shared<Pipeline<int>>(prod, adders, cons);

  std::thread fill_buffer_job(fillNamedPipe);
//  sleep(15);

  RunPipeline<int>(thread_pool_executor, pipeline);
  fill_buffer_job.join();
}

