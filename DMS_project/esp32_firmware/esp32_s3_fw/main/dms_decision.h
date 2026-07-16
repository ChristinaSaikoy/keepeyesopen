#ifndef DMS_DECISION_H
#define DMS_DECISION_H

#include <stdbool.h>
#include <stdint.h>

#include "dms_mqtt_contract.h"
#include "perclos.h"

typedef enum {
    DMS_LINK_DISCONNECTED = 0,
    DMS_LINK_WAITING,
    DMS_LINK_ACTIVE,
    DMS_LINK_STALE,
    DMS_LINK_INVALID,
} dms_link_status_t;

typedef struct {
    perclos_state_t fatigue;
    bool observation_valid;
    bool has_sequence;
    uint32_t last_sequence;
    uint32_t last_source_timestamp_ms;
    uint32_t last_receive_ms;
    dms_link_status_t link_status;
    bool eyes_closed;
    bool yawning;
    uint32_t invalid_message_count;
    uint32_t duplicate_sequence_count;
    uint32_t out_of_order_count;
    uint32_t stale_timeout_count;
} dms_decision_state_t;

typedef enum {
    DMS_DECISION_ACCEPTED = 0,
    DMS_DECISION_INVALID,
    DMS_DECISION_DUPLICATE,
    DMS_DECISION_OUT_OF_ORDER,
} dms_decision_result_t;

void dms_decision_init(dms_decision_state_t *state);
dms_decision_result_t dms_decision_ingest(dms_decision_state_t *state,
                                          const dms_vision_observation_t *observation,
                                          uint32_t receive_ms);
void dms_decision_mark_invalid(dms_decision_state_t *state, uint32_t now_ms, dms_link_status_t link_status);
void dms_decision_mark_disconnected(dms_decision_state_t *state, uint32_t now_ms);
void dms_decision_tick(dms_decision_state_t *state, uint32_t now_ms);
const char *dms_link_status_name(dms_link_status_t status);

#endif /* DMS_DECISION_H */
