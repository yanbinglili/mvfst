//
// Created by liyan on 2025-07-22.
//

#pragma once

#include <folly/io/async/AsyncTimeout.h>
#include <quic/congestion_control/CongestionController.h>
#include <quic/state/StateData.h>


namespace quic {

class SwitchableCC_old final : public CongestionController, public folly::AsyncTimeout{
  public:
  explicit SwitchableCC_old(QuicConnectionStateBase& conn,
                          CongestionControlType init=CongestionControlType::NewReno,
                          folly::EventBase* evb = nullptr,
                           uint32_t switchInterval = 5000)
        : AsyncTimeout(evb), conn_(conn), cur_(init), switchInterval_(switchInterval) {
    testAlgorithms_ = {
      CongestionControlType::Cubic,
      CongestionControlType::BBR,
      CongestionControlType::NewReno
  };

    switchTo(testAlgorithms_.front());

    scheduleTimeout(switchInterval_);

    VLOG(4) << "Simple Timer SwitchableCC initialized";
    }

  void timeoutExpired() noexcept override {
    switchTo(getNextAlgorithm());

    scheduleTimeout(switchInterval_);
  }

  CongestionControlType getNextAlgorithm() {
    currentIndex_ = (currentIndex_ + 1) % testAlgorithms_.size();
    return testAlgorithms_[currentIndex_];
  }

  std::unique_ptr<CongestionController> makeCCPtr(QuicConnectionStateBase& conn,
                        CongestionControlType type) const;

  void switchTo(CongestionControlType type);

  void onPacketSent(const OutstandingPacketWrapper& p) override { impl_->onPacketSent(p); }
  void onRemoveBytesFromInflight(uint64_t b) override { impl_->onRemoveBytesFromInflight(b); }
  void setAppLimited() override { impl_->setAppLimited(); }
  void setAppIdle(bool idle, TimePoint eventTime) override { impl_->setAppIdle(idle, eventTime); }
  void getStats(CongestionControllerStats& stats) const override { impl_->getStats(stats); }
  void setExperimental(bool experimental) override { impl_->setExperimental(experimental); }
  void onPacketAckOrLoss(
      const AckEvent* FOLLY_NULLABLE ackEvent,
      const LossEvent* FOLLY_NULLABLE lossEvent) override {
      impl_->onPacketAckOrLoss(ackEvent, lossEvent);
  }

  [[nodiscard]] Optional<Bandwidth> getBandwidth() const override { return impl_->getBandwidth(); }
  [[nodiscard]] uint64_t getBDP() const override { return impl_->getBDP(); }
  [[nodiscard]] bool isAppLimited() const override {return impl_->isAppLimited(); }
  [[nodiscard]] uint64_t getWritableBytes() const override { return impl_->getWritableBytes(); }
  [[nodiscard]] uint64_t getCongestionWindow() const override { return impl_->getCongestionWindow(); }
  [[nodiscard]] CongestionControlType type() const noexcept override { return cur_; }

  private:
  QuicConnectionStateBase& conn_;
  std::unordered_map<CongestionControlType, std::unique_ptr<CongestionController>> pool_;

  CongestionControlType cur_{CongestionControlType::Cubic};
  CongestionController* impl_{nullptr};

  std::chrono::milliseconds switchInterval_;
  std::vector<CongestionControlType> testAlgorithms_;
  size_t currentIndex_{0};
  };
}
