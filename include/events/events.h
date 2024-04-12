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

#ifndef SIMBRICKS_TRACE_EVENTS_H_
#define SIMBRICKS_TRACE_EVENTS_H_

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <ostream>
#include <set>
#include <string>
#include <type_traits>

#include "util/exception.h"
#include "sync/corobelt.h"
#include "sync/channel.h"
#include "util/log.h"
#include "events/eventType.h"

#define DEBUG_EVENT_ ;

class LogParser;

/* Parent class for all events of interest */
class Event {
  EventType type_;
  const std::string name_;
  uint64_t timestamp_;
  const size_t parser_identifier_;
  const std::string &parser_name_;

 public:
  inline size_t GetParserIdent() const {
    return parser_identifier_;
  }

  inline const std::string &GetName() {
    return name_;
  }

  inline const std::string &GetParserName() {
    return parser_name_;
  }

  EventType GetType() const {
    return type_;
  }

  inline uint64_t GetTs() const {
    return timestamp_;
  }

  virtual void Display(std::ostream &out);

  virtual bool Equal(const Event &other);

  virtual Event *clone() {
    return new Event(*this);
  }

  virtual ~Event() = default;

 protected:
  explicit Event(uint64_t timestamp, const size_t parser_identifier,
                 const std::string &parser_name, EventType type,
                 const std::string &&name)
      : type_(type),
        name_(name),
        timestamp_(timestamp),
        parser_identifier_(parser_identifier),
        parser_name_(parser_name) {
  }

  Event(const Event &other) = default;
};

/* Simbricks Events */
class SimSendSync : public Event {
 public:
  explicit SimSendSync(uint64_t timestamp, const size_t parser_identifier,
                       const std::string &parser_name)
      : Event(timestamp, parser_identifier, parser_name, EventType::kSimSendSyncT,
              "SimSendSyncSimSendSync") {
  }

  SimSendSync(const SimSendSync &other) = default;

  Event *clone() override {
    return new SimSendSync(*this);
  }

  ~SimSendSync() override = default;

  void Display(std::ostream &out) override;

  bool Equal(const Event &other) override;
};

class SimProcInEvent : public Event {
 public:
  explicit SimProcInEvent(uint64_t timestamp, const size_t parser_identifier,
                          const std::string &parser_name)
      : Event(timestamp, parser_identifier, parser_name,
              EventType::kSimProcInEventT, "SimProcInEvent") {
  }

  SimProcInEvent(const SimProcInEvent &other) = default;

  Event *clone() override {
    return new SimProcInEvent(*this);
  }

  ~SimProcInEvent() override = default;

  void Display(std::ostream &out) override;

  bool Equal(const Event &other) override;
};

/* Host related events */

class HostInstr : public Event {

  uint64_t pc_;

 public:
  HostInstr(uint64_t timestamp, const size_t parser_identifier,
            const std::string &parser_name, uint64_t pc)
      : Event(timestamp, parser_identifier, parser_name,
              EventType::kHostInstrT, "HostInstr"),
        pc_(pc) {
  }

  HostInstr(uint64_t timestamp, size_t parser_identifier, const std::string &parser_name,
            uint64_t pc, EventType type, const std::string &&name)
      : Event(timestamp, parser_identifier, parser_name, type, std::move(name)),
        pc_(pc) {
  }

  HostInstr(const HostInstr &other) = default;

  Event *clone() override {
    return new HostInstr(*this);
  }

  uint64_t GetPc() const;

  ~HostInstr() override = default;

  void Display(std::ostream &out) override;

  bool Equal(const Event &other) override;
};

class HostCall : public HostInstr {

  const std::string *func_;
  const std::string *comp_;

 public:
  explicit HostCall(uint64_t timestamp, const size_t parser_identifier,
                    const std::string &parser_name, uint64_t pc,
                    const std::string *func,
                    const std::string *comp)
      : HostInstr(timestamp, parser_identifier, parser_name, pc,
                  EventType::kHostCallT, "HostCall"),
        func_(func),
        comp_(comp) {
  }

  HostCall(const HostCall &other) = default;

  Event *clone() override {
    return new HostCall(*this);
  }

  ~HostCall() override = default;

  void Display(std::ostream &out) override;

  bool Equal(const Event &other) override;

  const std::string *GetFunc() const;

  const std::string *GetComp() const;
};

class HostMmioImRespPoW : public Event {
 public:
  explicit HostMmioImRespPoW(uint64_t timestamp, const size_t parser_identifier,
                             const std::string &parser_name)
      : Event(timestamp, parser_identifier, parser_name,
              EventType::kHostMmioImRespPoWT, "HostMmioImRespPoW") {
  }

