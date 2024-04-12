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

#include <cassert>

#include "analytics/spanner.h"
#include "util/exception.h"
#include "events/printer.h"

concurrencpp::result<void> HostSpanner::FinishPendingSpan(
    std::shared_ptr<concurrencpp::executor> resume_executor) {
  if (not pending_host_call_span_) {
    co_return;
  }

  if (not pending_host_call_span_->DoesKernelReceive()) {
    co_await tracer_.MarkSpanAsDone(resume_executor, pending_host_call_span_);
    co_return;
  }

  spdlog::info("{} host try poll nic receive", name_);
  auto context = co_await PopPropagateContext(resume_executor, from_nic_receives_queue_);
  spdlog::info("{} host polled nic receive", name_);

  co_await tracer_.AddParentLazily(resume_executor, pending_host_call_span_, context);

  uint64_t syscall_start = pending_host_call_span_->GetStartingTs();
  std::function<bool(std::shared_ptr<Context> &)>
      did_arrive_before_receive_syscall =
      [&syscall_start](std::shared_ptr<Context> &context) {
        return context->HasParent() and
            syscall_start > context->GetParentStartingTs();
      };

  std::optional<std::shared_ptr<Context>> context_opt;
  spdlog::info("{} host try polling copy contexts nic receive", name_);
  for (
      context_opt = co_await from_nic_receives_queue_->TryPopOnTrue(resume_executor, did_arrive_before_receive_syscall);
      context_opt.has_value();
      context_opt =
          co_await from_nic_receives_queue_->TryPopOnTrue(resume_executor, did_arrive_before_receive_syscall)) {
//    from_nic_receives_queue_->PokeAwaiters();
    spdlog::info("{} host polled copy contexts nic receive", name_);
    context = *context_opt;

    auto copy_span = CloneShared(pending_host_call_span_);
    copy_span->SetOriginal(pending_host_call_span_);
    co_await tracer_.StartSpanSetParentContext(resume_executor, copy_span, context);
    co_await tracer_.MarkSpanAsDone(resume_executor, copy_span);
  }

  co_await tracer_.MarkSpanAsDone(resume_executor, pending_host_call_span_);
  co_return;
}

concurrencpp::result<bool> HostSpanner::CreateTraceStartingSpan(
    std::shared_ptr<concurrencpp::executor> resume_executor,
    std::shared_ptr<Event> &starting_event, bool fragmented) {
  co_await FinishPendingSpan(resume_executor);
  pending_host_call_span_ = co_await tracer_.StartSpan<HostCallSpan>(
      resume_executor, starting_event, starting_event->GetParserIdent(), name_, fragmented);
  throw_if_empty(pending_host_call_span_,
                 "could not register new pending_host_call_span_",
                 source_loc::current());
  last_trace_starting_span_ = pending_host_call_span_;

  pci_write_before_ = false;
  co_return true;
}

concurrencpp::result<bool> HostSpanner::HandelCall(
    std::shared_ptr<concurrencpp::executor> resume_executor,
    std::shared_ptr<Event> &event_ptr) {
  assert(event_ptr and "event_ptr is null");

  if (not pending_host_call_span_ and
      not co_await CreateTraceStartingSpan(resume_executor, event_ptr, false)) {
    co_return false;
  }

  throw_if_empty(pending_host_call_span_, TraceException::kSpanIsNull, source_loc::current());
  if (pending_host_call_span_->AddToSpan(event_ptr)) {
    pci_write_before_ = trace_environment_.is_pci_write(event_ptr);
    co_return true;

  } else if (pending_host_call_span_->IsComplete()) {
    auto created_new =
        co_await CreateTraceStartingSpan(resume_executor, event_ptr, false);
    assert(created_new and
               "HostSpanner: CreateTraceStartingSpan could not create new span");

    if (not pending_host_call_span_) {
      spdlog::warn("found new syscall entry, could not add "
                   "pending_host_call_span_");
      co_return false;
    }

    co_return true;
  }

  co_return false;
}

