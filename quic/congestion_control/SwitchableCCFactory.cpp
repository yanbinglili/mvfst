//
// Created by liyan on 2025-07-28.
//

#include <folly/io/async/EventBase.h>
#include <quic/congestion_control/CongestionControllerFactory.h>

#include "SwitchableCC.h"
#include "quic/congestion_control/SwitchableCCFactory.h"
#include "quic/state/QuicTransportStatsCallback.h"
#include <quic/congestion_control/Bbr.h>
#include <quic/congestion_control/Bbr2.h>
#include <quic/congestion_control/BbrBandwidthSampler.h>
#include <quic/congestion_control/BbrRttSampler.h>
#include <quic/congestion_control/BbrTesting.h>
#include <quic/congestion_control/Copa.h>
#include <quic/congestion_control/Copa2.h>
#include <quic/congestion_control/NewReno.h>
#include <quic/congestion_control/QuicCubic.h>

#include "AsyncLogger.h"


namespace quic {

thread_local folly::EventBase* current_evb_for_cc = nullptr;

std::unique_ptr<CongestionController> SwitchableCCFactory::makeCongestionController(
    QuicConnectionStateBase& conn,
    CongestionControlType type) {
    folly::EventBase* evb = current_evb_for_cc;

    auto setupBBR = [&conn](BbrCongestionController* bbr) {
      bbr->setRttSampler(std::make_unique<BbrRttSampler>(
          std::chrono::seconds(kDefaultRttSamplerExpiration)));
      bbr->setBandwidthSampler(std::make_unique<BbrBandwidthSampler>(conn));
    };

    std::unique_ptr<CongestionController> congestionController;
    switch (type) {
    case CongestionControlType::NewReno:
      congestionController = std::make_unique<NewReno>(conn);
      break;
    case CongestionControlType::Cubic:
      congestionController = std::make_unique<Cubic>(conn);
      break;
    case CongestionControlType::Copa:
      congestionController = std::make_unique<Copa>(conn);
      break;
    case CongestionControlType::Copa2:
      congestionController = std::make_unique<Copa2>(conn);
      break;
    case CongestionControlType::BBR: {
      auto bbr = std::make_unique<BbrCongestionController>(conn);
      setupBBR(bbr.get());
      congestionController = std::move(bbr);
      break;
    }
    case CongestionControlType::BBRTesting: {
      auto bbr = std::make_unique<BbrTestingCongestionController>(conn);
      setupBBR(bbr.get());
      congestionController = std::move(bbr);
      break;
    }
    case CongestionControlType::BBR2: {
      auto bbr2 = std::make_unique<Bbr2CongestionController>(conn);
      congestionController = std::move(bbr2);
      break;
    }
    case CongestionControlType::SwitchableCC: {
      auto swiCC = std::make_unique<SwitchableCC>(conn, evb);
      congestionController = std::move(swiCC);
      break;
    }
    case CongestionControlType::StaticCwnd: {
      throw QuicInternalException(
          "StaticCwnd Congestion Controller cannot be "
          "constructed via CongestionControllerFactory.",
          LocalErrorCode::INTERNAL_ERROR);
    }
    case CongestionControlType::None:
      break;
    case CongestionControlType::MAX:
      throw QuicInternalException(
          "MAX is not a valid cc algorithm.", LocalErrorCode::INTERNAL_ERROR);
  }
  QUIC_STATS(conn.statsCallback, onNewCongestionController, type);
  return congestionController;
 }

};

