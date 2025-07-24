//
// Created by liyan on 2025-07-22.
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

}