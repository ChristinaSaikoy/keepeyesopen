#ifndef DMS_CAM_CONFIG_H
#define DMS_CAM_CONFIG_H

#define CAMERA_BOARD_PROFILE_USER_DEFINED 0
#define CAMERA_BOARD_PROFILE_AI_THINKER 1

/* USER ACTION REQUIRED: leave this as USER_DEFINED until the hardware teammate
 * confirms both the ESP32-CAM board profile and the camera/UART wiring. */
#ifndef CAMERA_BOARD_PROFILE
#define CAMERA_BOARD_PROFILE CAMERA_BOARD_PROFILE_USER_DEFINED
#endif

#if CAMERA_BOARD_PROFILE == CAMERA_BOARD_PROFILE_AI_THINKER
/* Reference AI-Thinker OV2640 map. This repository has not validated it on a board. */
#define CAM_PIN_PWDN 32
#define CAM_PIN_RESET -1
#define CAM_PIN_XCLK 0
#define CAM_PIN_SIOD 26
#define CAM_PIN_SIOC 27
#define CAM_PIN_D7 35
#define CAM_PIN_D6 34
#define CAM_PIN_D5 39
#define CAM_PIN_D4 36
#define CAM_PIN_D3 21
#define CAM_PIN_D2 19
#define CAM_PIN_D1 18
#define CAM_PIN_D0 5
#define CAM_PIN_VSYNC 25
#define CAM_PIN_HREF 23
#define CAM_PIN_PCLK 22
#define CAM_FLASH_LED_PIN 4
#elif CAMERA_BOARD_PROFILE == CAMERA_BOARD_PROFILE_USER_DEFINED
/* USER ACTION REQUIRED: define every camera GPIO after physical verification. */
#define CAM_PIN_PWDN -1
#define CAM_PIN_RESET -1
#define CAM_PIN_XCLK -1
#define CAM_PIN_SIOD -1
#define CAM_PIN_SIOC -1
#define CAM_PIN_D7 -1
#define CAM_PIN_D6 -1
#define CAM_PIN_D5 -1
#define CAM_PIN_D4 -1
#define CAM_PIN_D3 -1
#define CAM_PIN_D2 -1
#define CAM_PIN_D1 -1
#define CAM_PIN_D0 -1
#define CAM_PIN_VSYNC -1
#define CAM_PIN_HREF -1
#define CAM_PIN_PCLK -1
#define CAM_FLASH_LED_PIN -1
#else
#error "Unsupported CAMERA_BOARD_PROFILE"
#endif

/* USER ACTION REQUIRED: verify CAM TX to S3 RX and common ground. */
#define CAM_UART_PORT UART_NUM_1
#define CAM_UART_TX_PIN 14
#define CAM_UART_BAUD_RATE 115200

#define CAM_MODE_CAPTURE_TEST 1
#define CAM_MODE_UART_LINK_TEST 2
#define CAM_MODE_MODEL_PIPELINE 3
#define CAM_MODE CAM_MODE_MODEL_PIPELINE

#define CAM_XCLK_FREQ_HZ 20000000U
#define CAM_INIT_MAX_ATTEMPTS 3U
#define CAM_INIT_RETRY_DELAY_MS 500U
#define CAM_MAX_CONSECUTIVE_CAPTURE_FAILURES 3U
#define CAM_CAPTURE_INTERVAL_MS 100U
#define CAM_HEARTBEAT_INTERVAL_MS 1000U
#define CAM_MODEL_UNAVAILABLE_INTERVAL_MS 2000U

#endif /* DMS_CAM_CONFIG_H */
