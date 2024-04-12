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

#ifndef SIMBRICKS_TRACE_COROBELT_H_
#define SIMBRICKS_TRACE_COROBELT_H_

#include <exception>
#include <iostream>
#include <memory>
#include <algorithm>

#include "spdlog/spdlog.h"

#include "sync/channel.h"
#include "util/exception.h"
#include "util/factory.h"
#include "util/utils.h"

// TODO: may make this a concurrencpp::generator<...>
template<typename ValueType>
class Producer {
 public:
  explicit Producer() = default;

  virtual concurrencpp::result<std::optional<ValueType>> produce(std::shared_ptr<concurrencpp::executor> executor) {
    return {};
  };
};

template<typename ValueType>
class Consumer {
 public:
  explicit Consumer() = default;

  virtual concurrencpp::result<void> consume(std::shared_ptr<concurrencpp::executor> executor, ValueType value) {
    co_return;
  };
};

template<typename ValueType>
class Handler {
 public:
  explicit Handler() = default;

  virtual concurrencpp::result<bool> handel(std::shared_ptr<concurrencpp::executor> executor, ValueType &value) {
    co_return false;
  };
};

template<typename ValueType>
class Pipeline {
 public:
  std::shared_ptr<Producer<ValueType>> prod_;
  std::shared_ptr<std::vector<std::shared_ptr<Handler<ValueType>>>> handler_;
  std::shared_ptr<Consumer<ValueType>> cons_;

  explicit Pipeline(const std::shared_ptr<Producer<ValueType>> &prod,
                    const std::shared_ptr<std::vector<std::shared_ptr<Handler<ValueType>>>> &handler,
                    const std::shared_ptr<Consumer<ValueType>> &cons)
      : prod_(prod), handler_(handler), cons_(cons) {
    throw_if_empty(prod_, TraceException::kProducerIsNull, source_loc::current());
    throw_if_empty(handler_, TraceException::kHandlerIsNull, source_loc::current());
    for (std::shared_ptr<Handler<ValueType>> &han : *handler_) {
      throw_if_empty(han, TraceException::kProducerIsNull, source_loc::current());
    }
    throw_if_empty(cons_, TraceException::kConsumerIsNull, source_loc::current());
  }
};

template<typename ValueType>
inline concurrencpp::result<std::optional<ValueType>>
ProduceTask(concurrencpp::executor_tag,
            std::shared_ptr<concurrencpp::executor> tpe,
            std::shared_ptr<Producer<ValueType>> prod) {
  co_return co_await prod->produce(tpe);
}

template<typename ValueType>
inline concurrencpp::result<void> Produce(concurrencpp::executor_tag,
                                          std::shared_ptr<concurrencpp::executor> tpe,
                                          std::shared_ptr<Producer<ValueType>> producer,
                                          std::shared_ptr<CoroChannel<ValueType>> tar_chan) {
  throw_if_empty(tpe, TraceException::kResumeExecutorNull, source_loc::current());
  throw_if_empty(tar_chan, TraceException::kChannelIsNull, source_loc::current());
  throw_if_empty(producer, TraceException::kProducerIsNull, source_loc::current());

  std::optional<ValueType> value;
  do {
    value = co_await ProduceTask<ValueType>({}, tpe, producer);
    if (not value.has_value()) {
      break;
    }
    const bool could_push = co_await tar_chan->Push(tpe, *value);
//    tar_chan->PokeAwaiters();
    throw_on(not could_push,
             "unable to push next event to target channel",
             source_loc::current());
  } while (true);

  co_await tar_chan->CloseChannel(tpe);

  co_return;
}

template<typename ValueType>
inline concurrencpp::result<void> ConsumeTask(concurrencpp::executor_tag,
                                              std::shared_ptr<concurrencpp::executor> tpe,
                                              std::shared_ptr<Consumer<ValueType>> cons,
                                              ValueType val) {
  co_await cons->consume(tpe, val);
  co_return;
}

template<typename ValueType>
inline concurrencpp::result<void> Consume(concurrencpp::executor_tag,
                                          std::shared_ptr<concurrencpp::executor> tpe,
                                          std::shared_ptr<Consumer<ValueType>> consumer,
                                          std::shared_ptr<CoroChannel<ValueType>> src_chan) {
  throw_if_empty(tpe, TraceException::kResumeExecutorNull, source_loc::current());
  throw_if_empty(src_chan, TraceException::kChannelIsNull, source_loc::current());
  throw_if_empty(consumer, TraceException::kConsumerIsNull, source_loc::current());

  std::optional<ValueType> opt_val;
  for (opt_val = co_await src_chan->Pop(tpe); opt_val.has_value(); opt_val = co_await src_chan->Pop(tpe)) {
//    src_chan->PokeAwaiters();
    ValueType value = *opt_val;
    spdlog::trace("consumer consume next event");
    co_await ConsumeTask<ValueType>({}, tpe, consumer, value);
  }

  co_return;
}

template<typename ValueType>
inline concurrencpp::result<bool> HandelTask(concurrencpp::executor_tag,
                                             std::shared_ptr<concurrencpp::executor> tpe,
                                             std::shared_ptr<Handler<ValueType>> hand,
                                             ValueType &value) {
  const bool res = co_await hand->handel(tpe, value);
  co_return res;
}