  HostMmioImRespPoW(const HostMmioImRespPoW &other) = default;

  Event *clone() override {
    return new HostMmioImRespPoW(*this);
  }

  ~HostMmioImRespPoW() override = default;

  void Display(std::ostream &out) override;

  bool Equal(const Event &other) override;
};

class HostIdOp : public Event {
  uint64_t id_;

 public:
  void Display(std::ostream &out) override;

  uint64_t GetId() const;

  Event *clone() override {
    return new HostIdOp(*this);
  }

 protected:
  explicit HostIdOp(uint64_t timestamp, const size_t parser_identifier,
                    const std::string &parser_name, EventType type,
                    const std::string &&name, uint64_t ident)
      : Event(timestamp, parser_identifier, parser_name, type,
              std::move(name)),
        id_(ident) {
  }

  HostIdOp(const HostIdOp &other) = default;

  ~HostIdOp() override = default;

  bool Equal(const Event &other) override;
};

class HostMmioCR : public HostIdOp {
 public:
  explicit HostMmioCR(uint64_t timestamp, const size_t parser_identifier,
                      const std::string &parser_name, uint64_t ident)
      : HostIdOp(timestamp, parser_identifier, parser_name,
                 EventType::kHostMmioCRT, "HostMmioCR", ident) {
  }

  HostMmioCR(const HostMmioCR &other) = default;

  Event *clone() override {
    return new HostMmioCR(*this);
  }

  ~HostMmioCR() override = default;

  void Display(std::ostream &out) override;

  bool Equal(const Event &other) override;
};
class HostMmioCW : public HostIdOp {
 public:
  explicit HostMmioCW(uint64_t timestamp, const size_t parser_identifier,
                      const std::string &parser_name, uint64_t ident)
      : HostIdOp(timestamp, parser_identifier, parser_name,
                 EventType::kHostMmioCWT, "HostMmioCW", ident) {
  }

  HostMmioCW(const HostMmioCW &other) = default;

  Event *clone() override {
    return new HostMmioCW(*this);
  }

  ~HostMmioCW() override = default;

  void Display(std::ostream &out) override;

  bool Equal(const Event &other) override;
};

class HostAddrSizeOp : public HostIdOp {

  uint64_t addr_;
  size_t size_;

 public:
  void Display(std::ostream &out) override;

  uint64_t GetAddr() const;

  size_t GetSize() const;

  Event *clone() override {
    return new HostAddrSizeOp(*this);
  }

 protected:
  explicit HostAddrSizeOp(uint64_t timestamp, const size_t parser_identifier,
                          const std::string &parser_name, EventType type,
                          const std::string &&name, uint64_t ident, uint64_t addr,
                          size_t size)
      : HostIdOp(timestamp, parser_identifier, parser_name, type,
                 std::move(name), ident),
        addr_(addr),
        size_(size) {
  }

  HostAddrSizeOp(const HostAddrSizeOp &other) = default;

  ~HostAddrSizeOp() override = default;

  bool Equal(const Event &other) override;
};

class HostMmioOp : public HostAddrSizeOp {

  int bar_;
  uint64_t offset_;

 public:
  void Display(std::ostream &out) override;

  int GetBar() const;

  uint64_t GetOffset() const;

  Event *clone() override {
    return new HostMmioOp(*this);
  }

 protected:
  explicit HostMmioOp(uint64_t timestamp, const size_t parser_identifier,
                      const std::string &parser_name, EventType type,
                      const std::string &&name, uint64_t ident, uint64_t addr,
                      size_t size, int bar, uint64_t offset)
      : HostAddrSizeOp(timestamp, parser_identifier, parser_name, type,
                       std::move(name), ident, addr, size), bar_(bar), offset_(offset) {
  }

  HostMmioOp(const HostMmioOp &other) = default;

  bool Equal(const Event &other) override;
};

class HostMmioR : public HostMmioOp {
 public:
  explicit HostMmioR(uint64_t timestamp, const size_t parser_identifier,
                     const std::string &parser_name, uint64_t ident, uint64_t addr,
                     size_t size, int bar, uint64_t offset)
      : HostMmioOp(timestamp, parser_identifier, parser_name,
                   EventType::kHostMmioRT, "HostMmioR", ident, addr, size, bar,
                   offset) {
  }

  HostMmioR(const HostMmioR &other) = default;

  Event *clone() override {
    return new HostMmioR(*this);
  }

  ~HostMmioR() override = default;

  void Display(std::ostream &out) override;

  bool Equal(const Event &other) override;
};

