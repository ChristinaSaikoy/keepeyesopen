/* ESP32-S3: strict MQTT observation consumer, fatigue decision, and local safety alert. */
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/i2s_std.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "mqtt_client.h"
#include "nvs_flash.h"

#include "alert_rest.h"
#include "alert_safe.h"
#include "alert_sleep.h"
#include "alert_wake.h"
#include "alert_yawn.h"
#include "config.h"
#include "dms_decision.h"
#include "dms_mqtt_contract.h"

static const char *TAG = "DMS_S3";
#define WIFI_CONNECTED_BIT BIT0
#define OLED_ADDR 0x3CU

typedef enum { AUDIO_SLEEP, AUDIO_YAWN, AUDIO_WAKE, AUDIO_REST, AUDIO_SAFE } audio_clip_t;

static EventGroupHandle_t network_events;
static QueueHandle_t audio_queue;
static i2s_chan_handle_t i2s_channel;
static i2c_master_dev_handle_t oled;
static esp_mqtt_client_handle_t mqtt_client;
static dms_decision_state_t decision;
static uint8_t framebuffer[1024];
static uint32_t state_sequence;

static bool is_placeholder(const char *value)
{
    return value == NULL || strstr(value, "YOUR_") != NULL;
}

static uint32_t now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000LL);
}

static void oled_command(uint8_t command)
{
    uint8_t payload[] = {0x00U, command};
    (void)i2c_master_transmit(oled, payload, sizeof(payload), -1);
}

static void oled_data(const uint8_t *data, size_t length)
{
    uint8_t payload[129];
    payload[0] = 0x40U;
    while (length > 0U) {
        const size_t chunk = length > 128U ? 128U : length;
        memcpy(&payload[1], data, chunk);
        (void)i2c_master_transmit(oled, payload, chunk + 1U, -1);
        data += chunk;
        length -= chunk;
    }
}

static void framebuffer_set(int x, int y, bool on)
{
    if (x < 0 || x >= 128 || y < 0 || y >= 64) {
        return;
    }
    const size_t index = (size_t)x + (size_t)(y >> 3) * 128U;
    const uint8_t bit = (uint8_t)(1U << (y & 7));
    if (on) {
        framebuffer[index] |= bit;
    } else {
        framebuffer[index] &= (uint8_t)~bit;
    }
}

static void framebuffer_circle(int center_x, int center_y, int radius, bool on)
{
    for (int y = -radius; y <= radius; ++y) {
        for (int x = -radius; x <= radius; ++x) {
            if (x * x + y * y <= radius * radius) {
                framebuffer_set(center_x + x, center_y + y, on);
            }
        }
    }
}

static void draw_eyes(bool closed)
{
    memset(framebuffer, 0, sizeof(framebuffer));
    if (closed) {
        for (int x = 15; x <= 51; ++x) {
            framebuffer_set(x, 28, true);
            framebuffer_set(x + 61, 28, true);
        }
    } else {
        framebuffer_circle(33, 28, 18, true);
        framebuffer_circle(94, 28, 18, true);
        framebuffer_circle(33, 28, 5, false);
        framebuffer_circle(94, 28, 5, false);
    }
    for (uint8_t page = 0U; page < 8U; ++page) {
        oled_command((uint8_t)(0xB0U + page));
        oled_command(0x00U);
        oled_command(0x10U);
        oled_data(&framebuffer[(size_t)page * 128U], 128U);
    }
}

