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

#include <utility>

#include "channel.h"
#include "corobelt.h"
#include "events/events.h"
#include "env/traceEnvironment.h"

#ifndef SIMBRICKS_TRACE_CORO_SYNC_SPECIALIZATIONS_H_
#define SIMBRICKS_TRACE_CORO_SYNC_SPECIALIZATIONS_H_

inline concurrencpp::result<std::optional<std::shared_ptr<Event>>>
ProduceTask(concurrencpp::executor_tag,
            std::shared_ptr<concurrencpp::executor> tpe,
            std::shared_ptr<Producer<std::shared_ptr<Event>>> prod) {
  co_return co_await prod->produce(tpe);
}

// specialization of corobelt methods
inline concurrencpp::result<void> Produce(concurrencpp::executor_tag,
                                          std::shared_ptr<concurrencpp::executor> tpe,
                                          std::shared_ptr<Producer<std::shared_ptr<Event>>> producer,
                                          std::shared_ptr<CoroBoundedChannel<std::shared_ptr<Event>>> tar_chan) {
  throw_if_empty(tpe, TraceException::kResumeExecutorNull, source_loc::current());
  throw_if_empty(tar_chan, TraceException::kChannelIsNull, source_loc::current());
  throw_if_empty(producer, TraceException::kProducerIsNull, source_loc::current());

  std::optional<std::shared_ptr<Event>> value;
  do {
    value = co_await ProduceTask({}, tpe, producer);
    if (not value.has_value()) {
      break;
    }
    const bool could_push = co_await tar_chan->Push(tpe, std::move(*value));
//    tar_chan->PokeAwaiters();
    throw_on(not could_push,
             "unable to push next event to target channel",
             source_loc::current());
  } while (true);

  co_await tar_chan->CloseChannel(tpe);

  co_return;
}

inline concurrencpp::result<void> ConsumeTask(concurrencpp::executor_tag,
                                              std::shared_ptr<concurrencpp::executor> tpe,
                                              std::shared_ptr<Consumer<std::shared_ptr<Event>>> cons,
                                              std::shared_ptr<Event> val) {
  co_await cons->consume(tpe, std::move(val));
  co_return;
}

inline concurrencpp::result<void> Consume(concurrencpp::executor_tag,
                                          std::shared_ptr<concurrencpp::executor> tpe,
                                          std::shared_ptr<Consumer<std::shared_ptr<Event>>> consumer,
                                          std::shared_ptr<CoroBoundedChannel<std::shared_ptr<Event>>> src_chan) {
  throw_if_empty(tpe, TraceException::kResumeExecutorNull, source_loc::current());
  throw_if_empty(src_chan, TraceException::kChannelIsNull, source_loc::current());
  throw_if_empty(consumer, TraceException::kConsumerIsNull, source_loc::current());

  std::optional<std::shared_ptr<Event>> opt_val;
  for (opt_val = co_await src_chan->Pop(tpe); opt_val.has_value(); opt_val = co_await src_chan->Pop(tpe)) {
//    src_chan->PokeAwaiters();
    std::shared_ptr<Event> value = std::move(*opt_val);
    spdlog::trace("consumer consume next event");
    co_await ConsumeTask({}, tpe, consumer, std::move(value));
  }

  co_return;
}

inline concurrencpp::result<bool> HandelTask(concurrencpp::executor_tag,
                                             std::shared_ptr<concurrencpp::executor> tpe,
                                             std::shared_ptr<Handler<std::shared_ptr<Event>>> hand,
                                             std::shared_ptr<Event> value) {
  const bool res = co_await hand->handel(tpe, value);
  co_return res;
}

inline concurrencpp::result<void> Handel(concurrencpp::executor_tag,
                                         std::shared_ptr<concurrencpp::executor> tpe,
                                         std::shared_ptr<Handler<std::shared_ptr<Event>>> handler,
                                         std::shared_ptr<CoroBoundedChannel<std::shared_ptr<Event>>> src_chan,
                                         std::shared_ptr<CoroBoundedChannel<std::shared_ptr<Event>>> tar_chan) {
  throw_if_empty(tpe, TraceException::kResumeExecutorNull, source_loc::current());
  throw_if_empty(src_chan, TraceException::kChannelIsNull, source_loc::current());
  throw_if_empty(tar_chan, TraceException::kChannelIsNull, source_loc::current());
  throw_if_empty(handler, TraceException::kHandlerIsNull, source_loc::current());

  std::optional<std::shared_ptr<Event>> opt_val;
  for (opt_val = co_await src_chan->Pop(tpe); opt_val.has_value(); opt_val = co_await src_chan->Pop(tpe)) {
//    src_chan->PokeAwaiters();
    std::shared_ptr<Event> value = std::move(*opt_val);

    spdlog::trace("handler handel next event");
    const bool pass_on = co_await HandelTask({}, tpe, handler, value);

    if (pass_on) {
      spdlog::trace("handler pass on next event");
      const bool could_push = co_await tar_chan->Push(tpe, std::move(value));
//      tar_chan->PokeAwaiters();
      throw_on(not could_push,
               "unable to push next event to target channel",
               source_loc::current());
    }
  }

  co_await tar_chan->CloseChannel(tpe);

  co_return;
}