class HostMmioW : public HostMmioOp {
  bool posted_ = false;

 public:
  bool IsPosted() const;

  explicit HostMmioW(uint64_t timestamp, const size_t parser_identifier,
                     const std::string &parser_name, uint64_t ident, uint64_t addr,
                     size_t size, int bar, uint64_t offset, bool posted)
      : HostMmioOp(timestamp, parser_identifier, parser_name,
                   EventType::kHostMmioWT, "HostMmioW", ident, addr, size, bar,
                   offset), posted_(posted) {
  }

  HostMmioW(const HostMmioW &other) = default;

  Event *clone() override {
    return new HostMmioW(*this);
  }

  ~HostMmioW() override = default;

  void Display(std::ostream &out) override;

  bool Equal(const Event &other) override;
};

class HostDmaC : public HostIdOp {
 public:
  explicit HostDmaC(uint64_t timestamp, const size_t parser_identifier,
                    const std::string &parser_name, uint64_t ident)
      : HostIdOp(timestamp, parser_identifier, parser_name,
                 EventType::kHostDmaCT, "HostDmaC", ident) {
  }

  HostDmaC(const HostDmaC &other) = default;

  Event *clone() override {
    return new HostDmaC(*this);
  }

  ~HostDmaC() override = default;

  void Display(std::ostream &out) override;

  bool Equal(const Event &other) override;
};

class HostDmaR : public HostAddrSizeOp {
 public:
  explicit HostDmaR(uint64_t timestamp, const size_t parser_identifier,
                    const std::string &parser_name, uint64_t ident, uint64_t addr,
                    size_t size)
      : HostAddrSizeOp(timestamp, parser_identifier, parser_name,
                       EventType::kHostDmaRT, "HostDmaR", ident, addr, size) {
  }

  HostDmaR(const HostDmaR &other) = default;

  Event *clone() override {
    return new HostDmaR(*this);
  }

  ~HostDmaR() override = default;

  void Display(std::ostream &out) override;

  bool Equal(const Event &other) override;
};

class HostDmaW : public HostAddrSizeOp {
 public:
  explicit HostDmaW(uint64_t timestamp, const size_t parser_identifier,
                    const std::string &parser_name, uint64_t ident, uint64_t addr,
                    size_t size)
      : HostAddrSizeOp(timestamp, parser_identifier, parser_name,
                       EventType::kHostDmaWT, "HostDmaW", ident, addr, size) {
  }

  HostDmaW(const HostDmaW &other) = default;

  Event *clone() override {
    return new HostDmaW(*this);
  }

  ~HostDmaW() override = default;

  void Display(std::ostream &out) override;

  bool Equal(const Event &other) override;
};

class HostMsiX : public Event {

  uint64_t vec_;

 public:
  explicit HostMsiX(uint64_t timestamp, const size_t parser_identifier,
                    const std::string &parser_name, uint64_t vec)
      : Event(timestamp, parser_identifier, parser_name,
              EventType::kHostMsiXT, "HostMsiX"),
        vec_(vec) {
  }

  HostMsiX(const HostMsiX &other) = default;

  Event *clone() override {
    return new HostMsiX(*this);
  }

  ~HostMsiX() override = default;

  void Display(std::ostream &out) override;

  bool Equal(const Event &other) override;

  uint64_t GetVec() const;
};

class HostConf : public Event {

  uint64_t dev_;
  uint64_t func_;
  uint64_t reg_;
  size_t bytes_;
  uint64_t data_;
  bool is_read_;

 public:
  explicit HostConf(uint64_t timestamp, const size_t parser_identifier,
                    const std::string &parser_name, uint64_t dev, uint64_t func,
                    uint64_t reg, size_t bytes, uint64_t data, bool is_read)
      : Event(timestamp, parser_identifier, parser_name,
              EventType::kHostConfT,
              is_read ? "HostConfRead" : "HostConfWrite"),
        dev_(dev),
        func_(func),
        reg_(reg),
        bytes_(bytes),
        data_(data),
        is_read_(is_read) {
  }

  HostConf(const HostConf &other) = default;

  Event *clone() override {
    return new HostConf(*this);
  }

  ~HostConf() override = default;

  void Display(std::ostream &out) override;

  bool Equal(const Event &other) override;

  uint64_t GetDev() const;

  uint64_t GetFunc() const;

  uint64_t GetReg() const;

  size_t GetBytes() const;

  uint64_t GetData() const;

  bool IsRead() const;
};

class HostClearInt : public Event {
 public:
  explicit HostClearInt(uint64_t timestamp, const size_t parser_identifier,
                        const std::string &parser_name)
      : Event(timestamp, parser_identifier, parser_name,
              EventType::kHostClearIntT, "HostClearInt") {
  }

