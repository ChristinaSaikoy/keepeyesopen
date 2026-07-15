#include <math.h>
#include <stdbool.h>
#include <stdint.h>

#include "cam_model_adapter.h"
#include "camera_board.h"
#include "config.h"
#include "driver/uart.h"
#include "dms_uart_protocol.h"
#include "esp_camera.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "DMS_CAM";
static uint32_t tx_sequence = 0U;
static uint32_t uart_tx_failures = 0U;
static uint32_t capture_failures = 0U;

static bool uart_send(const uint8_t *data, size_t length)
{
    const int written = uart_write_bytes(CAM_UART_PORT, (const char *)data, length);
    if (written != (int)length || uart_wait_tx_done(CAM_UART_PORT, pdMS_TO_TICKS(100U)) != ESP_OK) {
        ++uart_tx_failures;
        ESP_LOGE(TAG, "UART send failed, count=%lu", (unsigned long)uart_tx_failures);
        return false;
    }
    return true;
}

static void send_heartbeat(void)
{
    uint8_t frame[DMS_UART_MAX_FRAME_SIZE];
    size_t length = 0U;
    const dms_uart_heartbeat_t heartbeat = {.uptime_ms = (uint32_t)(esp_timer_get_time() / 1000LL),
                                             .status_flags = (uint16_t)uart_tx_failures};
    if (dms_uart_encode_heartbeat(tx_sequence++, &heartbeat, frame, sizeof(frame), &length) == DMS_UART_OK) {
        (void)uart_send(frame, length);
    }
}

static void send_camera_status(dms_camera_status_code_t code, const camera_fb_t *frame, uint16_t sensor_pid)
{
    uint8_t encoded[DMS_UART_MAX_FRAME_SIZE];
    size_t length = 0U;
    const dms_uart_camera_status_t status = {.status_code = (uint8_t)code,
                                              .sensor_pid = sensor_pid,
                                              .width = frame == NULL ? 0U : frame->width,
                                              .height = frame == NULL ? 0U : frame->height,
                                              .pixel_format = frame == NULL ? 0U : (uint8_t)frame->format,
                                              .capture_error_count = capture_failures};
    if (dms_uart_encode_camera_status(tx_sequence++, &status, encoded, sizeof(encoded), &length) == DMS_UART_OK) {
        (void)uart_send(encoded, length);
    }
}

static void send_model_unavailable(dms_model_status_t status)
{
    uint8_t frame[DMS_UART_MAX_FRAME_SIZE];
    size_t length = 0U;
    const dms_uart_model_unavailable_t unavailable = {.source_timestamp_ms = (uint32_t)(esp_timer_get_time() / 1000LL),
                                                       .status_code = (uint16_t)status};
    if (dms_uart_encode_model_unavailable(tx_sequence++, &unavailable, frame, sizeof(frame), &length) == DMS_UART_OK) {
        (void)uart_send(frame, length);
    }
}

static void send_model_result(const dms_model_output_t *output)
{
    if (output == NULL || !output->face_valid || !isfinite(output->face_valid_probability) ||
        !isfinite(output->eye_closed_probability) || !isfinite(output->yawn_probability) ||
        output->face_valid_probability < 0.0f || output->face_valid_probability > 1.0f ||
        output->eye_closed_probability < 0.0f || output->eye_closed_probability > 1.0f ||
        output->yawn_probability < 0.0f || output->yawn_probability > 1.0f) {
        return;
    }
    const dms_uart_model_result_t result = {
        .source_timestamp_ms = (uint32_t)(esp_timer_get_time() / 1000LL),
        .face_valid_probability_q = (uint16_t)(output->face_valid_probability * DMS_UART_PROBABILITY_SCALE + 0.5f),
        .eye_closed_probability_q = (uint16_t)(output->eye_closed_probability * DMS_UART_PROBABILITY_SCALE + 0.5f),
        .yawn_probability_q = (uint16_t)(output->yawn_probability * DMS_UART_PROBABILITY_SCALE + 0.5f),
        .inference_time_us = output->inference_time_us,
        .status_flags = output->status_flags,
    };
    uint8_t frame[DMS_UART_MAX_FRAME_SIZE];
    size_t length = 0U;
    if (dms_uart_encode_model_result(tx_sequence++, &result, frame, sizeof(frame), &length) == DMS_UART_OK) {
        (void)uart_send(frame, length);
    }
}

