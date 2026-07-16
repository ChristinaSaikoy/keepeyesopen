/* ESP32-CAM: low-latency LAN MJPEG producer. MQTT is not used for JPEG data. */
#include <stdio.h>
#include <string.h>

#include "esp_camera.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#include "config.h"

static const char *TAG = "DMS_CAM";
static EventGroupHandle_t wifi_events;
static constexpr EventBits_t WIFI_CONNECTED_BIT = BIT0;

static bool is_placeholder(const char *value)
{
    return value == NULL || strstr(value, "YOUR_") != NULL;
}

static void wifi_event_handler(void *, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        (void)esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        const ip_event_got_ip_t *event = static_cast<const ip_event_got_ip_t *>(event_data);
        ESP_LOGI(TAG, "Wi-Fi connected: " IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(wifi_events, WIFI_CONNECTED_BIT);
    }
}

static esp_err_t health_handler(httpd_req_t *request)
{
    httpd_resp_set_type(request, "application/json");
    return httpd_resp_sendstr(request, "{\"status\":\"ok\",\"stream\":\"/stream\"}");
}

static esp_err_t stream_handler(httpd_req_t *request)
{
    static const char *CONTENT_TYPE = "multipart/x-mixed-replace;boundary=dmsframe";
    httpd_resp_set_type(request, CONTENT_TYPE);
    httpd_resp_set_hdr(request, "Access-Control-Allow-Origin", "*");
    while (true) {
        camera_fb_t *frame = esp_camera_fb_get();
        if (frame == NULL) {
            ESP_LOGW(TAG, "camera frame unavailable");
            vTaskDelay(pdMS_TO_TICKS(10U));
            continue;
        }
        const uint32_t capture_ms = (uint32_t)(esp_timer_get_time() / 1000LL);
        char header[160];
        const int header_length = snprintf(header, sizeof(header),
                                           "--dmsframe\r\nContent-Type: image/jpeg\r\n"
                                           "Content-Length: %u\r\nX-Capture-Timestamp-Ms: %lu\r\n\r\n",
                                           (unsigned)frame->len, (unsigned long)capture_ms);
        esp_err_t result = httpd_resp_send_chunk(request, header, header_length);
        if (result == ESP_OK) {
            result = httpd_resp_send_chunk(request, reinterpret_cast<const char *>(frame->buf), frame->len);
        }
        if (result == ESP_OK) {
            result = httpd_resp_send_chunk(request, "\r\n", 2);
        }
        esp_camera_fb_return(frame);
        if (result != ESP_OK) {
            ESP_LOGI(TAG, "MJPEG client disconnected: %s", esp_err_to_name(result));
            return result;
        }
        if (DMS_CAM_FRAME_INTERVAL_MS > 0U) {
            vTaskDelay(pdMS_TO_TICKS(DMS_CAM_FRAME_INTERVAL_MS));
        }
    }
}

static void start_http_server(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = DMS_CAM_STREAM_PORT;
    config.ctrl_port = (uint16_t)(DMS_CAM_STREAM_PORT + 1U);
    config.max_uri_handlers = 2U;
    httpd_handle_t server = NULL;
    if (httpd_start(&server, &config) != ESP_OK) {
        ESP_LOGE(TAG, "HTTP server start failed");
        return;
    }
    const httpd_uri_t health = {.uri = "/health", .method = HTTP_GET, .handler = health_handler, .user_ctx = NULL};
    const httpd_uri_t stream = {.uri = "/stream", .method = HTTP_GET, .handler = stream_handler, .user_ctx = NULL};
    (void)httpd_register_uri_handler(server, &health);
    (void)httpd_register_uri_handler(server, &stream);
    ESP_LOGI(TAG, "MJPEG ready: http://<cam-ip>:%u/stream (%ux%u)", DMS_CAM_STREAM_PORT,
             DMS_CAM_FRAME_WIDTH, DMS_CAM_FRAME_HEIGHT);
}

