#include <string.h>

#include "config.h"
#include "dms_decision.h"

static bool sequence_is_newer(uint32_t candidate, uint32_t previous)
{
    const uint32_t delta = candidate - previous;
    return delta != 0U && delta < 0x80000000U;
}

void dms_decision_init(dms_decision_state_t *state)
{
    if (state != NULL) {
        memset(state, 0, sizeof(*state));
        perclos_reset(&state->fatigue);
        state->link_status = DMS_LINK_WAITING;
    }
}

void dms_decision_mark_invalid(dms_decision_state_t *state, uint32_t now_ms, dms_link_status_t link_status)
{
    if (state == NULL) {
        return;
    }
    state->observation_valid = false;
    state->eyes_closed = false;
    state->yawning = false;
    state->link_status = link_status;
    perclos_mark_observation_invalid(now_ms, &state->fatigue);
}

dms_decision_result_t dms_decision_ingest(dms_decision_state_t *state,
                                          const dms_vision_observation_t *observation,
                                          uint32_t receive_ms)
{
    if (state == NULL || observation == NULL || !observation->face_valid) {
        if (state != NULL) {
            ++state->invalid_message_count;
            dms_decision_mark_invalid(state, receive_ms, DMS_LINK_INVALID);
        }
        return DMS_DECISION_INVALID;
    }
    if (state->has_sequence) {
        if (observation->sequence == state->last_sequence) {
            ++state->duplicate_sequence_count;
            dms_decision_mark_invalid(state, receive_ms, DMS_LINK_INVALID);
            return DMS_DECISION_DUPLICATE;
        }
        if (!sequence_is_newer(observation->sequence, state->last_sequence)) {
            ++state->out_of_order_count;
            dms_decision_mark_invalid(state, receive_ms, DMS_LINK_INVALID);
            return DMS_DECISION_OUT_OF_ORDER;
        }
    }
    state->has_sequence = true;
    state->last_sequence = observation->sequence;
    state->last_source_timestamp_ms = observation->source_timestamp_ms;
    state->last_receive_ms = receive_ms;
    state->link_status = DMS_LINK_ACTIVE;
    state->eyes_closed = state->eyes_closed ? observation->ear < DMS_EAR_CLOSED_RECOVER :
                                               observation->ear <= DMS_EAR_CLOSED_START;
    state->yawning = state->yawning ? observation->mar > DMS_MAR_YAWN_RECOVER :
                                      observation->mar >= DMS_MAR_YAWN_START;
    const dms_fatigue_observation_t semantic = {.valid = true,
                                                .eyes_closed = state->eyes_closed,
                                                .yawning = state->yawning};
    state->observation_valid = true;
    perclos_update_semantic(&semantic, receive_ms, &state->fatigue);
    return DMS_DECISION_ACCEPTED;
}

void dms_decision_mark_disconnected(dms_decision_state_t *state, uint32_t now_ms)
{
    dms_decision_mark_invalid(state, now_ms, DMS_LINK_DISCONNECTED);
}

void dms_decision_tick(dms_decision_state_t *state, uint32_t now_ms)
{
    if (state == NULL || !state->observation_valid) {
        return;
    }
    if ((uint32_t)(now_ms - state->last_receive_ms) >= DMS_OBSERVATION_TIMEOUT_MS) {
        ++state->stale_timeout_count;
        dms_decision_mark_invalid(state, now_ms, DMS_LINK_STALE);
    }
}

const char *dms_link_status_name(dms_link_status_t status)
{
    switch (status) {
    case DMS_LINK_DISCONNECTED: return "disconnected";
    case DMS_LINK_WAITING: return "waiting";
    case DMS_LINK_ACTIVE: return "active";
    case DMS_LINK_STALE: return "stale";
    case DMS_LINK_INVALID: return "invalid";
    default: return "unknown";
    }
}