  HostClearInt(const HostClearInt &other) = default;

  Event *clone() override {
    return new HostClearInt(*this);
  }

  ~HostClearInt() override = default;

  void Display(std::ostream &out) override;

  bool Equal(const Event &other) override;
};

class HostPostInt : public Event {
 public:
  explicit HostPostInt(uint64_t timestamp, const size_t parser_identifier,
                       const std::string &parser_name)
      : Event(timestamp, parser_identifier, parser_name,
              EventType::kHostPostIntT, "HostPostInt") {
  }

  HostPostInt(const HostPostInt &other) = default;

  Event *clone() override {
    return new HostPostInt(*this);
  }

  ~HostPostInt() override = default;

  void Display(std::ostream &out) override;

  bool Equal(const Event &other) override;
};

class HostPciRW : public Event {

  uint64_t offset_;
  size_t size_;
  bool is_read_;

 public:

  explicit HostPciRW(uint64_t timestamp, const size_t parser_identifier,
                     const std::string &parser_name, uint64_t offset,
                     size_t size, bool is_read)
      : Event(timestamp, parser_identifier, parser_name,
              EventType::kHostPciRWT, is_read ? "HostPciR" : "HostPciW"),
        offset_(offset),
        size_(size),
        is_read_(is_read) {
  }

  HostPciRW(const HostPciRW &other) = default;

  Event *clone() override {
    return new HostPciRW(*this);
  }

  ~HostPciRW() override = default;

  void Display(std::ostream &out) override;

  bool Equal(const Event &other) override;

  uint64_t GetOffset() const;

  size_t GetSize() const;

  bool IsRead() const;
};

/* NIC related events */
class NicMsix : public Event {

  uint16_t vec_;
  bool isX_;

 public:

  NicMsix(uint64_t timestamp, const size_t parser_identifier,
          const std::string &parser_name, uint16_t vec, bool isX)
      : Event(timestamp, parser_identifier, parser_name,
              EventType::kNicMsixT, isX ? "NicMsix" : "NicMsi"),
        vec_(vec),
        isX_(isX) {
  }

  NicMsix(const NicMsix &other) = default;

  Event *clone() override {
    return new NicMsix(*this);
  }

  ~NicMsix() override = default;

  void Display(std::ostream &out) override;

  bool Equal(const Event &other) override;

  uint16_t GetVec() const;

  bool IsX() const;
};

class NicDma : public Event {

  uint64_t id_;
  uint64_t addr_;
  size_t len_;

 public:

  void Display(std::ostream &out) override;

  uint64_t GetId() const;

  uint64_t GetAddr() const;

  size_t GetLen() const;

  Event *clone() override {
    return new NicDma(*this);
  }

 protected:
  NicDma(uint64_t timestamp, const size_t parser_identifier,
         const std::string &parser_name, EventType type, const std::string &&name,
         uint64_t ident, uint64_t addr, size_t len)
      : Event(timestamp, parser_identifier, parser_name, type,
              std::move(name)),
        id_(ident),
        addr_(addr),
        len_(len) {
  }

  NicDma(const NicDma &other) = default;

  ~NicDma() override = default;

  bool Equal(const Event &other) override;
};

class SetIX : public Event {

  uint64_t intr_;

 public:

  SetIX(uint64_t timestamp, const size_t parser_identifier,
        const std::string &parser_name, uint64_t intr)
      : Event(timestamp, parser_identifier, parser_name, EventType::kSetIXT,
              "SetIX"),
        intr_(intr) {
  }

  SetIX(const SetIX &other) = default;

  Event *clone() override {
    return new SetIX(*this);
  }

  ~SetIX() override = default;

  void Display(std::ostream &out) override;

  bool Equal(const Event &other) override;

  uint64_t GetIntr() const;
};

class NicDmaI : public NicDma {
 public:
  NicDmaI(uint64_t timestamp, const size_t parser_identifier,
          const std::string &parser_name, uint64_t id, uint64_t addr,
          size_t len)
      : NicDma(timestamp, parser_identifier, parser_name,
               EventType::kNicDmaIT, "NicDmaI", id, addr, len) {
  }

  NicDmaI(const NicDmaI &other) = default;

  Event *clone() override {
    return new NicDmaI(*this);
  }

  ~NicDmaI() override = default;

  void Display(std::ostream &out) override;

  bool Equal(const Event &other) override;
};

