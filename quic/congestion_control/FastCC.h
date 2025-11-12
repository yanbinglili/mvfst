#pragma once

#include <quic/state/StateData.h>
#include <quic/congestion_control/CongestionController.h>
#include <quic/congestion_control/Pico_FastCC.h>

namespace quic {

  class FastCC : public CongestionController {
  public:
    explicit FastCC(QuicConnectionStateBase& conn);
    ~FastCC() override {
      picoquic_fastcc_delete(&view_);
    }

    void onPacketSent(const OutstandingPacketWrapper& p) override;
    void onRemoveBytesFromInflight(uint64_t b) override;
    void setAppLimited() noexcept override;
    void setAppIdle(bool /*idle*/, TimePoint /*eventTime*/) noexcept override {}
    void getStats(CongestionControllerStats& /*stats*/) const override {}

    void onPacketAckOrLoss(
        const AckEvent* FOLLY_NULLABLE ackEvent,
        const LossEvent* FOLLY_NULLABLE lossEvent) override;

    [[nodiscard]] bool isAppLimited() const override { return appLimited_; }
    [[nodiscard]] uint64_t getWritableBytes() const noexcept override;
    [[nodiscard]] uint64_t getCongestionWindow() const noexcept override;
    [[nodiscard]] CongestionControlType type() const noexcept override {
      return CongestionControlType::FastCC;
    }

  private:
    void captureState();
    void updatePacing();
    static uint64_t nowUs();

  private:
    QuicConnectionStateBase& conn_;
    bool appLimited_{false};
    TimePoint appLimitedExitTarget_{};

    // 只做去抖用途，避免重复触发
    uint32_t prevCECount{0};
    uint32_t prevSpuriousCount{0};
    bool seededCwnd_{false};

    picoquic_path_t view_;
  };

} // namespace quic
