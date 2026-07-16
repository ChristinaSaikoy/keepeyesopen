#include <assert.h>
#include <math.h>
#include <stdio.h>

#include "dms_decision.h"

static dms_vision_observation_t observation(uint32_t sequence, float ear, float mar)
{
    return (dms_vision_observation_t){.schema_version = 1U,
                                      .sequence = sequence,
                                      .source_timestamp_ms = sequence,
                                      .face_valid = true,
                                      .ear = ear,
                                      .mar = mar,
                                      .head_pitch = 0.0f,
                                      .processing_time_ms = 5.0f};
}

static void test_eye_priority_and_recovery(void)
{
    dms_decision_state_t state;
    dms_decision_init(&state);
    dms_vision_observation_t first = observation(1U, 0.30f, 0.1f);
    dms_vision_observation_t closed = observation(2U, 0.10f, 0.8f);
    dms_vision_observation_t closed_later = observation(3U, 0.10f, 0.8f);
    dms_vision_observation_t deep = observation(4U, 0.10f, 0.8f);
    dms_vision_observation_t recovered = observation(5U, 0.30f, 0.1f);
    assert(dms_decision_ingest(&state, &first, 0U) == DMS_DECISION_ACCEPTED);
    assert(dms_decision_ingest(&state, &closed, 100U) == DMS_DECISION_ACCEPTED);
    assert(dms_decision_ingest(&state, &closed_later, 700U) == DMS_DECISION_ACCEPTED);
    assert(state.fatigue.level == DMS_LEVEL_2_MICROSLEEP);
    assert(state.fatigue.cause == DMS_CAUSE_MICROSLEEP);
    assert(dms_decision_ingest(&state, &deep, 1700U) == DMS_DECISION_ACCEPTED);
    assert(state.fatigue.level == DMS_LEVEL_3_SLEEP);
    assert(!state.fatigue.should_send_alert); /* Cooldown suppresses send, not Level 3 risk. */
    assert(dms_decision_ingest(&state, &recovered, 1800U) == DMS_DECISION_ACCEPTED);
    assert(state.fatigue.level == DMS_LEVEL_NORMAL);
}

static void test_yawn_and_invalid(void)
{
    dms_decision_state_t state;
    dms_decision_init(&state);
    dms_vision_observation_t first = observation(1U, 0.30f, 0.70f);
    dms_vision_observation_t confirmed = observation(2U, 0.30f, 0.70f);
    assert(dms_decision_ingest(&state, &first, 0U) == DMS_DECISION_ACCEPTED);
    assert(dms_decision_ingest(&state, &confirmed, 2600U) == DMS_DECISION_ACCEPTED);
    assert(state.fatigue.level == DMS_LEVEL_2_YAWN);
    dms_vision_observation_t mouth_closed = observation(3U, 0.30f, 0.10f);
    dms_vision_observation_t yawn_recovered = observation(4U, 0.30f, 0.10f);
    assert(dms_decision_ingest(&state, &mouth_closed, 2700U) == DMS_DECISION_ACCEPTED);
    assert(dms_decision_ingest(&state, &yawn_recovered, 3800U) == DMS_DECISION_ACCEPTED);
    assert(state.fatigue.level == DMS_LEVEL_NORMAL);
    dms_vision_observation_t invalid = observation(5U, 0.30f, 0.1f);
    invalid.face_valid = false;
    assert(dms_decision_ingest(&state, &invalid, 2700U) == DMS_DECISION_INVALID);
    assert(state.fatigue.level == DMS_LEVEL_NORMAL);
    assert(!state.fatigue.should_send_alert);
}

static void test_perclos_window_evicts_old_samples(void)
{
    perclos_state_t state;
    const dms_fatigue_observation_t eyes_closed = {.valid = true, .eyes_closed = true, .yawning = false};
    const dms_fatigue_observation_t eyes_open = {.valid = true, .eyes_closed = false, .yawning = false};
    perclos_reset(&state);
    perclos_update_semantic(&eyes_closed, 0U, &state);
    for (uint32_t now_ms = 500U; now_ms <= 10000U; now_ms += 500U) {
        perclos_update_semantic(&eyes_closed, now_ms, &state);
    }
    for (uint32_t now_ms = 10500U; now_ms <= 60000U; now_ms += 500U) {
        perclos_update_semantic(&eyes_open, now_ms, &state);
    }
    assert(state.perclos_valid_ms == PERCLOS_WINDOW_MS);
    assert(fabsf(state.perclos_ratio - (10000.0f / 60000.0f)) < 0.01f);
    for (uint32_t now_ms = 60500U; now_ms <= 71000U; now_ms += 500U) {
        perclos_update_semantic(&eyes_open, now_ms, &state);
    }
    assert(state.perclos_closed_ms == 0U);
    assert(state.perclos_ratio == 0.0f);
}

static void test_sequence_timeout_and_wrap(void)
{
    dms_decision_state_t state;
    dms_decision_init(&state);
    dms_vision_observation_t near_wrap = observation(UINT32_MAX - 1U, 0.30f, 0.1f);
    dms_vision_observation_t last = observation(UINT32_MAX, 0.30f, 0.1f);
    dms_vision_observation_t wrapped = observation(0U, 0.30f, 0.1f);
    dms_vision_observation_t newer = observation(2U, 0.30f, 0.1f);
    dms_vision_observation_t old = observation(1U, 0.30f, 0.1f);
    dms_vision_observation_t final = observation(3U, 0.30f, 0.1f);
    assert(dms_decision_ingest(&state, &near_wrap, UINT32_MAX - 100U) == DMS_DECISION_ACCEPTED);
    assert(dms_decision_ingest(&state, &last, UINT32_MAX - 50U) == DMS_DECISION_ACCEPTED);
    assert(dms_decision_ingest(&state, &wrapped, 20U) == DMS_DECISION_ACCEPTED);
    assert(dms_decision_ingest(&state, &wrapped, 30U) == DMS_DECISION_DUPLICATE);
    assert(!state.observation_valid);
    assert(dms_decision_ingest(&state, &newer, 100U) == DMS_DECISION_ACCEPTED);
    assert(dms_decision_ingest(&state, &old, 110U) == DMS_DECISION_OUT_OF_ORDER);
    assert(!state.fatigue.should_send_alert);
    assert(dms_decision_ingest(&state, &final, 200U) == DMS_DECISION_ACCEPTED);
    dms_decision_tick(&state, 1800U);
    assert(state.link_status == DMS_LINK_STALE);
    assert(state.fatigue.level == DMS_LEVEL_NORMAL);
}

int main(void)
{
    test_eye_priority_and_recovery();
    test_yawn_and_invalid();
    test_sequence_timeout_and_wrap();
    test_perclos_window_evicts_old_samples();
    puts("host_algorithm_tests: passed");
    return 0;
}