class NicDmaEx : public NicDma {
 public:
  NicDmaEx(uint64_t timestamp, const size_t parser_identifier,
           const std::string &parser_name, uint64_t id, uint64_t addr,
           size_t len)
      : NicDma(timestamp, parser_identifier, parser_name,
               EventType::kNicDmaExT, "NicDmaEx", id, addr, len) {
  }

  NicDmaEx(const NicDmaEx &other) = default;

  Event *clone() override {
    return new NicDmaEx(*this);
  }

  ~NicDmaEx() override = default;

  void Display(std::ostream &out) override;

  bool Equal(const Event &other) override;
};

class NicDmaEn : public NicDma {
 public:
  NicDmaEn(uint64_t timestamp, const size_t parser_identifier,
           const std::string &parser_name, uint64_t id, uint64_t addr,
           size_t len)
      : NicDma(timestamp, parser_identifier, parser_name,
               EventType::kNicDmaEnT, "NicDmaEn", id, addr, len) {
  }

  NicDmaEn(const NicDmaEn &other) = default;

  Event *clone() override {
    return new NicDmaEn(*this);
  }

  ~NicDmaEn() override = default;

  void Display(std::ostream &out) override;

  bool Equal(const Event &other) override;
};

class NicDmaCR : public NicDma {
 public:
  NicDmaCR(uint64_t timestamp, const size_t parser_identifier,
           const std::string &parser_name, uint64_t id, uint64_t addr,
           size_t len)
      : NicDma(timestamp, parser_identifier, parser_name,
               EventType::kNicDmaCRT, "NicDmaCR", id, addr, len) {
  }

  NicDmaCR(const NicDmaCR &other) = default;

  Event *clone() override {
    return new NicDmaCR(*this);
  }

  ~NicDmaCR() override = default;

  void Display(std::ostream &out) override;

  bool Equal(const Event &other) override;
};

class NicDmaCW : public NicDma {
 public:
  NicDmaCW(uint64_t timestamp, const size_t parser_identifier,
           const std::string &parser_name, uint64_t id, uint64_t addr,
           size_t len)
      : NicDma(timestamp, parser_identifier, parser_name,
               EventType::kNicDmaCWT, "NicDmaCW", id, addr, len) {
  }

  NicDmaCW(const NicDmaCW &other) = default;

  Event *clone() override {
    return new NicDmaCW(*this);
  }

  ~NicDmaCW() override = default;

  void Display(std::ostream &out) override;

  bool Equal(const Event &other) override;
};

class NicMmio : public Event {

  uint64_t off_;
  size_t len_;
  uint64_t val_;

 public:

  void Display(std::ostream &out) override;

  uint64_t GetOff() const;

  size_t GetLen() const;

  uint64_t GetVal() const;

  Event *clone() override {
    return new NicMmio(*this);
  }

 protected:
  NicMmio(uint64_t timestamp, const size_t parser_identifier,
          const std::string &parser_name, EventType type, const std::string &&name,
          uint64_t off, size_t len, uint64_t val)
      : Event(timestamp, parser_identifier, parser_name, type, std::move(name)),
        off_(off),
        len_(len),
        val_(val) {
  }

  NicMmio(const NicMmio &other) = default;

  ~NicMmio() override = default;

  bool Equal(const Event &other) override;
};

class NicMmioR : public NicMmio {
 public:
  NicMmioR(uint64_t timestamp, const size_t parser_identifier,
           const std::string &parser_name, uint64_t off, size_t len,
           uint64_t val)
      : NicMmio(timestamp, parser_identifier, parser_name,
                EventType::kNicMmioRT, "NicMmioR", off, len, val) {
  }

  NicMmioR(const NicMmioR &other) = default;

  Event *clone() override {
    return new NicMmioR(*this);
  }

  ~NicMmioR() override = default;

  void Display(std::ostream &out) override;

  bool Equal(const Event &other) override;
};

class NicMmioW : public NicMmio {
  bool posted_ = false;

 public:

  bool IsPosted() const;

  NicMmioW(uint64_t timestamp, const size_t parser_identifier,
           const std::string &parser_name, uint64_t off, size_t len,
           uint64_t val, bool posted)
      : NicMmio(timestamp, parser_identifier, parser_name,
                EventType::kNicMmioWT, "NicMmioW", off, len, val), posted_(posted) {
  }

  NicMmioW(const NicMmioW &other) = default;

  Event *clone() override {
    return new NicMmioW(*this);
  }

  ~NicMmioW() override = default;

  void Display(std::ostream &out) override;

  bool Equal(const Event &other) override;
};

class NicTrx : public Event {
  size_t len_;
  bool is_read_;

 public:

  void Display(std::ostream &out) override;

  size_t GetLen() const;

  bool IsRead() const {
    return is_read_;
  }