static void oled_init(void)
{
    const i2c_master_bus_config_t bus_config = {.clk_source = I2C_CLK_SRC_DEFAULT,
                                                .i2c_port = I2C_NUM_0,
                                                .scl_io_num = 9,
                                                .sda_io_num = 8,
                                                .glitch_ignore_cnt = 7,
                                                .flags = {.enable_internal_pullup = true}};
    i2c_master_bus_handle_t bus = NULL;
    const i2c_device_config_t device_config = {.dev_addr_length = I2C_ADDR_BIT_LEN_7,
                                                .device_address = OLED_ADDR,
                                                .scl_speed_hz = 400000};
    if (i2c_new_master_bus(&bus_config, &bus) != ESP_OK || i2c_master_bus_add_device(bus, &device_config, &oled) != ESP_OK) {
        ESP_LOGW(TAG, "OLED initialization failed; decision path remains active");
        return;
    }
    const uint8_t init[] = {0xAE, 0xD5, 0x80, 0xA8, 0x3F, 0xD3, 0x00, 0x40, 0x8D, 0x14, 0x20, 0x00,
                            0xA1, 0xC8, 0xDA, 0x12, 0x81, 0xCF, 0xD9, 0xF1, 0xDB, 0x40, 0xA4, 0xA6, 0xAF};
    for (size_t index = 0U; index < sizeof(init); ++index) {
        oled_command(init[index]);
    }
    draw_eyes(false);
}

static bool audio_init(void)
{
    const i2s_chan_config_t channel_config = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    const i2s_std_config_t standard_config = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(16000),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {.mclk = I2S_GPIO_UNUSED,
                     .bclk = I2S_BCLK_PIN,
                     .ws = I2S_LRC_PIN,
                     .dout = I2S_DIN_PIN,
                     .din = I2S_GPIO_UNUSED},
    };
    if (i2s_new_channel(&channel_config, &i2s_channel, NULL) != ESP_OK ||
        i2s_channel_init_std_mode(i2s_channel, &standard_config) != ESP_OK ||
        i2s_channel_enable(i2s_channel) != ESP_OK) {
        ESP_LOGE(TAG, "I2S initialization failed");
        return false;
    }
    gpio_set_direction(I2S_SD_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(I2S_SD_PIN, 1);
    return true;
}

static void play_clip(audio_clip_t clip)
{
    const uint8_t *data = alert_safe;
    size_t length = ALERT_SAFE_LEN;
    switch (clip) {
    case AUDIO_SLEEP: data = alert_sleep; length = ALERT_SLEEP_LEN; break;
    case AUDIO_YAWN: data = alert_yawn; length = ALERT_YAWN_LEN; break;
    case AUDIO_WAKE: data = alert_wake; length = ALERT_WAKE_LEN; break;
    case AUDIO_REST: data = alert_rest; length = ALERT_REST_LEN; break;
    case AUDIO_SAFE: break;
    }
    size_t written = 0U;
    (void)i2s_channel_write(i2s_channel, data, length, &written, portMAX_DELAY);
}

static void audio_task(void *)
{
    audio_clip_t clip;
    while (true) {
        if (xQueueReceive(audio_queue, &clip, portMAX_DELAY) == pdTRUE) {
            play_clip(clip);
        }
    }
}

static void queue_local_alert(dms_fatigue_cause_t cause)
{
    audio_clip_t clip = AUDIO_SAFE;
    if (cause == DMS_CAUSE_DEEP_SLEEP || cause == DMS_CAUSE_MICROSLEEP) {
        clip = AUDIO_SLEEP;
    } else if (cause == DMS_CAUSE_YAWN) {
        clip = AUDIO_YAWN;
    } else if (cause == DMS_CAUSE_PERCLOS) {
        clip = AUDIO_REST;
    }
    if (xQueueSend(audio_queue, &clip, 0U) != pdTRUE) {
        ESP_LOGW(TAG, "audio queue full; MQTT and decision tasks remain non-blocking");
    }
}

static void publish_json(const char *topic, const char *payload)
{
    if (mqtt_client != NULL) {
        (void)esp_mqtt_client_publish(mqtt_client, topic, payload, 0, 1, 0);
    }
}

static void publish_state(bool include_alert)
{
    char topic[112];
    char payload[512];
    snprintf(topic, sizeof(topic), "dms/%s/fatigue/%s", DMS_S3_DEVICE_ID, include_alert ? "alert" : "state");
    snprintf(payload, sizeof(payload),
             "{\"schema_version\":1,\"device_id\":\"%s\",\"sequence\":%lu,\"source_timestamp_ms\":%lu,"
             "\"current_fatigue_level\":%d,\"alert_due\":%s,\"perclos\":%.5f,"
             "\"observation_valid\":%s,\"link_status\":\"%s\",\"last_sequence\":%lu,\"cause\":%d}",
             DMS_S3_DEVICE_ID, (unsigned long)state_sequence++, (unsigned long)now_ms(), decision.fatigue.level,
             decision.fatigue.should_send_alert ? "true" : "false", (double)decision.fatigue.perclos_ratio,
             decision.observation_valid ? "true" : "false", dms_link_status_name(decision.link_status),
             (unsigned long)decision.last_sequence, decision.fatigue.cause);
    publish_json(topic, payload);
}

