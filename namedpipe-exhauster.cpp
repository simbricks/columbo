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

#include <iostream>

#include "concurrencpp/concurrencpp.h"
#include "reader/cReader.h"
#include "spdlog/spdlog.h"
#include "util/cxxopts.hpp"
#include "util/utils.h"

using ExhaustFuncT = std::function<concurrencpp::result<void>()>;
using ExecutorT = std::shared_ptr<concurrencpp::thread_pool_executor>;

ExhaustFuncT MakeExhaustTask(const std::string &file_path, bool is_named_pipe) {
  ExhaustFuncT task = [&path = file_path,
                       is_pipe = is_named_pipe]() -> concurrencpp::result<void> {
    ReaderBuffer<MultiplePagesBytes(8)> buffer{"exhauster"};
    buffer.OpenFile(path, is_pipe);
    std::pair<bool, LineHandler *> handler;
    for (handler = buffer.NextHandler();
         handler.second and handler.second != nullptr;
         handler = buffer.NextHandler())
      ;
    co_return;
  };
  return task;
}

int main(int argc, char *argv[]) {
  cxxopts::Options options("exhauster",
                           "Tool to Exhaust Log-File or Named-Pipe");

  options.add_options()("h,help", "Print usage")(
      "filenames", "The filename(s) to exhaust",
      cxxopts::value<std::vector<std::string>>());

  cxxopts::ParseResult result;
  try {
    options.parse_positional({"filenames"});
    result = options.parse(argc, argv);
  } catch (cxxopts::exceptions::exception &e) {
    std::cerr << "Could not parse cli options: " << e.what() << '\n';
    exit(EXIT_FAILURE);
  }

  if (result.count("help")) {
    std::cout << options.help() << '\n';
    exit(EXIT_SUCCESS);
  }

  if (!result.count("filenames")) {
    std::cerr << "invalid arguments given" << '\n' << options.help() << '\n';
    exit(EXIT_FAILURE);
  }

  constexpr size_t kLineBufferSize = 1;
  constexpr size_t kTries = 1;
  const concurrencpp::runtime runtime;
  const ExecutorT executor = runtime.thread_pool_executor();

  const auto &filenames = result["filenames"].as<std::vector<std::string>>();
  std::vector<concurrencpp::result<concurrencpp::result<void>>> tasks(
      filenames.size());

  spdlog::info("START RUNNING EXHAUSTION");

  for (int index = 0; index < filenames.size(); index++) {
    const auto &file = filenames[index];
    auto task = MakeExhaustTask(file, true);
    tasks[index] = executor->submit(task);
  }

  for (auto &task : tasks) {
    task.get().get();
  }

  spdlog::info("FINISHED EXHAUSTION");

  exit(EXIT_SUCCESS);
}
