#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "cJSON.h"

#include "config.h"
#include "dms_mqtt_contract.h"

#define DMS_SCHEMA_VERSION 1U

static bool device_id_valid(const char *value)
{
    size_t length = 0U;
    if (value == NULL) {
        return false;
    }
    while (value[length] != '\0') {
        if (length >= 48U || (!isalnum((unsigned char)value[length]) && value[length] != '_' && value[length] != '-')) {
            return false;
        }
        ++length;
    }
    return length > 0U;
}

static bool object_has_exact_fields(const cJSON *object, size_t expected_count)
{
    size_t count = 0U;
    const cJSON *child = NULL;
    cJSON_ArrayForEach(child, object) {
        ++count;
    }
    return count == expected_count;
}

static bool read_uint32(const cJSON *item, uint32_t *value)
{
    if (!cJSON_IsNumber(item) || !isfinite(item->valuedouble) || item->valuedouble < 0.0 ||
        item->valuedouble > 4294967295.0 || floor(item->valuedouble) != item->valuedouble) {
        return false;
    }
    *value = (uint32_t)item->valuedouble;
    return true;
}

static bool read_float_range(const cJSON *item, float minimum, float maximum, float *value)
{
    if (!cJSON_IsNumber(item) || !isfinite(item->valuedouble) || item->valuedouble < minimum ||
        item->valuedouble > maximum) {
        return false;
    }
    *value = (float)item->valuedouble;
    return true;
}

static const cJSON *required(const cJSON *object, const char *name)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, name);
    return item == NULL ? NULL : item;
}

dms_contract_result_t dms_parse_vision_observation(const char *topic,
                                                    const char *payload,
                                                    size_t payload_length,
                                                    dms_vision_observation_t *observation)
{
    if (topic == NULL || payload == NULL || observation == NULL || payload_length == 0U) {
        return DMS_CONTRACT_JSON_ERROR;
    }
    cJSON *root = cJSON_ParseWithLengthOpts(payload, payload_length, NULL, 0);
    if (root == NULL || !cJSON_IsObject(root) || !object_has_exact_fields(root, 9U)) {
        cJSON_Delete(root);
        return DMS_CONTRACT_JSON_ERROR;
    }
    memset(observation, 0, sizeof(*observation));
    const cJSON *schema = required(root, "schema_version");
    const cJSON *device_id = required(root, "device_id");
    const cJSON *sequence = required(root, "sequence");
    const cJSON *timestamp = required(root, "source_timestamp_ms");
    const cJSON *face_valid = required(root, "face_valid");
    const cJSON *ear = required(root, "ear");
    const cJSON *mar = required(root, "mar");
    const cJSON *head_pitch = required(root, "head_pitch");
    const cJSON *processing = required(root, "processing_time_ms");
    if (!read_uint32(schema, &observation->schema_version) || observation->schema_version != DMS_SCHEMA_VERSION ||
        !cJSON_IsString(device_id) || !device_id_valid(device_id->valuestring) ||
        !read_uint32(sequence, &observation->sequence) || !read_uint32(timestamp, &observation->source_timestamp_ms) ||
        !cJSON_IsBool(face_valid) || !read_float_range(ear, 0.0f, 1.5f, &observation->ear) ||
        !read_float_range(mar, 0.0f, 3.0f, &observation->mar) ||
        !read_float_range(head_pitch, -90.0f, 90.0f, &observation->head_pitch) ||
        !read_float_range(processing, 0.0f, DMS_MAX_PROCESSING_TIME_MS, &observation->processing_time_ms)) {
        cJSON_Delete(root);
        return DMS_CONTRACT_RANGE_ERROR;
    }
    observation->face_valid = cJSON_IsTrue(face_valid);
    snprintf(observation->device_id, sizeof(observation->device_id), "%s", device_id->valuestring);
    char expected_topic[112];
    snprintf(expected_topic, sizeof(expected_topic), "dms/%s/vision/observation", observation->device_id);
    cJSON_Delete(root);
    return strcmp(topic, expected_topic) == 0 ? DMS_CONTRACT_OK : DMS_CONTRACT_TOPIC_ERROR;
}

dms_contract_result_t dms_parse_status_led_command(const char *topic,
                                                    const char *payload,
                                                    size_t payload_length,
                                                    const char *s3_device_id,
                                                    dms_status_led_command_t *command)
{
    if (topic == NULL || payload == NULL || s3_device_id == NULL || command == NULL) {
        return DMS_CONTRACT_JSON_ERROR;
    }
    char expected_topic[112];
    snprintf(expected_topic, sizeof(expected_topic), "dms/%s/command", s3_device_id);
    if (strcmp(topic, expected_topic) != 0) {
        return DMS_CONTRACT_TOPIC_ERROR;
    }
    cJSON *root = cJSON_ParseWithLengthOpts(payload, payload_length, NULL, 0);
    if (root == NULL || !cJSON_IsObject(root) || !object_has_exact_fields(root, 5U)) {
        cJSON_Delete(root);
        return DMS_CONTRACT_JSON_ERROR;
    }
    const cJSON *schema = required(root, "schema_version");
    const cJSON *device = required(root, "device_id");
    const cJSON *command_id = required(root, "command_id");
    const cJSON *action = required(root, "action");
    const cJSON *enabled = required(root, "enabled");
    uint32_t schema_value = 0U;
    const bool valid = read_uint32(schema, &schema_value) && schema_value == DMS_SCHEMA_VERSION &&
                       cJSON_IsString(device) && strcmp(device->valuestring, s3_device_id) == 0 &&
                       cJSON_IsString(command_id) && device_id_valid(command_id->valuestring) &&
                       cJSON_IsString(action) && strcmp(action->valuestring, "set_status_led") == 0 &&
                       cJSON_IsBool(enabled);
    if (valid) {
        snprintf(command->command_id, sizeof(command->command_id), "%s", command_id->valuestring);
        command->enabled = cJSON_IsTrue(enabled);
    }
    cJSON_Delete(root);
    return valid ? DMS_CONTRACT_OK : DMS_CONTRACT_COMMAND_REJECTED;
}

const char *dms_contract_result_name(dms_contract_result_t result)
{
    switch (result) {
    case DMS_CONTRACT_OK: return "ok";
    case DMS_CONTRACT_TOPIC_ERROR: return "topic_error";
    case DMS_CONTRACT_JSON_ERROR: return "json_error";
    case DMS_CONTRACT_SCHEMA_ERROR: return "schema_error";
    case DMS_CONTRACT_RANGE_ERROR: return "range_error";
    case DMS_CONTRACT_COMMAND_REJECTED: return "command_rejected";
    default: return "unknown";
    }
}