template<typename ValueType>
inline concurrencpp::result<void> Handel(concurrencpp::executor_tag,
                                         std::shared_ptr<concurrencpp::executor> tpe,
                                         std::shared_ptr<Handler<ValueType>> handler,
                                         std::shared_ptr<CoroChannel<ValueType>> src_chan,
                                         std::shared_ptr<CoroChannel<ValueType>> tar_chan) {
  throw_if_empty(tpe, TraceException::kResumeExecutorNull, source_loc::current());
  throw_if_empty(src_chan, TraceException::kChannelIsNull, source_loc::current());
  throw_if_empty(tar_chan, TraceException::kChannelIsNull, source_loc::current());
  throw_if_empty(handler, TraceException::kHandlerIsNull, source_loc::current());

  std::optional<ValueType> opt_val;
  for (opt_val = co_await src_chan->Pop(tpe); opt_val.has_value(); opt_val = co_await src_chan->Pop(tpe)) {
//    src_chan->PokeAwaiters();
    ValueType value = *opt_val;

    spdlog::trace("handler handel next event");
    const bool pass_on = co_await HandelTask<ValueType>({}, tpe, handler, value);

    if (pass_on) {
      spdlog::trace("handler pass on next event");
      const bool could_push = co_await tar_chan->Push(tpe, value);
//      tar_chan->PokeAwaiters();
      throw_on(not could_push,
               "unable to push next event to target channel",
               source_loc::current());
    }
  }

  co_await tar_chan->CloseChannel(tpe);

  co_return;
}

template<typename ValueType>
inline concurrencpp::result<void> RunPipelineImpl(std::shared_ptr<concurrencpp::executor> tpe,
                                                  std::shared_ptr<Pipeline<ValueType>> pipeline) {
  throw_if_empty(tpe, TraceException::kResumeExecutorNull, source_loc::current());
  throw_if_empty(pipeline, TraceException::kPipelineNull, source_loc::current());

  const size_t amount_channels = pipeline->handler_->size() + 1;
  std::vector<std::shared_ptr<CoroChannel<ValueType>>> channels{amount_channels};
  std::vector<concurrencpp::result<void>> tasks{amount_channels + 1};

  // start producer
  channels[0] = create_shared<CoroBoundedChannel<ValueType>>(TraceException::kChannelIsNull);
  throw_if_empty(pipeline->prod_, TraceException::kProducerIsNull, source_loc::current());
  tasks[0] = Produce({}, tpe, pipeline->prod_, channels[0]);
  // start handler
  for (int index = 0; index < pipeline->handler_->size(); index++) {
    auto &handl = *(pipeline->handler_);
    auto &handler = handl[index];
    throw_if_empty(handler, TraceException::kHandlerIsNull, source_loc::current());

    channels[index + 1] = create_shared<CoroBoundedChannel<ValueType>>(TraceException::kChannelIsNull);

    tasks[index + 1] = Handel({}, tpe, handler, channels[index], channels[index + 1]);
  }
  // start consumer
  throw_if_empty(pipeline->cons_, TraceException::kConsumerIsNull, source_loc::current());
  tasks[amount_channels] = Consume({}, tpe, pipeline->cons_, channels[amount_channels - 1]);

  // wait for all tasks to finish
  // NOTE: DO NOT USE concurrencpp::when_all(...) here, is
  // important to wait here in order and to close the channels in order
  for (int index = 0; index < amount_channels; index++) {
    co_await tasks[index];
    co_await channels[index]->CloseChannel(tpe);
  }
  co_await tasks[amount_channels];

  co_return;
}

template<typename ValueType>
inline void RunPipeline(std::shared_ptr<concurrencpp::executor> tpe,
                        std::shared_ptr<Pipeline<ValueType>> pipeline) {
  spdlog::info("start a pipeline");
  RunPipelineImpl(tpe, pipeline).get();
  spdlog::info("finished a pipeline");
}

template<typename ValueType>
inline concurrencpp::result<void> RunPipelineParallelImpl(concurrencpp::executor_tag,
                                                          std::shared_ptr<concurrencpp::executor> tpe,
                                                          std::shared_ptr<Pipeline<ValueType>> pipeline) {
  co_await RunPipelineImpl(tpe, pipeline);
}

template<typename ValueType>
inline concurrencpp::result<void> RunPipelinesImpl(std::shared_ptr<concurrencpp::executor> tpe,
                                                   std::shared_ptr<std::vector<std::shared_ptr<Pipeline<ValueType>>>> pipelines) {
  throw_if_empty(tpe, TraceException::kResumeExecutorNull, source_loc::current());
  throw_if_empty(pipelines, "vector is null", source_loc::current());

  std::vector<concurrencpp::result<void>> tasks(pipelines->size());
  for (int index = 0; index < pipelines->size(); index++) {
    std::shared_ptr<Pipeline<ValueType>> pipeline = (*pipelines)[index];
    throw_if_empty(pipeline, TraceException::kPipelineNull, source_loc::current());

    tasks[index] = RunPipelineParallelImpl({}, tpe, pipeline);
  }

  // wait for all tasks to finish
  auto all_done = co_await concurrencpp::when_all(tpe, tasks.begin(), tasks.end());
  for (auto &done : all_done) {
    co_await done;
  }
  co_return;
}

template<typename ValueType>
inline void RunPipelines(std::shared_ptr<concurrencpp::executor> tpe,
                         std::shared_ptr<std::vector<std::shared_ptr<Pipeline<ValueType>>>> pipelines) {
  spdlog::info("start a pipeline");
  RunPipelinesImpl(tpe, pipelines).get();
  spdlog::info("finished a pipeline");
}

#endif  // SIMBRICKS_TRACE_COROBELT_H_
