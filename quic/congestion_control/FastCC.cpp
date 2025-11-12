#include <quic/congestion_control/FastCC.h>
#include <quic/congestion_control/CongestionController.h>
#include <quic/state/StateData.h>
#include "CongestionControlFunctions.h"
#include <algorithm>

namespace quic {

uint64_t FastCC::nowUs() {
  return (uint64_t)std::chrono::duration_cast<std::chrono::microseconds>(
             Clock::now().time_since_epoch())
             .count();
}

FastCC::FastCC(QuicConnectionStateBase& conn) : conn_(conn) {
  std::memset(&view_, 0, sizeof(view_));

  captureState();

  if (view_.smoothed_rtt == 0) {
    uint64_t rtt = conn_.lossState.lrtt.count();
    if (!rtt) rtt = 10000; // 10ms
    view_.smoothed_rtt = rtt;
  }

  picoquic_fastcc_init(&view_, nowUs());

  auto mss = std::max<uint64_t>(conn_.udpSendPacketLen, 1200);
  auto initMss = conn_.transportSettings.initCwndInMss > 0
                   ? conn_.transportSettings.initCwndInMss
                   : 32;
  view_.cwin = std::max<uint64_t>(view_.cwin, initMss * mss);

  updatePacing();
}

void FastCC::onPacketSent(const OutstandingPacketWrapper& packet) {
  addAndCheckOverflow(
      conn_.lossState.inflightBytes,
      packet.metadata.encodedSize,
      2 * conn_.transportSettings.maxCwndInMss * conn_.udpSendPacketLen);
}

void FastCC::onRemoveBytesFromInflight(uint64_t bytes) {
  subtractAndCheckUnderflow(conn_.lossState.inflightBytes, bytes);
}

void FastCC::onPacketAckOrLoss(
    const AckEvent* FOLLY_NULLABLE ackEvent,
    const LossEvent* FOLLY_NULLABLE lossEvent) {
  const uint64_t now = nowUs();

  captureState();

  if (ackEvent) {
    subtractAndCheckUnderflow(
        conn_.lossState.inflightBytes, ackEvent->ackedBytes);
  }
  if (lossEvent) {
    subtractAndCheckUnderflow(
        conn_.lossState.inflightBytes, lossEvent->lostBytes);
  }

  if (ackEvent) {
    if (!seededCwnd_) {
      const uint64_t inflight = conn_.lossState.inflightBytes;
      if (inflight > view_.cwin) {
        picoquic_per_ack_state_t tmp = view_.ack_state;
        tmp.nb_bytes_acknowledged = inflight;
        picoquic_fastcc_notify(
            &view_, picoquic_congestion_notification_seed_cwin, &tmp, now);
      }
      seededCwnd_ = true;
    }

    if (appLimited_ && ackEvent->largestNewlyAckedPacketSentTime > appLimitedExitTarget_) {
      appLimited_ = false;
    }

    view_.ack_state.nb_bytes_acknowledged = ackEvent->ackedBytes;
    picoquic_fastcc_notify(
        &view_, picoquic_congestion_notification_acknowledgement, &view_.ack_state, now);

    if (ackEvent->rttSampleNoAckDelay.has_value()) {
      view_.ack_state.rtt_measurement =
          (uint64_t)ackEvent->rttSampleNoAckDelay->count();
      picoquic_fastcc_notify(
          &view_, picoquic_congestion_notification_rtt_measurement, &view_.ack_state, now);
    }

    if (ackEvent->ecnCECount > prevCECount) {
      picoquic_fastcc_notify(
          &view_, picoquic_congestion_notification_ecn_ec, &view_.ack_state, now);
      prevCECount = ackEvent->ecnCECount;
    }

    if (conn_.lossState.totalPacketsSpuriouslyMarkedLost > prevSpuriousCount) {
      picoquic_fastcc_notify(
          &view_, picoquic_congestion_notification_spurious_repeat, &view_.ack_state, now);
      prevSpuriousCount = conn_.lossState.totalPacketsSpuriouslyMarkedLost;
    }

    updatePacing();
  }

  if (lossEvent) {
    if (lossEvent->largestLostPacketNum) {
      view_.ack_state.lost_packet_number = *lossEvent->largestLostPacketNum;
    }

    if (lossEvent->persistentCongestion) {
      picoquic_fastcc_notify(
          &view_, picoquic_congestion_notification_timeout, &view_.ack_state, now);
    } else if (lossEvent->lostPackets > 0 || lossEvent->lostBytes > 0) {
      picoquic_fastcc_notify(
          &view_, picoquic_congestion_notification_repeat, &view_.ack_state, now);
    }

    updatePacing();
  }
}

void FastCC::captureState() {
  view_.smoothed_rtt = (uint64_t)conn_.lossState.srtt.count();

  if (conn_.lossState.lastAckedPacketSentTime) {
    view_.last_time_acked_data_frame_sent =
        (uint64_t)std::chrono::duration_cast<std::chrono::microseconds>(
            conn_.lossState.lastAckedPacketSentTime->time_since_epoch())
            .count();
  }

  if (conn_.lossState.largestSent) {
    view_.recovery_sequence = *conn_.lossState.largestSent;
  }
  if (conn_.ackStates.appDataAckState.largestAckedByPeer) {
    view_.ack_number = *conn_.ackStates.appDataAckState.largestAckedByPeer;
  }
}

void FastCC::setAppLimited() noexcept {
  if (conn_.lossState.inflightBytes > getCongestionWindow()) {
    return;
  }
  if (!appLimited_) {
    appLimited_ = true;
    appLimitedExitTarget_ = Clock::now();
    view_.last_sender_limited_time = nowUs();
  }
}

uint64_t FastCC::getWritableBytes() const noexcept {
  const uint64_t cwnd = getCongestionWindow();
  VLOG(1) << "getWritableBytes!" << (cwnd > conn_.lossState.inflightBytes ? (cwnd - conn_.lossState.inflightBytes) : 0);
  VLOG(1) << "inflights:" << conn_.lossState.inflightBytes << ", cwnd:" << cwnd;
  return cwnd > conn_.lossState.inflightBytes ? (cwnd - conn_.lossState.inflightBytes) : 0;
}

uint64_t FastCC::getCongestionWindow() const noexcept {
  return view_.cwin;
}

void FastCC::updatePacing() {
  if (!conn_.pacer) {
    return;
  }
  uint64_t rtt = view_.smoothed_rtt ? view_.smoothed_rtt
                                    : (uint64_t)conn_.lossState.srtt.count();
  if (!rtt) rtt = 10000;

  conn_.pacer->setRttFactor(
      conn_.transportSettings.defaultRttFactor.first,
      conn_.transportSettings.defaultRttFactor.second);
  conn_.pacer->refreshPacingRate(getCongestionWindow(), std::chrono::microseconds(rtt));
}

} // namespace quic