template<>
inline concurrencpp::result<void> RunPipelineImpl<std::shared_ptr<Event>>(std::shared_ptr<concurrencpp::executor> tpe,
                                                                          std::shared_ptr<Pipeline<std::shared_ptr<Event>>> pipeline) {
  throw_if_empty(tpe, TraceException::kResumeExecutorNull, source_loc::current());
  throw_if_empty(pipeline, TraceException::kPipelineNull, source_loc::current());

  const size_t amount_channels = pipeline->handler_->size() + 1;
  std::vector<std::shared_ptr<CoroBoundedChannel<std::shared_ptr<Event>>>> channels{amount_channels};
  std::vector<concurrencpp::result<void>> tasks{amount_channels + 1};

  // start producer
  channels[0] = create_shared<CoroBoundedChannel<std::shared_ptr<Event>>>(TraceException::kChannelIsNull);
  throw_if_empty(pipeline->prod_, TraceException::kProducerIsNull, source_loc::current());
  tasks[0] = Produce({}, tpe, pipeline->prod_, channels[0]);
  // start handler
  for (int index = 0; index < pipeline->handler_->size(); index++) {
    auto &handl = *(pipeline->handler_);
    auto &handler = handl[index];
    throw_if_empty(handler, TraceException::kHandlerIsNull, source_loc::current());

    channels[index + 1] = create_shared<CoroBoundedChannel<std::shared_ptr<Event>>>(TraceException::kChannelIsNull);

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

inline concurrencpp::result<void> RunPipelineImpl(std::shared_ptr<concurrencpp::executor> executor,
                                                  TraceEnvironment &trace_env,
                                                  std::shared_ptr<Pipeline<std::shared_ptr<Event>>> pipeline) {
  throw_if_empty(pipeline, TraceException::kPipelineNull, source_loc::current());

  const size_t amount_channels = pipeline->handler_->size() + 1;
  std::vector<std::shared_ptr<CoroBoundedChannel<std::shared_ptr<Event>>>> channels{amount_channels};
  std::vector<concurrencpp::result<void>> tasks{amount_channels + 1};

  // start producer
  channels[0] = create_shared<CoroBoundedChannel<std::shared_ptr<Event>>>(TraceException::kChannelIsNull);
  throw_if_empty(pipeline->prod_, TraceException::kProducerIsNull, source_loc::current());
  tasks[0] = Produce({}, trace_env.GetWorkerThreadExecutor(), pipeline->prod_, channels[0]);
  // start handler
  for (int index = 0; index < pipeline->handler_->size(); index++) {
    auto &handl = *(pipeline->handler_);
    auto &handler = handl[index];
    throw_if_empty(handler, TraceException::kHandlerIsNull, source_loc::current());

    channels[index + 1] = create_shared<CoroBoundedChannel<std::shared_ptr<Event>>>(TraceException::kChannelIsNull);

    tasks[index + 1] = Handel({}, trace_env.GetWorkerThreadExecutor(), handler, channels[index], channels[index + 1]);
  }
  // start consumer
  throw_if_empty(pipeline->cons_, TraceException::kConsumerIsNull, source_loc::current());
  tasks[amount_channels] =
      Consume({}, trace_env.GetWorkerThreadExecutor(), pipeline->cons_, channels[amount_channels - 1]);

  // wait for all tasks to finish
  // NOTE: DO NOT USE concurrencpp::when_all(...) here, is
  // important to wait here in order and to close the channels in order
  for (int index = 0; index < amount_channels; index++) {
    co_await tasks[index];
    co_await channels[index]->CloseChannel(executor);
  }
  co_await tasks[amount_channels];

  co_return;
}

template<>
inline void RunPipeline<std::shared_ptr<Event>>(std::shared_ptr<concurrencpp::executor> tpe,
                                                std::shared_ptr<Pipeline<std::shared_ptr<Event>>> pipeline) {
  spdlog::info("start a pipeline");
  RunPipelineImpl<std::shared_ptr<Event>>(std::move(tpe), std::move(pipeline)).get();
  spdlog::info("finished a pipeline");
}

template<>
inline concurrencpp::result<void> RunPipelineParallelImpl<std::shared_ptr<Event>>(concurrencpp::executor_tag,
                                                                                  std::shared_ptr<concurrencpp::executor> tpe,
                                                                                  std::shared_ptr<Pipeline<std::shared_ptr<
                                                                                      Event>>> pipeline) {
  co_await RunPipelineImpl<std::shared_ptr<Event>>(tpe, pipeline);
}

inline concurrencpp::result<void> RunPipelineParallelImpl(concurrencpp::executor_tag,
                                                          std::shared_ptr<concurrencpp::executor> tpe,
                                                          std::shared_ptr<Pipeline<std::shared_ptr<Event>>> pipeline,
                                                          TraceEnvironment &trace_env) {
  co_await RunPipelineImpl(tpe, trace_env, pipeline);
}

template<>
inline concurrencpp::result<void> RunPipelinesImpl<std::shared_ptr<Event>>(std::shared_ptr<concurrencpp::executor> tpe,
                                                                           std::shared_ptr<std::vector<std::shared_ptr<
                                                                               Pipeline<std::shared_ptr<Event>>>>> pipelines) {
  throw_if_empty(tpe, TraceException::kResumeExecutorNull, source_loc::current());
  throw_if_empty(pipelines, "vector is null", source_loc::current());

  std::vector<concurrencpp::result<void>> tasks(pipelines->size());
  for (int index = 0; index < pipelines->size(); index++) {
    std::shared_ptr<Pipeline<std::shared_ptr<Event>>> pipeline = (*pipelines)[index];
    throw_if_empty(pipeline, TraceException::kPipelineNull, source_loc::current());

    tasks[index] = RunPipelineParallelImpl<std::shared_ptr<Event>>({}, tpe, pipeline);
  }

  // wait for all tasks to finish
  auto all_done = co_await concurrencpp::when_all(tpe, tasks.begin(), tasks.end());
  for (auto &done : all_done) {
    co_await done;
  }
  co_return;
}

inline concurrencpp::result<void> RunPipelinesImpl(TraceEnvironment &trace_env,
                                                   std::shared_ptr<std::vector<std::shared_ptr<
                                                       Pipeline<std::shared_ptr<Event>>>>> pipelines) {
  throw_if_empty(pipelines, "vector is null", source_loc::current());

  std::vector<concurrencpp::result<void>> tasks(pipelines->size());
  for (int index = 0; index < pipelines->size(); index++) {
    std::shared_ptr<Pipeline<std::shared_ptr<Event>>> pipeline = (*pipelines)[index];
    throw_if_empty(pipeline, TraceException::kPipelineNull, source_loc::current());

    tasks[index] = RunPipelineParallelImpl({}, trace_env.GetWorkerThreadExecutor(), pipeline, trace_env);
  }

  // wait for all tasks to finish
  auto waiter_pool = trace_env.GetWorkerThreadExecutor();
  auto all_done = co_await concurrencpp::when_all(waiter_pool, tasks.begin(), tasks.end());
  for (auto &done : all_done) {
    co_await done;
  }
  co_return;
}

template<>
inline void RunPipelines<std::shared_ptr<Event>>(std::shared_ptr<concurrencpp::executor> tpe,
                                                 std::shared_ptr<std::vector<std::shared_ptr<Pipeline<std::shared_ptr<
                                                     Event>>>>> pipelines) {
  spdlog::info("start a pipeline");
  RunPipelinesImpl<std::shared_ptr<Event>>(std::move(tpe), std::move(pipelines)).get();
  spdlog::info("finished a pipeline");
}

inline void RunPipelines(TraceEnvironment &trace_env,
                         std::shared_ptr<std::vector<std::shared_ptr<Pipeline<std::shared_ptr<
                             Event>>>>> pipelines) {
  spdlog::info("start a pipeline");
  RunPipelinesImpl(trace_env, std::move(pipelines)).get();
  spdlog::info("finished a pipeline");
}

#endif // SIMBRICKS_TRACE_CORO_SYNC_SPECIALIZATIONS_H_