  Event *clone() override {
    return new NicTrx(*this);
  }

 protected:
  NicTrx(uint64_t timestamp, const size_t parser_identifier,
         const std::string &parser_name, EventType type, const std::string &&name,
         size_t len, bool is_read)
      : Event(timestamp, parser_identifier, parser_name, type, std::move(name)),
        len_(len), is_read_(is_read) {
  }

  NicTrx(const NicTrx &other) = default;

  ~NicTrx() override = default;

  bool Equal(const Event &other) override;
};

class NicTx : public NicTrx {
 public:
  NicTx(uint64_t timestamp, const size_t parser_identifier,
        const std::string &parser_name, size_t len)
      : NicTrx(timestamp, parser_identifier, parser_name,
               EventType::kNicTxT, "NicTx", len, false) {
  }

  NicTx(const NicTx &other) = default;

  Event *clone() override {
    return new NicTx(*this);
  }

  ~NicTx() override = default;

  void Display(std::ostream &out) override;

  bool Equal(const Event &other) override;
};

class NicRx : public NicTrx {
  int port_;

 public:
  NicRx(uint64_t timestamp, const size_t parser_identifier,
        const std::string &parser_name, int port, size_t len)
      : NicTrx(timestamp, parser_identifier, parser_name,
               EventType::kNicRxT, "NicRx", len, true),
        port_(port) {
  }

  NicRx(const NicRx &other) = default;

  Event *clone() override {
    return new NicRx(*this);
  }

  ~NicRx() override = default;

  void Display(std::ostream &out) override;

  bool Equal(const Event &other) override;

  int GetPort() const;
};

class NetworkEvent : public Event {

 public:

  struct MacAddress {
    static constexpr size_t kMacSize = 6;
    std::array<unsigned char, kMacSize> addr_ = {0, 0, 0, 0, 0, 0};

    explicit MacAddress() = default;

    explicit MacAddress(const std::array<unsigned char, kMacSize> &addr)
        : addr_(addr) {}

    void Display(std::ostream &out) const;

    bool operator==(const MacAddress &other) const {
      return std::ranges::equal(addr_, other.addr_);
    }
  };

  struct EthernetHeader {
    size_t length_type_ = 0;
    MacAddress src_mac_;
    MacAddress dst_mac_;

    void Display(std::ostream &out) const;

    bool operator==(const EthernetHeader &other) const {
      return src_mac_ == other.src_mac_ and dst_mac_ == other.dst_mac_ and length_type_ == other.length_type_;
    }
  };

  struct Ipv4 {
    uint32_t ip_ = 0;

    explicit Ipv4() = default;

    explicit Ipv4(uint32_t ipa) : ip_(ipa) {}

    void Display(std::ostream &out) const;

    bool operator==(const Ipv4 &other) const {
      return ip_ == other.ip_;
    }
  };

  struct ArpHeader {
    // MacAddress src_mac_;
    Ipv4 src_ip_;
    Ipv4 dst_ip_;
    bool is_request_ = false;

    void Display(std::ostream &out) const;

    bool operator==(const ArpHeader &other) const {
      return /*src_mac_ == other.src_mac_ and*/ src_ip_ == other.src_ip_ and dst_ip_ == other.dst_ip_
          and is_request_ == other.is_request_;
    }
  };

  struct Ipv4Header {
    // NOTE: currently only Ipv4 is supported!!
    size_t length_;
    Ipv4 src_ip_;
    Ipv4 dst_ip_;

    void Display(std::ostream &out) const;

    bool operator==(const Ipv4Header &other) const {
      return length_ == other.length_ and src_ip_ == other.src_ip_ and dst_ip_ == other.dst_ip_;
    }
  };

  enum NetworkDeviceType {
    kCosimNetDevice,
    kSimpleNetDevice
  };

  enum EventBoundaryType {
    kWithinSimulator,
    kFromAdapter,
    kToAdapter
  };

 private:
  const int node_;
  const int device_;
  const NetworkDeviceType device_type_;
  const uint64_t packet_uid_;
  const bool interesting_flag_;
  const size_t payload_size_ = 0;
  const EventBoundaryType boundary_type_;
  const std::optional<EthernetHeader> ethernet_header_;
  const std::optional<ArpHeader> arp_header_;
  const std::optional<Ipv4Header> ip_header_;

 public:

  void Display(std::ostream &out) override;

  Event *clone() override {
    return new NetworkEvent(*this);
  }

  int GetNode() const;

  int GetDevice() const;

  NetworkDeviceType GetDeviceType() const;

  uint64_t GetPacketUid() const;

