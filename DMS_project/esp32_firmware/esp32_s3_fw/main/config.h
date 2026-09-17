/**
 * ESP32-S3 算力板配置 —— UART接收 → EAR/MAR → 状态判定 → I2S音频 + MQTT + IR LED
 */
#ifndef CONFIG_H
#define CONFIG_H

// ── UART 接收（接 ESP32-CAM 的 TX）──
#define UART_RX_PIN     17
#define UART_TX_PIN     18    // 预留（调试用，不用接）
#define UART_BAUD       115200
#define UART_NUM        1

// ── MAX98357 I2S 音频 ──
#define I2S_BCLK_PIN    14
#define I2S_LRC_PIN     4
#define I2S_DIN_PIN     5
#define I2S_SD_PIN      6

// ── 红外 LED ──
#define IR_LED_PIN      7
#define STATUS_LED_PIN  48

// ── WiFi / MQTT ──
// Public-repository defaults only. Replace locally for deployment.
#define WIFI_SSID       "your_wifi"
#define WIFI_PASSWORD   "your_password"
#define MQTT_BROKER_URL "mqtt://broker.emqx.io"
#define MQTT_TOPIC      "dms/car/data"

// ── 疲劳阈值（竞赛原型参数，非安全/医学标准）──
#define EAR_THRESHOLD         0.18f
#define MAR_THRESHOLD         0.60f
#define BLINK_MICRO_SLEEP_MS  500
#define BLINK_DEEP_SLEEP_MS   1500
#define YAWN_DURATION_MS      2500
#define COOLDOWN_MS           4000

// ── 测试模式 ──
// 0=预留全功能 1=UART收+串口打印 2=I2S音频 3=IR LED 4=WiFi+MQTT 5=综合入口（开发中）
#define TEST_MODE  1

#endif