static bool init_uart(void)
{
    const uart_config_t config = {.baud_rate = CAM_UART_BAUD_RATE,
                                  .data_bits = UART_DATA_8_BITS,
                                  .parity = UART_PARITY_DISABLE,
                                  .stop_bits = UART_STOP_BITS_1,
                                  .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
                                  .source_clk = UART_SCLK_DEFAULT};
    return uart_driver_install(CAM_UART_PORT, 1024, 0, 0, NULL, 0) == ESP_OK &&
           uart_param_config(CAM_UART_PORT, &config) == ESP_OK &&
           uart_set_pin(CAM_UART_PORT, CAM_UART_TX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE) == ESP_OK;
}

static bool init_camera(uint16_t *sensor_pid)
{
    const dms_camera_board_status_t board_status = dms_camera_board_validate();
    if (board_status != DMS_CAMERA_BOARD_OK) {
        ESP_LOGE(TAG, "%s", dms_camera_board_status_name(board_status));
        return false;
    }
    camera_config_t config;
    dms_camera_board_fill_config(&config);
    for (uint32_t attempt = 1U; attempt <= CAM_INIT_MAX_ATTEMPTS; ++attempt) {
        const esp_err_t error = esp_camera_init(&config);
        if (error == ESP_OK) {
            sensor_t *sensor = esp_camera_sensor_get();
            *sensor_pid = sensor == NULL ? 0U : sensor->id.PID;
            ESP_LOGI(TAG, "camera initialized on attempt %lu, sensor PID=0x%04X", (unsigned long)attempt, *sensor_pid);
            return true;
        }
        ESP_LOGE(TAG, "camera init attempt %lu/%u failed: %s", (unsigned long)attempt, CAM_INIT_MAX_ATTEMPTS,
                 esp_err_to_name(error));
        (void)esp_camera_deinit();
        vTaskDelay(pdMS_TO_TICKS(CAM_INIT_RETRY_DELAY_MS));
    }
    return false;
}

static void run_capture_loop(bool run_model, uint16_t sensor_pid)
{
    uint32_t consecutive_failures = 0U;
    uint32_t last_heartbeat_ms = 0U;
    uint32_t last_unavailable_ms = 0U;
    while (true) {
        const uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000LL);
        if (now_ms - last_heartbeat_ms >= CAM_HEARTBEAT_INTERVAL_MS) {
            send_heartbeat();
            last_heartbeat_ms = now_ms;
        }
        camera_fb_t *frame = esp_camera_fb_get();
        if (frame == NULL) {
            ++capture_failures;
            ++consecutive_failures;
            send_camera_status(DMS_CAMERA_STATUS_CAPTURE_ERROR, NULL, sensor_pid);
            if (consecutive_failures >= CAM_MAX_CONSECUTIVE_CAPTURE_FAILURES) {
                ESP_LOGE(TAG, "capture failed %lu times; releasing camera and stopping capture loop", (unsigned long)consecutive_failures);
                (void)esp_camera_deinit();
                return;
            }
            vTaskDelay(pdMS_TO_TICKS(CAM_CAPTURE_INTERVAL_MS));
            continue;
        }
        consecutive_failures = 0U;
        send_camera_status(DMS_CAMERA_STATUS_CAPTURE_OK, frame, sensor_pid);
        if (run_model) {
            dms_model_output_t output;
            const dms_model_status_t status = cam_model_run(frame, &output);
            if (status == DMS_MODEL_OK) {
                send_model_result(&output);
            } else if (now_ms - last_unavailable_ms >= CAM_MODEL_UNAVAILABLE_INTERVAL_MS) {
                send_model_unavailable(status);
                last_unavailable_ms = now_ms;
            }
        }
        esp_camera_fb_return(frame);
        vTaskDelay(pdMS_TO_TICKS(CAM_CAPTURE_INTERVAL_MS));
    }
}

void app_main(void)
{
    if (!init_uart()) {
        ESP_LOGE(TAG, "UART initialization failed; cannot emit protocol diagnostics");
        return;
    }
    ESP_LOGI(TAG, "CAM mode=%d, profile=%d", CAM_MODE, CAMERA_BOARD_PROFILE);
    if (CAM_MODE == CAM_MODE_UART_LINK_TEST) {
        while (true) {
            send_heartbeat();
            vTaskDelay(pdMS_TO_TICKS(CAM_HEARTBEAT_INTERVAL_MS));
        }
    }
    uint16_t sensor_pid = 0U;
    if (!init_camera(&sensor_pid)) {
        send_camera_status(DMS_CAMERA_STATUS_INIT, NULL, sensor_pid);
        send_model_unavailable(DMS_MODEL_UNAVAILABLE);
        return;
    }
    send_camera_status(DMS_CAMERA_STATUS_INIT, NULL, sensor_pid);
    run_capture_loop(CAM_MODE == CAM_MODE_MODEL_PIPELINE, sensor_pid);
}
