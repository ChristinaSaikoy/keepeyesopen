#include <math.h>
#include <stdint.h>
#include <stdio.h>

#include "perclos.h"

static int failures = 0;

#define CHECK(condition)                                                                            \
    do {                                                                                            \
        if (!(condition)) {                                                                         \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition);                \
            ++failures;                                                                             \
            return;                                                                                 \
        }                                                                                           \
    } while (0)

static void closed(perclos_state_t *state, uint32_t now_ms)
{
    perclos_update_classifier(0.90f, 0.90f, 0.10f, now_ms, state);
}

static void open_eyes(perclos_state_t *state, uint32_t now_ms)
{
    perclos_update_classifier(0.90f, 0.10f, 0.10f, now_ms, state);
}

static void test_sleep_recovery_cooldown_and_wrap(void)
{
    perclos_state_t state;
    perclos_reset(&state);
    closed(&state, 0U);
    closed(&state, BLINK_MICRO_SLEEP_MS);
    CHECK(state.level == DMS_LEVEL_2_MICROSLEEP && state.should_send_alert);
    closed(&state, BLINK_MICRO_SLEEP_MS + 1U);
    CHECK(state.level == DMS_LEVEL_2_MICROSLEEP && !state.should_send_alert);
    closed(&state, BLINK_DEEP_SLEEP_MS);
    CHECK(state.level == DMS_LEVEL_3_SLEEP);
    open_eyes(&state, BLINK_DEEP_SLEEP_MS + 1U);
    CHECK(state.level == DMS_LEVEL_NORMAL);

    perclos_reset(&state);
    closed(&state, UINT32_MAX - 500U);
    closed(&state, 100U);
    CHECK(state.level == DMS_LEVEL_2_MICROSLEEP);
}

static void test_yawn_priority_and_recovery(void)
{
    perclos_state_t state;
    perclos_reset(&state);
    perclos_update_classifier(0.90f, 0.10f, 0.90f, 0U, &state);
    perclos_update_classifier(0.90f, 0.10f, 0.90f, YAWN_DURATION_MS, &state);
    CHECK(state.level == DMS_LEVEL_2_YAWN);
    perclos_update_classifier(0.90f, 0.10f, 0.10f, YAWN_DURATION_MS + 1U, &state);
    perclos_update_classifier(0.90f, 0.10f, 0.10f, YAWN_DURATION_MS + 1U + YAWN_RECOVERY_MS, &state);
    CHECK(state.level == DMS_LEVEL_NORMAL);

    perclos_reset(&state);
    perclos_update_classifier(0.90f, 0.90f, 0.90f, 0U, &state);
    perclos_update_classifier(0.90f, 0.90f, 0.90f, BLINK_DEEP_SLEEP_MS, &state);
    CHECK(state.level == DMS_LEVEL_3_SLEEP && state.cause == DMS_CAUSE_DEEP_SLEEP);
}

static void test_hysteresis_invalid_and_perclos(void)
{
    perclos_state_t state;
    perclos_reset(&state);
    perclos_update_classifier(0.90f, 0.69f, 0.10f, 0U, &state);
    CHECK(!state.classifier_eyes_closed);
    perclos_update_classifier(0.90f, 0.71f, 0.10f, 100U, &state);
    CHECK(state.classifier_eyes_closed);
    perclos_update_classifier(0.90f, 0.50f, 0.10f, 200U, &state);
    CHECK(state.classifier_eyes_closed);
    perclos_update_classifier(0.90f, 0.34f, 0.10f, 300U, &state);
    CHECK(!state.classifier_eyes_closed);

    perclos_reset(&state);
    open_eyes(&state, 0U);
    closed(&state, 1000U);
    closed(&state, 2000U);
    CHECK(state.perclos_valid_ms == 2000U && fabsf(state.perclos_ratio - 0.5f) < 0.0001f);
    const uint32_t valid_before = state.perclos_valid_ms;
    perclos_update_classifier(0.20f, 0.90f, 0.10f, 2100U, &state);
    CHECK(state.level == DMS_LEVEL_NORMAL && state.perclos_valid_ms == valid_before);
}

static void test_window_eviction(void)
{
    perclos_state_t state;
    perclos_reset(&state);
    closed(&state, 0U);
    for (uint32_t now_ms = 1000U; now_ms <= 30000U; now_ms += 1000U) {
        closed(&state, now_ms);
    }
    for (uint32_t now_ms = 31000U; now_ms <= 91000U; now_ms += 1000U) {
        open_eyes(&state, now_ms);
    }
    CHECK(state.perclos_valid_ms <= PERCLOS_WINDOW_MS && state.perclos_ratio < 0.001f);
}

int main(void)
{
    test_sleep_recovery_cooldown_and_wrap();
    test_yawn_priority_and_recovery();
    test_hysteresis_invalid_and_perclos();
    test_window_eviction();
    if (failures != 0) {
        fprintf(stderr, "%d algorithm test group(s) failed\n", failures);
        return 1;
    }
    puts("4 algorithm regression test groups passed");
    return 0;
}
