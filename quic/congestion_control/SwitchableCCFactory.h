//
// Created by liyan on 2025-07-28.
//

#pragma once

#include <folly/io/async/EventBase.h>
#include <quic/congestion_control/CongestionControllerFactory.h>

#include "SwitchableCC.h"
#include "AsyncLogger.h"

namespace quic {

class SwitchableCCFactory : public CongestionControllerFactory {
public:
    explicit SwitchableCCFactory() = default;

    std::unique_ptr<CongestionController> makeCongestionController(
        QuicConnectionStateBase& conn,
        CongestionControlType type) override;
};

}