  bool InterestingFlag() const;

  size_t GetPayloadSize() const;

  EventBoundaryType GetBoundaryType() const;

  bool IsBoundaryType(EventBoundaryType boundary_type) const;

  bool HasEthernetHeader() const;

  const EthernetHeader &GetEthernetHeader() const;

  bool HasArpHeader() const;

  const ArpHeader &GetArpHeader() const;

  bool HasIpHeader() const;

  const Ipv4Header &GetIpHeader() const;

  bool Equal(const Event &other) override;

 protected:
  explicit NetworkEvent(uint64_t timetsamp,
                        const size_t parser_identifier,
                        const std::string &parser_name,
                        EventType type,
                        const std::string &&name,
                        int node,
                        int device,
                        const NetworkDeviceType device_type,
                        uint64_t packet_uid,
                        bool interesting_flag,
                        size_t payload_size,
                        const EventBoundaryType boundary_type,
                        const std::optional<EthernetHeader> &ethernet_header = std::nullopt,
                        const std::optional<ArpHeader> arp_header = std::nullopt,
                        const std::optional<Ipv4Header> &ip_header = std::nullopt)
      : Event(timetsamp, parser_identifier, parser_name, type, std::move(name)),
        node_(node),
        device_(device),
        device_type_(device_type),
        packet_uid_(packet_uid),
        interesting_flag_(interesting_flag),
        payload_size_(payload_size),
        boundary_type_(boundary_type),
        ethernet_header_(ethernet_header),
        arp_header_(arp_header),
        ip_header_(ip_header) {
  }

  NetworkEvent(const NetworkEvent &other) = default;

  ~NetworkEvent() override = default;
};

inline std::ostream &operator<<(std::ostream &out, NetworkEvent::NetworkDeviceType device) {
  switch (device) {
    case NetworkEvent::NetworkDeviceType::kCosimNetDevice: {
      out << "ns3::CosimNetDevice";
      break;
    }
    case NetworkEvent::NetworkDeviceType::kSimpleNetDevice: {
      out << "ns3::SimpleNetDevice";
      break;
    }
    default:out << "unknown";
      break;
  }
  return out;
}

inline std::ostream &operator<<(std::ostream &out, NetworkEvent::EventBoundaryType type) {
  switch (type) {
    case NetworkEvent::EventBoundaryType::kWithinSimulator: {
      out << "kWithinSimulator";
      break;
    }
    case NetworkEvent::EventBoundaryType::kFromAdapter: {
      out << "kFromAdapter";
      break;
    }
    case NetworkEvent::EventBoundaryType::kToAdapter: {
      out << "kToAdapter";
      break;
    }
    default:out << "unknown";
      break;
  }
  return out;
}

inline std::ostream &operator<<(std::ostream &out, const NetworkEvent::MacAddress &mac) {
  mac.Display(out);
  return out;
}

inline std::ostream &operator<<(std::ostream &out, const NetworkEvent::Ipv4 &ipv_4) {
  ipv_4.Display(out);
  return out;
}

inline bool IsDeviceType(const std::shared_ptr<NetworkEvent> &event, NetworkEvent::NetworkDeviceType device_type) {
  return event and event->GetDeviceType() == device_type;
}

inline std::string IpToString(const NetworkEvent::Ipv4 &ipv_4) {
  std::stringstream out;
  ipv_4.Display(out);
  return out.str();
}

template<>
struct std::hash<NetworkEvent::Ipv4> {
  std::size_t operator()(const NetworkEvent::Ipv4 &ipv4) const {
    return ipv4.ip_;
  }
};

class NetworkEnqueue : public NetworkEvent {
 public:
  explicit NetworkEnqueue(uint64_t timetsamp,
                          const size_t parser_identifier,
                          const std::string &parser_name,
                          int node,
                          int device,
                          const NetworkDeviceType device_type,
                          uint64_t packet_uid,
                          bool interesting_flag,
                          size_t payload_size,
                          const EventBoundaryType boundary_type,
                          const std::optional<EthernetHeader> &ethernet_header = std::nullopt,
                          const std::optional<ArpHeader> arp_header = std::nullopt,
                          const std::optional<Ipv4Header> &ip_header = std::nullopt)
      : NetworkEvent(timetsamp,
                     parser_identifier,
                     parser_name,
                     EventType::kNetworkEnqueueT,
                     "NetworkEnqueue",
                     node,
                     device,
                     device_type,
                     packet_uid,
                     interesting_flag,
                     payload_size,
                     boundary_type,
                     ethernet_header,
                     arp_header,
                     ip_header) {}

  NetworkEnqueue(const NetworkEnqueue &other) = default;

