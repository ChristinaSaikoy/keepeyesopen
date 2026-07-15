#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "config.h"
#include "driver/gpio.h"
#include "driver/i2s_std.h"
#include "driver/ledc.h"
#include "driver/uart.h"
#include "dms_s3_pipeline.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "mqtt_client.h"
#include "nvs_flash.h"

static const char *TAG = "DMS_S3";
static i2s_chan_handle_t audio_channel;
static EventGroupHandle_t network_events;
static esp_mqtt_client_handle_t mqtt_client;

#define WIFI_CONNECTED_BIT BIT0
#define MQTT_CONNECTED_BIT BIT1

static bool string_is_placeholder(const char *value)
{
    return value == NULL || strstr(value, "YOUR_") != NULL || strstr(value, "replace_me") != NULL;
}

static bool init_uart(void)
{
    const uart_config_t config = {.baud_rate = DMS_UART_BAUD_RATE,
                                  .data_bits = UART_DATA_8_BITS,
                                  .parity = UART_PARITY_DISABLE,
                                  .stop_bits = UART_STOP_BITS_1,
                                  .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
                                  .source_clk = UART_SCLK_DEFAULT};
    return uart_driver_install(DMS_UART_PORT, DMS_UART_RX_BUFFER_SIZE, 0, 0, NULL, 0) == ESP_OK &&
           uart_param_config(DMS_UART_PORT, &config) == ESP_OK &&
           uart_set_pin(DMS_UART_PORT, DMS_UART_TX_PIN, DMS_UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE) == ESP_OK;
}

static void run_uart_raw_link_test(void)
{
    uint8_t buffer[128];
    ESP_LOGI(TAG, "raw UART test: RX GPIO%d, TX GPIO%d", DMS_UART_RX_PIN, DMS_UART_TX_PIN);
    while (true) {
        const int length = uart_read_bytes(DMS_UART_PORT, buffer, sizeof(buffer), pdMS_TO_TICKS(500U));
        if (length > 0) {
            ESP_LOGI(TAG, "raw UART received %d byte(s)", length);
        }
    }
}

static void log_protocol_diagnostics(const dms_uart_parser_t *parser, const dms_s3_pipeline_t *pipeline)
{
    ESP_LOGI(TAG,
             "frames=%lu crc=%lu len=%lu ver=%lu gaps=%lu timeout=%lu model=%lu unavailable=%lu model_timeout=%lu level=%d alert=%d",
             (unsigned long)parser->stats.frames_ok, (unsigned long)parser->stats.crc_errors,
             (unsigned long)parser->stats.bad_length_errors, (unsigned long)parser->stats.bad_version_errors,
             (unsigned long)parser->stats.sequence_gap_count, (unsigned long)parser->stats.timeout_count,
             (unsigned long)pipeline->model_result_count, (unsigned long)pipeline->model_unavailable_count,
             (unsigned long)pipeline->model_result_timeout_count, pipeline->fatigue.level,
             pipeline->fatigue.should_send_alert);
}

static void run_protocol_state_machine(void)
{
    uint8_t buffer[256];
    dms_uart_parser_t parser;
    dms_s3_pipeline_t pipeline;
    uint32_t last_log_ms = 0U;
    dms_uart_parser_init(&parser);
    dms_s3_pipeline_init(&pipeline);
    while (true) {
        const uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000LL);
        const int length = uart_read_bytes(DMS_UART_PORT, buffer, sizeof(buffer), pdMS_TO_TICKS(100U));
        if (length > 0) {
            dms_uart_parser_feed(&parser, buffer, (size_t)length, now_ms, dms_s3_pipeline_handle_frame, &pipeline);
        }
        dms_uart_parser_advance_time(&parser, now_ms, DMS_UART_FRAME_TIMEOUT_MS);
        dms_s3_pipeline_advance_time(&pipeline, now_ms);
        if (pipeline.fatigue.should_send_alert) {
            ESP_LOGW(TAG, "fatigue alert decision: level=%d cause=%d", pipeline.fatigue.level, pipeline.fatigue.cause);
        }
        if (now_ms - last_log_ms >= 1000U) {
            log_protocol_diagnostics(&parser, &pipeline);
            last_log_ms = now_ms;
        }
    }
}

static void test_i2s(void)
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
    if (i2s_new_channel(&channel_config, &audio_channel, NULL) != ESP_OK ||
        i2s_channel_init_std_mode(audio_channel, &standard_config) != ESP_OK || i2s_channel_enable(audio_channel) != ESP_OK) {
        ESP_LOGE(TAG, "I2S test initialization failed");
        return;
    }
    gpio_set_direction(I2S_SD_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(I2S_SD_PIN, 1);
    ESP_LOGI(TAG, "I2S interface initialized; audio output requires hardware confirmation");
}