concurrencpp::result<bool> HostSpanner::HandelMmio(
    std::shared_ptr<concurrencpp::executor> resume_executor,
    std::shared_ptr<Event> &event_ptr) {
  assert(event_ptr and "event_ptr is null");

  auto pending_mmio_span =
      iterate_add_erase<HostMmioSpan>(pending_host_mmio_spans_, event_ptr);
  if (pending_mmio_span) {
    if (pending_mmio_span->IsComplete()) {
      co_await tracer_.MarkSpanAsDone(resume_executor, pending_mmio_span);
    }

    co_return true;
  }

  // create a pack that belongs to the trace of the current host call span
  auto mmio = std::static_pointer_cast<HostMmioOp>(event_ptr);
  throw_if_empty(pending_host_call_span_, TraceException::kSpanIsNull, source_loc::current());
  pending_mmio_span = co_await tracer_.StartSpanByParent<HostMmioSpan>(
      resume_executor, pending_host_call_span_, event_ptr, event_ptr->GetParserIdent(), name_,
      mmio->GetBar());
  if (not pending_mmio_span) {
    co_return false;
  }

  assert((IsType(event_ptr, EventType::kHostMmioWT) or
      IsType(event_ptr, EventType::kHostMmioRT)) and
      "try to create mmio host span but event is neither read nor write");

  if (not pci_write_before_ and trace_environment_.IsToDeviceBarNumber(
      pending_mmio_span->GetBarNumber())) {
    spdlog::info("{} host try push mmio", name_);
    co_await PushPropagateContext<expectation::kMmio>(resume_executor, to_nic_queue_, pending_mmio_span);
    spdlog::info("{} host pushed mmio", name_);
  }

  if (trace_environment_.IsMsixNotToDeviceBarNumber(
      pending_mmio_span->GetBarNumber()) and
      pending_mmio_span->IsComplete()) {
    co_await tracer_.MarkSpanAsDone(resume_executor, pending_mmio_span);
  }
  pending_host_mmio_spans_.push_back(pending_mmio_span);
  co_return true;
}

concurrencpp::result<bool> HostSpanner::HandelPci(
    std::shared_ptr<concurrencpp::executor> resume_executor,
    std::shared_ptr<Event> &event_ptr) {
  assert(event_ptr and "event_ptr is null");
  if (pending_pci_span_ and IsType(event_ptr, EventType::kHostConfT)) {
    bool could_be_added = pending_pci_span_->AddToSpan(event_ptr);
    throw_on(not could_be_added,
             "HostSpanner::HandelPci: could not add event to pending pci span",
             source_loc::current());
    assert(pending_pci_span_->IsComplete() and
        "HostSpanner::HandelPci: span is not complete but should be");
    co_await tracer_.MarkSpanAsDone(resume_executor, pending_pci_span_);
    pending_pci_span_ = nullptr;
    co_return true;
  }

  assert(IsType(event_ptr, EventType::kHostPciRWT) and
      "HostSpanner::HandelPci: event is no pci starting event");
  if (pending_pci_span_) {
    throw_on(not pending_pci_span_->HasEvents(),
             "HostSpanner::HandelPci: finsih pci without conf has no events!",
             source_loc::current());
    pending_pci_span_->MarkAsDone();
    co_await tracer_.MarkSpanAsDone(resume_executor, pending_pci_span_);
  }

  throw_if_empty(pending_host_call_span_, TraceException::kSpanIsNull, source_loc::current());
  pending_pci_span_ = co_await tracer_.StartSpanByParent<HostPciSpan>(
      resume_executor, pending_host_call_span_, event_ptr, event_ptr->GetParserIdent(), name_);
  if (not pending_pci_span_) {
    co_return false;
  }

  co_return true;
}

concurrencpp::result<bool> HostSpanner::HandelDma(
    std::shared_ptr<concurrencpp::executor> resume_executor,
    std::shared_ptr<Event> &event_ptr) {
  assert(event_ptr and "event_ptr is null");

  // check if we had an interrupt (msix) this belongs to
  if (pending_host_msix_span_ and
      pending_host_msix_span_->AddToSpan(event_ptr)) {
    assert(pending_host_msix_span_->IsComplete() and
        "pending_host_msix_span_ is not complete");
    co_await tracer_.MarkSpanAsDone(resume_executor, pending_host_msix_span_);
    pending_host_msix_span_ = nullptr;
    co_return true;
  }

  auto pending_dma =
      iterate_add_erase<HostDmaSpan>(pending_host_dma_spans_, event_ptr);
  if (pending_dma) {
    if (pending_dma->IsComplete()) {
      co_await tracer_.MarkSpanAsDone(resume_executor, pending_dma);
    }
    co_return true;
  }

  // when receiving a dma, we expect to get a context from the nic simulator,
  // hence poll this context blocking!!
  spdlog::info("{} host try poll dma: {}", name_, *event_ptr);
  const auto con = co_await PopPropagateContext(resume_executor, from_nic_queue_);
  spdlog::info("{} host polled dma", name_);

  if (not is_expectation(con, expectation::kDma)) {
    spdlog::critical("when polling for dma context, no dma context was fetched");
    co_return false;
  }

  // TODO: investigate this case further
  if (IsType(event_ptr, EventType::kHostDmaCT)) {
    spdlog::warn("unexpected event: ", *event_ptr);
    co_return false;
  }
  assert(not IsType(event_ptr, EventType::kHostDmaCT) and
      "cannot start HostDmaSPan with Dma completion");
  pending_dma = co_await tracer_.StartSpanByParentPassOnContext<HostDmaSpan>(
      resume_executor, con, event_ptr, event_ptr->GetParserIdent(), name_);
  if (not pending_dma) {
    co_return false;
  }
  pending_host_dma_spans_.push_back(pending_dma);
  co_return true;
}