static void publish_command_ack(const char *command_id, bool accepted, const char *reason)
{
    char topic[112];
    char payload[256];
    snprintf(topic, sizeof(topic), "dms/%s/command/ack", DMS_S3_DEVICE_ID);
    snprintf(payload, sizeof(payload),
             "{\"schema_version\":1,\"device_id\":\"%s\",\"command_id\":\"%s\","
             "\"action\":\"set_status_led\",\"accepted\":%s,\"reason\":\"%s\"}",
             DMS_S3_DEVICE_ID, command_id == NULL ? "unknown" : command_id, accepted ? "true" : "false", reason);
    publish_json(topic, payload);
}

static void handle_mqtt_data(const esp_mqtt_event_handle_t event)
{
    if (event->topic_len <= 0 || event->topic_len >= 112 || event->data_len <= 0 || event->data_len >= 512) {
        dms_decision_mark_invalid(&decision, now_ms(), DMS_LINK_INVALID);
        publish_state(false);
        return;
    }
    char topic[112];
    char payload[512];
    memcpy(topic, event->topic, (size_t)event->topic_len);
    topic[event->topic_len] = '\0';
    memcpy(payload, event->data, (size_t)event->data_len);
    payload[event->data_len] = '\0';

    dms_vision_observation_t observation;
    const uint32_t received_ms = now_ms();
    const dms_contract_result_t observation_result =
        dms_parse_vision_observation(topic, payload, (size_t)event->data_len, &observation);
    if (observation_result == DMS_CONTRACT_OK) {
        const dms_decision_result_t result = dms_decision_ingest(&decision, &observation, received_ms);
        if (result != DMS_DECISION_ACCEPTED) {
            ESP_LOGW(TAG, "observation rejected: %d", result);
        }
        publish_state(false);
        if (decision.fatigue.should_send_alert) {
            queue_local_alert(decision.fatigue.cause);
            publish_state(true);
        }
        return;
    }

    dms_status_led_command_t command;
    const dms_contract_result_t command_result =
        dms_parse_status_led_command(topic, payload, (size_t)event->data_len, DMS_S3_DEVICE_ID, &command);
    if (command_result == DMS_CONTRACT_OK) {
        gpio_set_level(STATUS_LED_PIN, command.enabled ? 1 : 0);
        publish_command_ack(command.command_id, true, "applied");
        return;
    }
    if (command_result != DMS_CONTRACT_TOPIC_ERROR) {
        publish_command_ack("unknown", false, dms_contract_result_name(command_result));
        return;
    }
    ESP_LOGW(TAG, "discarded MQTT payload: %s", dms_contract_result_name(observation_result));
    dms_decision_mark_invalid(&decision, received_ms, DMS_LINK_INVALID);
    publish_state(false);
}

static void mqtt_event_handler(void *, esp_event_base_t, int32_t event_id, void *event_data)
{
    const esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
    if (event_id == MQTT_EVENT_CONNECTED) {
        char observation_topic[112];
        char command_topic[112];
        snprintf(observation_topic, sizeof(observation_topic), "dms/%s/vision/observation", DMS_PC_DEVICE_ID);
        snprintf(command_topic, sizeof(command_topic), "dms/%s/command", DMS_S3_DEVICE_ID);
        (void)esp_mqtt_client_subscribe(event->client, observation_topic, 1);
        (void)esp_mqtt_client_subscribe(event->client, command_topic, 1);
        decision.link_status = DMS_LINK_WAITING;
        ESP_LOGI(TAG, "MQTT connected; awaiting PC observations");
    } else if (event_id == MQTT_EVENT_DATA) {
        handle_mqtt_data(event);
    } else if (event_id == MQTT_EVENT_DISCONNECTED) {
        dms_decision_mark_disconnected(&decision, now_ms());
        ESP_LOGW(TAG, "MQTT disconnected; fatigue decision reset without alert");
    }
}

