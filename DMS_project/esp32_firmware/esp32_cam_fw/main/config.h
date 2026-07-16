#ifndef DMS_CAM_CONFIG_H
#define DMS_CAM_CONFIG_H

/* Copy dms_secrets.h.example to ignored dms_secrets.h for real credentials. */
#if defined(__has_include)
#if __has_include("dms_secrets.h")
#include "dms_secrets.h"
#endif
#endif

#ifndef DMS_WIFI_SSID
#define DMS_WIFI_SSID "YOUR_WIFI_SSID"
#endif
#ifndef DMS_WIFI_PASSWORD
#define DMS_WIFI_PASSWORD "YOUR_WIFI_PASSWORD"
#endif

/* AI Thinker OV2640 reference map. Board identity remains hardware-unverified. */
#define CAM_PIN_SIOD 26
#define CAM_PIN_SIOC 27
#define CAM_PIN_D0 5
#define CAM_PIN_D1 18
#define CAM_PIN_D2 19
#define CAM_PIN_D3 21
#define CAM_PIN_D4 36
#define CAM_PIN_D5 39
#define CAM_PIN_D6 34
#define CAM_PIN_D7 35
#define CAM_PIN_PCLK 22
#define CAM_PIN_VSYNC 25
#define CAM_PIN_HREF 23
#define CAM_PIN_XCLK 0
#define CAM_PIN_PWDN 32
#define CAM_PIN_RESET -1

#define DMS_CAM_STREAM_PORT 80
#define DMS_CAM_JPEG_QUALITY 12
#define DMS_CAM_FRAME_SIZE FRAMESIZE_QVGA
#define DMS_CAM_FRAME_WIDTH 320
#define DMS_CAM_FRAME_HEIGHT 240
#define DMS_CAM_FRAME_INTERVAL_MS 0U
#define DMS_CAM_FRAMEBUFFER_COUNT 2

#endif /* DMS_CAM_CONFIG_H */