concurrencpp::result<bool> HostSpanner::HandelMsix(
    std::shared_ptr<concurrencpp::executor> resume_executor,
    std::shared_ptr<Event> &event_ptr) {
  assert(event_ptr and "event_ptr is null");

  spdlog::info("{} host try poll msix", name_);
  const auto con = co_await PopPropagateContext(resume_executor, from_nic_queue_);
  spdlog::info("{} host polled msix", name_);

  if (not is_expectation(con, expectation::kMsix)) {
    spdlog::warn("did not receive msix on context queue");
    co_return false;
  }

  pending_host_msix_span_ = co_await tracer_.StartSpanByParentPassOnContext<HostMsixSpan>(
      resume_executor, con, event_ptr, event_ptr->GetParserIdent(), name_);
  if (not pending_host_msix_span_) {
    co_return false;
  }

  assert(pending_host_msix_span_->IsPending() and "host msix span is complete");
  co_return true;
}

concurrencpp::result<bool> HostSpanner::HandelInt(
    [[maybe_unused]] std::shared_ptr<concurrencpp::executor> resume_executor,
    std::shared_ptr<Event> &event_ptr) {
  assert(event_ptr and "event_ptr is null");

  if (not pending_host_int_span_) {
    throw_if_empty(pending_host_call_span_, TraceException::kSpanIsNull, source_loc::current());
    pending_host_int_span_ = co_await tracer_.StartSpanByParent<HostIntSpan>(
        resume_executor, pending_host_call_span_, event_ptr, event_ptr->GetParserIdent(), name_);
    if (not pending_host_int_span_) {
      co_return false;
    }

    co_return true;
  }

  if (not pending_host_int_span_->AddToSpan(event_ptr)) {
    co_return false;
  }

  if (pending_host_int_span_->IsPending()) {
    co_return false;
  }

  co_await tracer_.MarkSpanAsDone(resume_executor, pending_host_int_span_);
  pending_host_int_span_ = nullptr;
  co_return true;
}

HostSpanner::HostSpanner(
    TraceEnvironment &trace_environment,
    std::string &&name,
    Tracer &tra,
    std::shared_ptr<CoroChannel<std::shared_ptr<Context>>> to_nic,
    std::shared_ptr<CoroChannel<std::shared_ptr<Context>>> from_nic,
    std::shared_ptr<CoroChannel<std::shared_ptr<Context>>> from_nic_receives)
    : Spanner(trace_environment, std::move(name), tra),
      to_nic_queue_(std::move(to_nic)),
      from_nic_queue_(std::move(from_nic)),
      from_nic_receives_queue_(std::move(from_nic_receives)) {
  throw_if_empty(to_nic_queue_, TraceException::kQueueIsNull, source_loc::current());
  throw_if_empty(from_nic_queue_, TraceException::kQueueIsNull, source_loc::current());
  throw_if_empty(from_nic_receives_queue_, TraceException::kQueueIsNull, source_loc::current());

  auto handel_call = [this](ExecutorT resume_executor, EventT &event_ptr) {
    return HandelCall(std::move(resume_executor), event_ptr);
  };
  RegisterHandler(EventType::kHostCallT, handel_call);

  auto handel_mmio = [this](ExecutorT resume_executor, EventT &event_ptr) {
    return HandelMmio(std::move(resume_executor), event_ptr);
  };
  RegisterHandler(EventType::kHostMmioWT, handel_mmio);
  RegisterHandler(EventType::kHostMmioRT, handel_mmio);
  RegisterHandler(EventType::kHostMmioImRespPoWT, handel_mmio);
  RegisterHandler(EventType::kHostMmioCWT, handel_mmio);
  RegisterHandler(EventType::kHostMmioCRT, handel_mmio);

  auto handel_pci = [this](ExecutorT resume_executor, EventT &event_ptr) {
    return HandelPci(std::move(resume_executor), event_ptr);
  };
  RegisterHandler(EventType::kHostPciRWT, handel_pci);
  RegisterHandler(EventType::kHostConfT, handel_pci);

  auto handel_dma = [this](ExecutorT resume_executor, EventT &event_ptr) {
    return HandelDma(std::move(resume_executor), event_ptr);
  };
  RegisterHandler(EventType::kHostDmaWT, handel_dma);
  RegisterHandler(EventType::kHostDmaRT, handel_dma);
  RegisterHandler(EventType::kHostDmaCT, handel_dma);

  auto handel_misx = [this](ExecutorT resume_executor, EventT &event_ptr) {
    return HandelMsix(std::move(resume_executor), event_ptr);
  };
  RegisterHandler(EventType::kHostMsiXT, handel_misx);

  auto handel_int = [this](ExecutorT resume_executor, EventT &event_ptr) {
    return HandelInt(std::move(resume_executor), event_ptr);
  };
  RegisterHandler(EventType::kHostPostIntT, handel_int);
  RegisterHandler(EventType::kHostClearIntT, handel_int);
}
