// Created by liyan on 2025-08-31.
#pragma once
#include <stdint.h>

#define FASTCC_MIN_ACK_DELAY_FOR_BANDWIDTH 5000
#define FASTCC_BANDWIDTH_FRACTION 0.5
#define FASTCC_REPEAT_THRESHOLD 4
#define FASTCC_BETA 0.125
#define FASTCC_BETA_HEAVY_LOSS 0.5
#define FASTCC_EVAL_ALPHA 0.25
#define FASTCC_DELAY_THRESHOLD_MAX 25000
#define FASTCC_NB_PERIOD 6
#define FASTCC_PERIOD 1000000

#define PICOQUIC_MIN_MAX_RTT_SCOPE 7
#define PICOQUIC_SMOOTHED_LOSS_SCOPE 32
#define PICOQUIC_SMOOTHED_LOSS_FACTOR (1.0/16.0)
#define PICOQUIC_SMOOTHED_LOSS_THRESHOLD (0.15)

#define PICOQUIC_MAX_PACKET_SIZE 1536
#define PICOQUIC_CWIN_INITIAL (10 * PICOQUIC_MAX_PACKET_SIZE)
#define PICOQUIC_CWIN_MINIMUM (2 * PICOQUIC_MAX_PACKET_SIZE)

typedef enum {
  picoquic_fastcc_initial = 0,
  picoquic_fastcc_eval,
  picoquic_fastcc_freeze
} picoquic_fastcc_alg_state_t;

typedef enum {
  picoquic_congestion_notification_acknowledgement,
  picoquic_congestion_notification_repeat,
  picoquic_congestion_notification_timeout,
  picoquic_congestion_notification_spurious_repeat,
  picoquic_congestion_notification_rtt_measurement,
  picoquic_congestion_notification_ecn_ec,
  picoquic_congestion_notification_cwin_blocked,
  picoquic_congestion_notification_seed_cwin,
  picoquic_congestion_notification_reset,
  picoquic_congestion_notification_lost_feedback
} picoquic_congestion_notification_t;

typedef struct {
  uint64_t rtt_measurement;
  uint64_t nb_bytes_acknowledged;
  uint64_t lost_packet_number;
} picoquic_per_ack_state_t;

typedef struct {
  int sample_current;
  int is_init;
  double smoothed_drop_rate;
  uint64_t last_lost_packet_number;
  uint64_t sample_min;
  uint64_t sample_max;
  uint64_t samples[PICOQUIC_MIN_MAX_RTT_SCOPE];
} picoquic_min_max_rtt_t;

typedef struct {
  picoquic_fastcc_alg_state_t alg_state;
  uint64_t end_of_freeze;
  uint64_t last_ack_time;
  uint64_t ack_interval;
  uint64_t nb_bytes_ack;
  uint64_t nb_bytes_ack_since_rtt;
  uint64_t end_of_epoch;
  uint64_t recovery_sequence;
  uint64_t rtt_min;
  uint64_t delay_threshold;
  uint64_t rolling_rtt_min;
  uint64_t last_rtt_min[FASTCC_NB_PERIOD];
  int nb_cc_events;
  unsigned int last_freeze_was_timeout : 1;
  unsigned int last_freeze_was_not_delay : 1;
  unsigned int rtt_min_is_trusted : 1;
  picoquic_min_max_rtt_t rtt_filter;
} picoquic_fastcc_state_t;

typedef struct {
  uint64_t smoothed_rtt;
  uint64_t last_time_acked_data_frame_sent;
  uint64_t last_sender_limited_time;
  uint64_t recovery_sequence;
  uint64_t ack_number;
  picoquic_fastcc_state_t* congestion_alg_state;
  picoquic_per_ack_state_t ack_state;
  uint64_t cwin;
} picoquic_path_t;

uint64_t picoquic_fastcc_delay_threshold(uint64_t rtt_min);
void picoquic_fastcc_reset(picoquic_fastcc_state_t*, picoquic_path_t*, uint64_t);
void picoquic_fastcc_seed_cwin(picoquic_fastcc_state_t*, picoquic_path_t*, uint64_t);
void picoquic_fastcc_init(picoquic_path_t*, uint64_t);
void picoquic_fastcc_notify(picoquic_path_t*, picoquic_congestion_notification_t,
                            picoquic_per_ack_state_t*, uint64_t);
void picoquic_fastcc_delete(picoquic_path_t*);
void picoquic_fastcc_observe(picoquic_path_t*, uint64_t*, uint64_t*);
