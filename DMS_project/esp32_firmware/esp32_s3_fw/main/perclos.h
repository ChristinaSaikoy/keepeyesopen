#ifndef DMS_S3_PERCLOS_H
#define DMS_S3_PERCLOS_H

#include <stdbool.h>
#include <stdint.h>

#include "config.h"

typedef enum {
    DMS_LEVEL_NORMAL = 0,
    DMS_LEVEL_2_MICROSLEEP = 2,
    DMS_LEVEL_2_YAWN = 2,
    DMS_LEVEL_3_SLEEP = 3,
} dms_level_t;

typedef enum {
    DMS_CAUSE_NONE = 0,
    DMS_CAUSE_YAWN,
    DMS_CAUSE_PERCLOS,
    DMS_CAUSE_MICROSLEEP,
    DMS_CAUSE_DEEP_SLEEP,
} dms_fatigue_cause_t;

typedef struct {
    uint32_t end_ms;
    uint32_t duration_ms;
    bool eyes_closed;
} perclos_interval_t;

typedef struct {
    bool valid;
    bool eyes_closed;
    bool yawning;
} dms_fatigue_observation_t;

typedef struct {
    bool has_last_observation;
    uint32_t last_observation_ms;
    bool last_eyes_closed;
    bool eyes_closed_active;
    uint32_t eyes_closed_start_ms;
    bool yawn_active;
    uint32_t yawn_start_ms;
    bool yawn_confirmed;
    bool mouth_closed_active;
    uint32_t mouth_closed_start_ms;
    perclos_interval_t history[DMS_PERCLOS_MAX_INTERVALS];
    uint16_t history_head;
    uint16_t history_count;
    uint32_t perclos_valid_ms;
    uint32_t perclos_closed_ms;
    float perclos_ratio;
    dms_level_t level;
    dms_fatigue_cause_t cause;
    const char *status_desc;
    bool should_send_alert;
    bool has_last_alert;
    uint32_t last_alert_ms;
} perclos_state_t;

void perclos_reset(perclos_state_t *state);
void perclos_mark_observation_invalid(uint32_t now_ms, perclos_state_t *state);
void perclos_update_semantic(const dms_fatigue_observation_t *observation,
                             uint32_t now_ms,
                             perclos_state_t *state);

#endif /* DMS_S3_PERCLOS_H */
