#ifndef DMS_MQTT_CONTRACT_H
#define DMS_MQTT_CONTRACT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint32_t schema_version;
    char device_id[49];
    uint32_t sequence;
    uint32_t source_timestamp_ms;
    bool face_valid;
    float ear;
    float mar;
    float head_pitch;
    float processing_time_ms;
} dms_vision_observation_t;

typedef struct {
    char command_id[65];
    bool enabled;
} dms_status_led_command_t;

typedef enum {
    DMS_CONTRACT_OK = 0,
    DMS_CONTRACT_TOPIC_ERROR,
    DMS_CONTRACT_JSON_ERROR,
    DMS_CONTRACT_SCHEMA_ERROR,
    DMS_CONTRACT_RANGE_ERROR,
    DMS_CONTRACT_COMMAND_REJECTED,
} dms_contract_result_t;

dms_contract_result_t dms_parse_vision_observation(const char *topic,
                                                    const char *payload,
                                                    size_t payload_length,
                                                    dms_vision_observation_t *observation);
dms_contract_result_t dms_parse_status_led_command(const char *topic,
                                                    const char *payload,
                                                    size_t payload_length,
                                                    const char *s3_device_id,
                                                    dms_status_led_command_t *command);
const char *dms_contract_result_name(dms_contract_result_t result);

#endif /* DMS_MQTT_CONTRACT_H */
