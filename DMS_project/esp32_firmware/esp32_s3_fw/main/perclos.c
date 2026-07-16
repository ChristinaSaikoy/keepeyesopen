#include <string.h>

#include "perclos.h"

static uint32_t elapsed_ms(uint32_t now_ms, uint32_t then_ms)
{
    return now_ms - then_ms;
}

static void set_normal(perclos_state_t *state)
{
    state->level = DMS_LEVEL_NORMAL;
    state->cause = DMS_CAUSE_NONE;
    state->status_desc = "Normal";
}

static void append_interval(perclos_state_t *state, uint32_t start_ms, uint32_t end_ms, bool eyes_closed)
{
    const uint32_t duration_ms = elapsed_ms(end_ms, start_ms);
    if (duration_ms == 0U) {
        return;
    }
    if (state->history_count > 0U) {
        const uint16_t last_index = (uint16_t)((state->history_head + DMS_PERCLOS_MAX_INTERVALS - 1U) %
                                               DMS_PERCLOS_MAX_INTERVALS);
        perclos_interval_t *last = &state->history[last_index];
        if (last->eyes_closed == eyes_closed && last->end_ms == start_ms) {
            last->duration_ms = UINT32_MAX - last->duration_ms < duration_ms ? UINT32_MAX :
                                                                                last->duration_ms + duration_ms;
            last->end_ms = end_ms;
            return;
        }
    }
    state->history[state->history_head] = (perclos_interval_t){.end_ms = end_ms,
                                                                 .duration_ms = duration_ms,
                                                                 .eyes_closed = eyes_closed};
    state->history_head = (uint16_t)((state->history_head + 1U) % DMS_PERCLOS_MAX_INTERVALS);
    if (state->history_count < DMS_PERCLOS_MAX_INTERVALS) {
        ++state->history_count;
    }
}

static void refresh_perclos(uint32_t now_ms, perclos_state_t *state)
{
    uint32_t valid_ms = 0U;
    uint32_t closed_ms = 0U;
    for (uint16_t i = 0U; i < state->history_count; ++i) {
        const perclos_interval_t *interval = &state->history[i];
        const uint32_t age_at_end_ms = elapsed_ms(now_ms, interval->end_ms);
        if (age_at_end_ms >= PERCLOS_WINDOW_MS) {
            continue;
        }
        uint32_t included_ms = interval->duration_ms;
        const uint32_t remaining_window_ms = PERCLOS_WINDOW_MS - age_at_end_ms;
        if (included_ms > remaining_window_ms) {
            included_ms = remaining_window_ms;
        }
        valid_ms = UINT32_MAX - valid_ms < included_ms ? UINT32_MAX : valid_ms + included_ms;
        if (interval->eyes_closed) {
            closed_ms = UINT32_MAX - closed_ms < included_ms ? UINT32_MAX : closed_ms + included_ms;
        }
    }
    state->perclos_valid_ms = valid_ms;
    state->perclos_closed_ms = closed_ms;
    state->perclos_ratio = valid_ms == 0U ? 0.0f : (float)closed_ms / (float)valid_ms;
}

static void update_yawn(bool yawning, uint32_t now_ms, perclos_state_t *state)
{
    if (yawning) {
        if (!state->yawn_active) {
            state->yawn_active = true;
            state->yawn_start_ms = now_ms;
            state->yawn_confirmed = false;
        }
        state->mouth_closed_active = false;
        if (elapsed_ms(now_ms, state->yawn_start_ms) >= YAWN_DURATION_MS) {
            state->yawn_confirmed = true;
        }
    } else if (state->yawn_active) {
        if (!state->mouth_closed_active) {
            state->mouth_closed_active = true;
            state->mouth_closed_start_ms = now_ms;
        } else if (elapsed_ms(now_ms, state->mouth_closed_start_ms) >= YAWN_RECOVERY_MS) {
            state->yawn_active = false;
            state->yawn_confirmed = false;
            state->mouth_closed_active = false;
        }
    }
}

static void update_risk(uint32_t now_ms, perclos_state_t *state)
{
    set_normal(state);
    if (state->eyes_closed_active) {
        const uint32_t closed_ms = elapsed_ms(now_ms, state->eyes_closed_start_ms);
        if (closed_ms >= BLINK_DEEP_SLEEP_MS) {
            state->level = DMS_LEVEL_3_SLEEP;
            state->cause = DMS_CAUSE_DEEP_SLEEP;
            state->status_desc = "Level 3: deep eye closure";
            return;
        }
        if (closed_ms >= BLINK_MICRO_SLEEP_MS) {
            state->level = DMS_LEVEL_2_MICROSLEEP;
            state->cause = DMS_CAUSE_MICROSLEEP;
            state->status_desc = "Level 2: microsleep";
            return;
        }
    }
    if (state->perclos_valid_ms >= PERCLOS_MIN_VALID_OBSERVATION_MS &&
        state->perclos_ratio >= PERCLOS_ALERT_RATIO) {
        state->level = DMS_LEVEL_2_MICROSLEEP;
        state->cause = DMS_CAUSE_PERCLOS;
        state->status_desc = "Level 2: elevated PERCLOS";
        return;
    }
    if (state->yawn_confirmed) {
        state->level = DMS_LEVEL_2_YAWN;
        state->cause = DMS_CAUSE_YAWN;
        state->status_desc = "Level 2: continuous yawn";
    }
}

static void update_alert(uint32_t now_ms, perclos_state_t *state)
{
    state->should_send_alert = false;
    if (state->level < DMS_LEVEL_2_MICROSLEEP) {
        return;
    }
    if (!state->has_last_alert || elapsed_ms(now_ms, state->last_alert_ms) >= ALERT_COOLDOWN_MS) {
        state->should_send_alert = true;
        state->has_last_alert = true;
        state->last_alert_ms = now_ms;
    }
}

void perclos_mark_observation_invalid(uint32_t now_ms, perclos_state_t *state)
{
    (void)now_ms;
    if (state == NULL) {
        return;
    }
    state->has_last_observation = false;
    state->eyes_closed_active = false;
    state->yawn_active = false;
    state->yawn_confirmed = false;
    state->mouth_closed_active = false;
    state->should_send_alert = false;
    set_normal(state);
}

void perclos_update_semantic(const dms_fatigue_observation_t *observation,
                             uint32_t now_ms,
                             perclos_state_t *state)
{
    if (state == NULL) {
        return;
    }
    if (observation == NULL || !observation->valid) {
        perclos_mark_observation_invalid(now_ms, state);
        return;
    }
    if (state->has_last_observation) {
        const uint32_t gap_ms = elapsed_ms(now_ms, state->last_observation_ms);
        if (gap_ms <= PERCLOS_MAX_OBSERVATION_GAP_MS) {
            append_interval(state, state->last_observation_ms, now_ms, state->last_eyes_closed);
        }
    }
    state->has_last_observation = true;
    state->last_observation_ms = now_ms;
    state->last_eyes_closed = observation->eyes_closed;
    refresh_perclos(now_ms, state);

    if (observation->eyes_closed && !state->eyes_closed_active) {
        state->eyes_closed_active = true;
        state->eyes_closed_start_ms = now_ms;
    } else if (!observation->eyes_closed) {
        state->eyes_closed_active = false;
    }
    update_yawn(observation->yawning, now_ms, state);
    update_risk(now_ms, state);
    update_alert(now_ms, state);
}

void perclos_reset(perclos_state_t *state)
{
    if (state != NULL) {
        memset(state, 0, sizeof(*state));
        set_normal(state);
    }
}