static void wifi_event_handler(void *, esp_event_base_t event_base, int32_t event_id, void *)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        (void)esp_wifi_connect();
    }
}

static void ip_event_handler(void *, esp_event_base_t, int32_t, void *event_data)
{
    const ip_event_got_ip_t *event = (const ip_event_got_ip_t *)event_data;
    ESP_LOGI(TAG, "Wi-Fi connected: " IPSTR, IP2STR(&event->ip_info.ip));
    xEventGroupSetBits(network_events, WIFI_CONNECTED_BIT);
}

static bool start_network(void)
{
    if (is_placeholder(DMS_WIFI_SSID) || is_placeholder(DMS_WIFI_PASSWORD) || is_placeholder(DMS_MQTT_BROKER_URL)) {
        ESP_LOGE(TAG, "network credentials are placeholders; create ignored dms_secrets.h from the example");
        return false;
    }
    network_events = xEventGroupCreate();
    if (network_events == NULL || nvs_flash_init() != ESP_OK || esp_netif_init() != ESP_OK ||
        esp_event_loop_create_default() != ESP_OK || esp_netif_create_default_wifi_sta() == NULL) {
        return false;
    }
    const wifi_init_config_t wifi_init = WIFI_INIT_CONFIG_DEFAULT();
    wifi_config_t wifi_config = {.sta = {.ssid = DMS_WIFI_SSID, .password = DMS_WIFI_PASSWORD}};
    if (esp_wifi_init(&wifi_init) != ESP_OK ||
        esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL) != ESP_OK ||
        esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, ip_event_handler, NULL) != ESP_OK ||
        esp_wifi_set_mode(WIFI_MODE_STA) != ESP_OK || esp_wifi_set_config(WIFI_IF_STA, &wifi_config) != ESP_OK ||
        esp_wifi_start() != ESP_OK) {
        return false;
    }
    if ((xEventGroupWaitBits(network_events, WIFI_CONNECTED_BIT, pdFALSE, pdTRUE, pdMS_TO_TICKS(20000U)) &
         WIFI_CONNECTED_BIT) == 0U) {
        return false;
    }
    const esp_mqtt_client_config_t mqtt_config = {.broker.address.uri = DMS_MQTT_BROKER_URL,
                                                   .session.keepalive = DMS_MQTT_KEEPALIVE_SECONDS};
    mqtt_client = esp_mqtt_client_init(&mqtt_config);
    return mqtt_client != NULL &&
           esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL) == ESP_OK &&
           esp_mqtt_client_start(mqtt_client) == ESP_OK;
}

void app_main(void)
{
    dms_decision_init(&decision);
    gpio_set_direction(STATUS_LED_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(STATUS_LED_PIN, 0);
    oled_init();
    if (!audio_init()) {
        return;
    }
    audio_queue = xQueueCreate(DMS_AUDIO_QUEUE_LENGTH, sizeof(audio_clip_t));
    if (audio_queue == NULL || xTaskCreate(audio_task, "dms_audio", 4096U, NULL, 5U, NULL) != pdPASS) {
        ESP_LOGE(TAG, "audio task creation failed");
        return;
    }
    if (!start_network()) {
        ESP_LOGE(TAG, "network unavailable; no MQTT data can trigger an alert");
        return;
    }
    uint32_t last_state_ms = 0U;
    while (true) {
        const uint32_t current_ms = now_ms();
        dms_decision_tick(&decision, current_ms);
        draw_eyes(decision.observation_valid && decision.eyes_closed);
        gpio_set_level(STATUS_LED_PIN, decision.fatigue.level >= DMS_LEVEL_2_MICROSLEEP ? 1 : 0);
        if ((uint32_t)(current_ms - last_state_ms) >= 1000U) {
            publish_state(false);
            last_state_ms = current_ms;
        }
        vTaskDelay(pdMS_TO_TICKS(50U));
    }
}
