# 端边云协同车载 DMS 原型

> 全国大学生物联网设计竞赛 · 乐鑫命题  
> 团队：不闭眼战车队  
> 当前状态：**原型开发中，部分子系统已实现，完整端到端闭环尚未完成**

本项目探索一个分层的 Driver Monitoring System（驾驶员监测系统）原型：摄像头节点负责采集与后续关键点提取，ESP32-S3 负责嵌入式疲劳判定与本地告警，MQTT/PC 后端负责遥测、可视化和可选的 LLM/TTS 交互。

## 1. 当前系统结构

```text
驾驶员 / 摄像头
      |
      v
ESP32 Camera Node
  摄像头采集
  UART 发送
  68点关键点提取 [待集成]
      |
      v
ESP32-S3 Compute Node
  EAR / MAR 几何计算
  闭眼 / 哈欠持续时间状态机
  I2S 本地告警
  IR / 状态指示
  Wi-Fi + MQTT
      |
      v
MQTT Broker
      |
      v
PC / Cloud Backend
  MQTT 接收
  WebSocket Dashboard
  DeepSeek 可选生成
  本地语料 fallback
  TTS
```

## 2. 实现状态

| 模块 | 状态 | 代码位置 |
|---|---|---|
| ESP32 Camera 初始化 / 图像采集 | 已实现 | `esp32_firmware/esp32_cam_fw/` |
| Camera -> S3 UART 链路框架 | 已实现，当前发送占位数据 | `esp32_cam_fw/main/main.c` |
| Camera 侧 68 点关键点提取 | 待集成 | `esp32_cam_fw/main/main.c` 中 TODO |
| EAR / MAR 几何算法 | 已实现 | `esp32_s3_fw/main/ear_mar.c` |
| 闭眼 / 哈欠持续时间判定 | 已实现 | `esp32_s3_fw/main/perclos.c` |
| UART / I2S / IR / Wi-Fi / MQTT 子系统测试 | 已实现 | `esp32_s3_fw/main/main.c` |
| S3 全功能综合模式 | 开发中 | `TEST_MODE=5` 当前仍为占位 |
| PC 端视觉原型 | 已有原型 | `pc_prototype/` |
| MQTT + WebSocket 后端 | 已实现原型 | `cloud_backend/web_backend.py` |
| DeepSeek + 本地 fallback + TTS | 已实现原型 | `cloud_backend/web_backend.py` |
| 完整板级端到端验证 | 待完成 | — |

> 说明：文件名 `perclos.c` 沿用项目早期命名。当前代码实现的是**基于 EAR/MAR 阈值与持续时间的疲劳状态机**，并不是完整的滑动窗口 PERCLOS 百分比算法。这里按实际实现描述，避免把原型能力写成已经完成的算法。

## 3. 项目目录

```text
DMS_project/
├── README.md
├── requirements.txt
├── .env.example
├── protocol.json
│
├── pc_prototype/
│   ├── face_capture.py
│   └── dms_logic.py
│
├── cloud_backend/
│   ├── web_backend.py
│   └── index.html
│
└── esp32_firmware/
    ├── esp32_cam_fw/
    │   ├── CMakeLists.txt
    │   └── main/
    │       ├── CMakeLists.txt
    │       ├── config.h
    │       └── main.c
    │
    └── esp32_s3_fw/
        ├── CMakeLists.txt
        └── main/
            ├── CMakeLists.txt
            ├── config.h
            ├── main.c
            ├── ear_mar.c
            ├── ear_mar.h
            ├── perclos.c
            └── perclos.h
```

## 4. 团队分工

本仓库是团队项目，不能把全部代码视为单人作品。

| 成员 | 主要工作 |
|---|---|
| 陈俊毅（队长） | PC 端原型、系统协同 |
| 覃晖 | 硬件与板级外设联调 |
| 廖宜乐 | 嵌入式算法：EAR/MAR、闭眼/哈欠状态逻辑、S3 算法路径设计 |
| 王宏博 | MQTT / WebSocket / LLM / TTS 后端 |

### 廖宜乐的主要代码范围

- `esp32_firmware/esp32_s3_fw/main/ear_mar.c/.h`
- `esp32_firmware/esp32_s3_fw/main/perclos.c/.h`
- Camera landmark -> S3 fatigue-state pipeline 的算法侧设计与集成工作

## 5. 嵌入式算法链路

### EAR / MAR

`ear_mar.c` 接收 68 点面部关键点，计算：

- EAR（Eye Aspect Ratio，眼睛纵横比）
- MAR（Mouth Aspect Ratio，嘴部纵横比）

双眼 EAR 取平均值；嘴部 MAR 由多组纵向距离与横向尺度归一化得到。

### 时间状态机

`perclos.c` 当前根据：

- EAR 是否低于闭眼阈值
- MAR 是否高于哈欠阈值
- 状态持续时间

区分正常、微睡眠/持续闭眼、深度持续闭眼和哈欠事件。

这套逻辑的优势是**时间基准不依赖固定帧数**；但当前仍属于竞赛原型参数，尚未在公开仓库中提供大规模受试者统计验证。

## 6. S3 子系统测试模式

`esp32_s3_fw/main/config.h` 使用 `TEST_MODE` 选择验证路径：

| TEST_MODE | 当前用途 |
|---|---|
| `1` | UART 接收测试 |
| `2` | I2S 音频测试 |
| `3` | IR LED 测试 |
| `4` | Wi-Fi + MQTT 测试 |
| `5` | 综合模式入口，目前仍待完成 |

## 7. MQTT 数据协议

```json
{
  "device_id": "ESP32_DMS_001",
  "data": {
    "ear": 0.28,
    "mar": 0.15
  },
  "status": {
    "fatigue_level": 0,
    "desc": "Normal"
  }
}
```

默认 topic：`dms/car/data`

## 8. 后端原型

`cloud_backend/web_backend.py` 当前提供：

- MQTT 消息接收
- WebSocket 推送到网页大屏
- DeepSeek API 可选生成提醒文本
- API 不可用时使用本地语料 fallback
- 本地 pyttsx3 TTS

API Key 通过环境变量读取，不应硬编码进仓库。

## 9. 当前限制

- Camera 侧还没有完成 68 点关键点提取接入。
- UART 当前仍使用占位 payload 验证链路。
- S3 综合运行路径尚未完成。
- 尚未公开可复现的准确率、误报率、端到端延迟、功耗和长期稳定性 benchmark。
- 当前阈值属于项目原型配置，不应理解为医学或安全标准。

## 10. 下一步

1. 接入 Camera 侧关键点提取。
2. 冻结 UART 关键点 / 事件数据协议。
3. 完成 S3 综合处理模式。
4. 给 EAR/MAR 和状态机加入 host-side 单元测试。
5. 完成板级端到端验证并记录证据。
6. 再进行 latency / robustness / power benchmark。