  Event *clone() override {
    return new NetworkEnqueue(*this);
  }

  ~NetworkEnqueue() override = default;

  void Display(std::ostream &out) override;

  bool Equal(const Event &other) override;
};

class NetworkDequeue : public NetworkEvent {
 public:
  explicit NetworkDequeue(uint64_t timetsamp,
                          const size_t parser_identifier,
                          const std::string &parser_name,
                          int node,
                          int device,
                          const NetworkDeviceType device_type,
                          uint64_t packet_uid,
                          bool interesting_flag,
                          size_t payload_size,
                          const EventBoundaryType boundary_type,
                          const std::optional<EthernetHeader> &ethernet_header = std::nullopt,
                          const std::optional<ArpHeader> arp_header = std::nullopt,
                          const std::optional<Ipv4Header> &ip_header = std::nullopt)
      : NetworkEvent(timetsamp,
                     parser_identifier,
                     parser_name,
                     EventType::kNetworkDequeueT,
                     "NetworkDequeue",
                     node,
                     device,
                     device_type,
                     packet_uid,
                     interesting_flag,
                     payload_size,
                     boundary_type,
                     ethernet_header,
                     arp_header,
                     ip_header) {}

  NetworkDequeue(const NetworkDequeue &other) = default;

  Event *clone() override {
    return new NetworkDequeue(*this);
  }

  ~NetworkDequeue() override = default;

  void Display(std::ostream &out) override;

  bool Equal(const Event &other) override;
};

class NetworkDrop : public NetworkEvent {
 public:
  explicit NetworkDrop(uint64_t timetsamp,
                       const size_t parser_identifier,
                       const std::string &parser_name,
                       int node,
                       int device,
                       const NetworkDeviceType device_type,
                       uint64_t packet_uid,
                       bool interesting_flag,
                       size_t payload_size,
                       const EventBoundaryType boundary_type,
                       const std::optional<EthernetHeader> &ethernet_header = std::nullopt,
                       const std::optional<ArpHeader> arp_header = std::nullopt,
                       const std::optional<Ipv4Header> &ip_header = std::nullopt)
      : NetworkEvent(timetsamp,
                     parser_identifier,
                     parser_name,
                     EventType::kNetworkDropT,
                     "NetworkDrop",
                     node,
                     device,
                     device_type,
                     packet_uid,
                     interesting_flag,
                     payload_size,
                     boundary_type,
                     ethernet_header,
                     arp_header,
                     ip_header) {}

  NetworkDrop(const NetworkDrop &other) = default;

  Event *clone() override {
    return new NetworkDrop(*this);
  }

  ~NetworkDrop() override = default;

  void Display(std::ostream &out) override;

  bool Equal(const Event &other) override;
};

inline std::shared_ptr<Event> CloneShared(const std::shared_ptr<Event> &other) {
  if (not other) {
    return {};
  }
  auto raw_ptr = other->clone();
  throw_if_empty(raw_ptr, "CloneShared: cloned raw pointer is null", source_loc::current());
  return std::shared_ptr<Event>(raw_ptr);
}

bool IsType(const std::shared_ptr<Event> &event_ptr, EventType type);

bool IsAnyType(const std::shared_ptr<Event> &event_ptr, const std::vector<EventType> &types);

bool IsAnyType(const std::shared_ptr<Event> &event_ptr, const std::set<EventType> &types);

bool IsType(const Event &event, EventType type);

inline bool IsBoundaryType(const std::shared_ptr<Event> &event) {
  if (event and IsAnyType(event, std::vector<EventType>{
      EventType::kNetworkEnqueueT, EventType::kNetworkDequeueT, EventType::kNetworkDropT})) {
    return IsBoundaryType(std::static_pointer_cast<NetworkEvent>(event));
  }
  return false;
}

inline bool IsBoundaryType(const std::shared_ptr<NetworkEvent> &event,
                           NetworkEvent::EventBoundaryType boundary_type) {
  return event and event->GetBoundaryType() == boundary_type;
}

struct EventComperator {
  bool operator()(const std::shared_ptr<Event> &ev1,
                  const std::shared_ptr<Event> &ev2) const {
    return ev1->GetTs() > ev2->GetTs();
  }
};

inline std::ostream &operator<<(std::ostream &out, Event &event) {
  event.Display(out);
  return out;
}

inline std::string GetTypeStr(const std::shared_ptr<Event> &event) {
  if (not event) {
    return "";
  }
  std::stringstream sss;
  sss << event->GetType();
  return std::move(sss.str());
}

#endif  // SIMBRICKS_TRACE_EVENTS_H_