extern "C" void app_main(void)
{
    if (is_placeholder(DMS_WIFI_SSID) || is_placeholder(DMS_WIFI_PASSWORD)) {
        ESP_LOGE(TAG, "Wi-Fi is not configured; create ignored dms_secrets.h from the example");
        return;
    }

    camera_config_t camera_config = {};
    camera_config.pin_pwdn = CAM_PIN_PWDN;
    camera_config.pin_reset = CAM_PIN_RESET;
    camera_config.pin_xclk = CAM_PIN_XCLK;
    camera_config.pin_sccb_sda = CAM_PIN_SIOD;
    camera_config.pin_sccb_scl = CAM_PIN_SIOC;
    camera_config.pin_d7 = CAM_PIN_D7;
    camera_config.pin_d6 = CAM_PIN_D6;
    camera_config.pin_d5 = CAM_PIN_D5;
    camera_config.pin_d4 = CAM_PIN_D4;
    camera_config.pin_d3 = CAM_PIN_D3;
    camera_config.pin_d2 = CAM_PIN_D2;
    camera_config.pin_d1 = CAM_PIN_D1;
    camera_config.pin_d0 = CAM_PIN_D0;
    camera_config.pin_vsync = CAM_PIN_VSYNC;
    camera_config.pin_href = CAM_PIN_HREF;
    camera_config.pin_pclk = CAM_PIN_PCLK;
    camera_config.xclk_freq_hz = 20000000;
    camera_config.ledc_timer = LEDC_TIMER_0;
    camera_config.ledc_channel = LEDC_CHANNEL_0;
    camera_config.pixel_format = PIXFORMAT_JPEG;
    camera_config.frame_size = DMS_CAM_FRAME_SIZE;
    camera_config.jpeg_quality = DMS_CAM_JPEG_QUALITY;
    camera_config.fb_count = DMS_CAM_FRAMEBUFFER_COUNT;
    camera_config.fb_location = CAMERA_FB_IN_PSRAM;
    camera_config.grab_mode = CAMERA_GRAB_LATEST;
    if (esp_camera_init(&camera_config) != ESP_OK) {
        ESP_LOGE(TAG, "camera initialization failed; verify board profile, GPIOs, sensor, and PSRAM");
        return;
    }
    const sensor_t *sensor = esp_camera_sensor_get();
    ESP_LOGI(TAG, "camera initialized, PID=0x%04X", sensor == NULL ? 0U : sensor->id.PID);

    wifi_events = xEventGroupCreate();
    if (wifi_events == NULL || nvs_flash_init() != ESP_OK || esp_netif_init() != ESP_OK ||
        esp_event_loop_create_default() != ESP_OK || esp_netif_create_default_wifi_sta() == NULL) {
        ESP_LOGE(TAG, "network initialization failed");
        return;
    }
    const wifi_init_config_t wifi_init = WIFI_INIT_CONFIG_DEFAULT();
    wifi_config_t wifi_config = {};
    (void)snprintf(reinterpret_cast<char *>(wifi_config.sta.ssid), sizeof(wifi_config.sta.ssid), "%s", DMS_WIFI_SSID);
    (void)snprintf(reinterpret_cast<char *>(wifi_config.sta.password), sizeof(wifi_config.sta.password), "%s", DMS_WIFI_PASSWORD);
    if (esp_wifi_init(&wifi_init) != ESP_OK ||
        esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL) != ESP_OK ||
        esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL) != ESP_OK ||
        esp_wifi_set_mode(WIFI_MODE_STA) != ESP_OK || esp_wifi_set_config(WIFI_IF_STA, &wifi_config) != ESP_OK ||
        esp_wifi_start() != ESP_OK) {
        ESP_LOGE(TAG, "Wi-Fi start failed");
        return;
    }
    if ((xEventGroupWaitBits(wifi_events, WIFI_CONNECTED_BIT, pdFALSE, pdTRUE, pdMS_TO_TICKS(20000U)) &
         WIFI_CONNECTED_BIT) == 0U) {
        ESP_LOGE(TAG, "Wi-Fi connection timed out");
        return;
    }
    start_http_server();
}
