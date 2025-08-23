//
// Created by liyan on 2025-07-31.
//

#pragma once

#include <quic/congestion_control/CongestionController.h>
#include <quic/state/StateData.h>
#include <folly/io/async/AsyncUDPSocket.h>
#include <map>
#include <memory>

namespace quic {

class SwitchableCC : public CongestionController,
                     public folly::AsyncUDPSocket::ReadCallback {
public:
    explicit SwitchableCC(QuicConnectionStateBase& conn,
                        folly::EventBase* evb = nullptr);

    ~SwitchableCC() override = default;

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

    // --- folly::AsyncUDPSocket::ReadCallback 接口 ---
    void onReadError(const folly::AsyncSocketException& ex) noexcept override;
    void onReadClosed() noexcept override;
    void getReadBuffer(void** buf, size_t* len) noexcept override;
    void onDataAvailable(
        const folly::SocketAddress& client,
        size_t len,
        bool truncated,
        OnDataAvailableParams params) noexcept override;

    std::unique_ptr<CongestionController> makeCCPtr(QuicConnectionStateBase& conn,
                  CongestionControlType type) const;

    void switchTo(CongestionControlType type);

private:
    QuicConnectionStateBase& conn_;

    CongestionControlType cur_{CongestionControlType::Cubic};
    CongestionController* impl_{nullptr};
    std::map<CongestionControlType, std::unique_ptr<CongestionController>> pool_;

    folly::AsyncUDPSocket socket_;
    char readBuf_[1024];
};

} // namespace quic