static void test_ir_led(void)
{
    const ledc_timer_config_t timer = {.speed_mode = LEDC_LOW_SPEED_MODE,
                                       .duty_resolution = LEDC_TIMER_10_BIT,
                                       .timer_num = LEDC_TIMER_0,
                                       .freq_hz = 5000,
                                       .clk_cfg = LEDC_AUTO_CLK};
    const ledc_channel_config_t channel = {.gpio_num = IR_LED_PIN,
                                           .speed_mode = LEDC_LOW_SPEED_MODE,
                                           .channel = LEDC_CHANNEL_0,
                                           .timer_sel = LEDC_TIMER_0,
                                           .duty = 0};
    if (ledc_timer_config(&timer) != ESP_OK || ledc_channel_config(&channel) != ESP_OK) {
        ESP_LOGE(TAG, "IR LED test initialization failed");
        return;
    }
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 512U);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    ESP_LOGI(TAG, "IR LED PWM configured; optical output is unverified");
}

static void network_event_handler(void *argument, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    (void)argument;
    (void)event_data;
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        (void)esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(network_events, WIFI_CONNECTED_BIT);
    } else if (event_base == MQTT_EVENTS && event_id == MQTT_EVENT_CONNECTED) {
        xEventGroupSetBits(network_events, MQTT_CONNECTED_BIT);
    }
}

static void test_wifi_mqtt(void)
{
    if (string_is_placeholder(DMS_WIFI_SSID) || string_is_placeholder(DMS_WIFI_PASSWORD) ||
        string_is_placeholder(DMS_MQTT_BROKER_URL)) {
        ESP_LOGE(TAG, "Wi-Fi/MQTT test blocked: configure ignored dms_secrets.h or NVS credentials first");
        return;
    }
    network_events = xEventGroupCreate();
    if (network_events == NULL || nvs_flash_init() != ESP_OK || esp_netif_init() != ESP_OK ||
        esp_event_loop_create_default() != ESP_OK || esp_netif_create_default_wifi_sta() == NULL) {
        ESP_LOGE(TAG, "Wi-Fi/MQTT test initialization failed");
        return;
    }
    const wifi_init_config_t wifi_init = WIFI_INIT_CONFIG_DEFAULT();
    const wifi_config_t wifi_config = {.sta = {.ssid = DMS_WIFI_SSID, .password = DMS_WIFI_PASSWORD}};
    if (esp_wifi_init(&wifi_init) != ESP_OK ||
        esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, network_event_handler, NULL) != ESP_OK ||
        esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, network_event_handler, NULL) != ESP_OK ||
        esp_wifi_set_mode(WIFI_MODE_STA) != ESP_OK || esp_wifi_set_config(WIFI_IF_STA, &wifi_config) != ESP_OK ||
        esp_wifi_start() != ESP_OK) {
        ESP_LOGE(TAG, "Wi-Fi start failed");
        return;
    }
    if ((xEventGroupWaitBits(network_events, WIFI_CONNECTED_BIT, pdFALSE, pdTRUE, pdMS_TO_TICKS(20000U)) &
         WIFI_CONNECTED_BIT) == 0U) {
        ESP_LOGE(TAG, "Wi-Fi did not connect within timeout");
        return;
    }
    const esp_mqtt_client_config_t mqtt_config = {.broker.address.uri = DMS_MQTT_BROKER_URL};
    mqtt_client = esp_mqtt_client_init(&mqtt_config);
    if (mqtt_client == NULL || esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, network_event_handler, NULL) != ESP_OK ||
        esp_mqtt_client_start(mqtt_client) != ESP_OK) {
        ESP_LOGE(TAG, "MQTT start failed");
        return;
    }
    if ((xEventGroupWaitBits(network_events, MQTT_CONNECTED_BIT, pdFALSE, pdTRUE, pdMS_TO_TICKS(10000U)) &
         MQTT_CONNECTED_BIT) == 0U) {
        ESP_LOGE(TAG, "MQTT did not connect within timeout");
        return;
    }
    (void)esp_mqtt_client_publish(mqtt_client, DMS_MQTT_TOPIC, "{\"test\":\"dms_s3_mqtt\"}", 0, 1, 0);
    ESP_LOGI(TAG, "Wi-Fi/MQTT test publish requested; hardware/network outcome still requires board logs");
}

void app_main(void)
{
    gpio_set_direction(STATUS_LED_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(STATUS_LED_PIN, 1);
    ESP_LOGI(TAG, "ESP32-S3 mode=%d", TEST_MODE);
    if (!init_uart()) {
        ESP_LOGE(TAG, "UART initialization failed");
        return;
    }
    switch (TEST_MODE) {
    case TEST_MODE_UART_RAW_LINK:
        run_uart_raw_link_test();
        break;
    case TEST_MODE_UART_PROTOCOL:
    case TEST_MODE_FULL_PROTOCOL_STATE_MACHINE:
        run_protocol_state_machine();
        break;
    case TEST_MODE_I2S:
        test_i2s();
        break;
    case TEST_MODE_IR_LED:
        test_ir_led();
        break;
    case TEST_MODE_WIFI_MQTT:
        test_wifi_mqtt();
        break;
    default:
        ESP_LOGE(TAG, "unsupported TEST_MODE=%d", TEST_MODE);
        break;
    }
}
