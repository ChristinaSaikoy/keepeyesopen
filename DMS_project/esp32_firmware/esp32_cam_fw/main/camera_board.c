#include <stddef.h>
#include <string.h>

#include "camera_board.h"
#include "config.h"

static bool pin_is_camera_owned(int pin)
{
    const int camera_pins[] = {CAM_PIN_D0,   CAM_PIN_D1,    CAM_PIN_D2,   CAM_PIN_D3,
                               CAM_PIN_D4,   CAM_PIN_D5,    CAM_PIN_D6,   CAM_PIN_D7,
                               CAM_PIN_XCLK, CAM_PIN_PCLK,  CAM_PIN_VSYNC, CAM_PIN_HREF,
                               CAM_PIN_SIOD, CAM_PIN_SIOC,  CAM_PIN_PWDN, CAM_PIN_RESET};
    for (size_t i = 0U; i < sizeof(camera_pins) / sizeof(camera_pins[0]); ++i) {
        if (pin >= 0 && pin == camera_pins[i]) {
            return true;
        }
    }
    return false;
}

dms_camera_board_status_t dms_camera_board_validate(void)
{
    if (CAMERA_BOARD_PROFILE == CAMERA_BOARD_PROFILE_USER_DEFINED) {
        return DMS_CAMERA_BOARD_UNCONFIGURED;
    }
    const int camera_pins[] = {CAM_PIN_D0,   CAM_PIN_D1,    CAM_PIN_D2,   CAM_PIN_D3,
                               CAM_PIN_D4,   CAM_PIN_D5,    CAM_PIN_D6,   CAM_PIN_D7,
                               CAM_PIN_XCLK, CAM_PIN_PCLK,  CAM_PIN_VSYNC, CAM_PIN_HREF,
                               CAM_PIN_SIOD, CAM_PIN_SIOC,  CAM_PIN_PWDN, CAM_PIN_RESET};
    for (size_t i = 0U; i < sizeof(camera_pins) / sizeof(camera_pins[0]); ++i) {
        if (camera_pins[i] < -1) {
            return DMS_CAMERA_BOARD_UNCONFIGURED;
        }
        for (size_t j = i + 1U; j < sizeof(camera_pins) / sizeof(camera_pins[0]); ++j) {
            if (camera_pins[i] >= 0 && camera_pins[i] == camera_pins[j]) {
                return DMS_CAMERA_BOARD_PIN_CONFLICT;
            }
        }
    }
    if (pin_is_camera_owned(CAM_FLASH_LED_PIN) || pin_is_camera_owned(CAM_UART_TX_PIN) ||
        (CAM_FLASH_LED_PIN >= 0 && CAM_FLASH_LED_PIN == CAM_UART_TX_PIN)) {
        return DMS_CAMERA_BOARD_PIN_CONFLICT;
    }
    return DMS_CAMERA_BOARD_OK;
}

const char *dms_camera_board_status_name(dms_camera_board_status_t status)
{
    switch (status) {
    case DMS_CAMERA_BOARD_OK:
        return "ok";
    case DMS_CAMERA_BOARD_UNCONFIGURED:
        return "USER ACTION REQUIRED: camera board profile/pins are unconfigured";
    case DMS_CAMERA_BOARD_PIN_CONFLICT:
        return "camera GPIO conflicts with flash LED or UART TX";
    default:
        return "unknown camera board status";
    }
}

void dms_camera_board_fill_config(camera_config_t *config)
{
    if (config == NULL) {
        return;
    }
    memset(config, 0, sizeof(*config));
    config->pin_pwdn = CAM_PIN_PWDN;
    config->pin_reset = CAM_PIN_RESET;
    config->pin_xclk = CAM_PIN_XCLK;
    config->pin_sccb_sda = CAM_PIN_SIOD;
    config->pin_sccb_scl = CAM_PIN_SIOC;
    config->pin_d7 = CAM_PIN_D7;
    config->pin_d6 = CAM_PIN_D6;
    config->pin_d5 = CAM_PIN_D5;
    config->pin_d4 = CAM_PIN_D4;
    config->pin_d3 = CAM_PIN_D3;
    config->pin_d2 = CAM_PIN_D2;
    config->pin_d1 = CAM_PIN_D1;
    config->pin_d0 = CAM_PIN_D0;
    config->pin_vsync = CAM_PIN_VSYNC;
    config->pin_href = CAM_PIN_HREF;
    config->pin_pclk = CAM_PIN_PCLK;
    config->xclk_freq_hz = CAM_XCLK_FREQ_HZ;
    config->ledc_timer = LEDC_TIMER_0;
    config->ledc_channel = LEDC_CHANNEL_0;
    config->pixel_format = PIXFORMAT_RGB565;
    config->frame_size = FRAMESIZE_QVGA;
    config->jpeg_quality = 12;
    config->fb_count = 1;
    config->grab_mode = CAMERA_GRAB_WHEN_EMPTY;
    config->fb_location = CAMERA_FB_IN_PSRAM;
}
