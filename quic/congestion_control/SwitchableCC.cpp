//
// Created by liyan on 2025-07-31.
//
#include <quic/congestion_control/SwitchableCC.h>
#include <quic/congestion_control/CongestionController.h>
#include <quic/congestion_control/Bbr.h>
#include <quic/congestion_control/Bbr2.h>
#include <quic/congestion_control/BbrRttSampler.h>
#include <quic/congestion_control/BbrBandwidthSampler.h>
#include <quic/congestion_control/Copa.h>
#include <quic/congestion_control/Copa2.h>
#include <quic/congestion_control/NewReno.h>
#include <quic/congestion_control/QuicCubic.h>

namespace quic {

SwitchableCC::SwitchableCC(QuicConnectionStateBase& conn, folly::EventBase* evb)
    : conn_(conn),
      socket_(evb) {
    folly::SocketAddress addr;

    const char* socket_path = "/tmp/cc_switch.sock";
    addr.setFromPath(socket_path);
    unlink(socket_path);
    socket_.bind(addr);

    socket_.resumeRead(this);

    switchTo(cur_);
}

void SwitchableCC::onDataAvailable(
    const folly::SocketAddress&,
    size_t len,
    bool,
    OnDataAvailableParams) noexcept {

    std::string cmd(readBuf_, len);

    if (auto newType = congestionControlStrToType(cmd)) {
        this->switchTo(*newType);
    }
}


void SwitchableCC::getReadBuffer(void** buf, size_t* len) noexcept {
    *buf = readBuf_;
    *len = sizeof(readBuf_);
}

void SwitchableCC::onReadError(const folly::AsyncSocketException& ex) noexcept {
    VLOG(2) << "SwitchableCC: control socket read error: " << ex.what();
    socket_.resumeRead(this);
}

void SwitchableCC::onReadClosed() noexcept {
    VLOG(4) << "SwitchableCC: control socket closed.";
    // 通常在这里可以做一些清理或重连的逻辑，如果需要的话
}

std::unique_ptr<CongestionController> SwitchableCC::makeCCPtr(QuicConnectionStateBase& conn,
                       const CongestionControlType type) const {
    auto setupBBR = [&conn](BbrCongestionController* bbr) {
        bbr->setRttSampler(std::make_unique<BbrRttSampler>(
            std::chrono::seconds(kDefaultRttSamplerExpiration)));
        bbr->setBandwidthSampler(std::make_unique<BbrBandwidthSampler>(conn));
    };

    switch(type) {
        case CongestionControlType::Copa:
            return std::make_unique<Copa>(conn_);
        case CongestionControlType::Copa2:
            return std::make_unique<Copa2>(conn_);
        case CongestionControlType::BBR: {
            auto bbr = std::make_unique<BbrCongestionController>(conn_);
            setupBBR(bbr.get());
            return bbr;
        }
        case CongestionControlType::BBR2:
            return std::make_unique<Bbr2CongestionController>(conn_);
        case CongestionControlType::Cubic:
            return std::make_unique<Cubic>(conn_);
        case CongestionControlType::NewReno:
            return std::make_unique<NewReno>(conn_);
        default:
            throw QuicInternalException(
                "Unsupported congestion control type", LocalErrorCode::CONGESTION_CONTROL_ERROR);
    }
}

void SwitchableCC::switchTo(const CongestionControlType type){
    if (impl_ && type == cur_) {
        return;
    }

    auto it = pool_.find(type);
    if (it == pool_.end()) {
        // create a new instance of the congestion controller and store if it is the first time
        pool_[type] = makeCCPtr(conn_, type);
        it = pool_.find(type);
        assert(it != pool_.end());
    }
    impl_ = it->second.get();
    cur_  = type;
}

} // namespace quic