"""HTTP/MJPEG latest-frame MediaPipe observer. It never decides fatigue level."""
from __future__ import annotations

import argparse
import json
import os
import queue
import statistics
import sys
import threading
import time
import urllib.request
from dataclasses import dataclass, field
from pathlib import Path

import cv2
import mediapipe as mp
import numpy as np
import paho.mqtt.client as mqtt
from dotenv import load_dotenv
from mediapipe.tasks import python, vision

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from shared.dms_contract import advice_topic, observation_topic, validate_observation


def now_epoch_ms() -> int:
    return int(time.time() * 1000) & 0xFFFFFFFF


def distance(first: tuple[int, int], second: tuple[int, int]) -> float:
    return float(np.linalg.norm(np.asarray(first) - np.asarray(second)))


def compute_ear(points: list[tuple[int, int]]) -> float:
    denominator = 2.0 * distance(points[0], points[3])
    return 0.0 if denominator < 1e-6 else (distance(points[1], points[5]) + distance(points[2], points[4])) / denominator


def compute_mar(points: list[tuple[int, int]]) -> float:
    denominator = 3.0 * distance(points[0], points[4])
    return 0.0 if denominator < 1e-6 else (distance(points[1], points[7]) + distance(points[2], points[6]) + distance(points[3], points[5])) / denominator


def estimate_pitch(points: list[tuple[int, int]]) -> float:
    """A bounded image-plane proxy, not IMU sensor fusion or calibrated head pose."""
    nose, left_eye, right_eye = points[1], points[33], points[263]
    eye_mid_y = (left_eye[1] + right_eye[1]) / 2.0
    eye_width = max(1.0, abs(right_eye[0] - left_eye[0]))
    return max(-90.0, min(90.0, (nose[1] - eye_mid_y) * 90.0 / eye_width))


class LatestFrameQueue:
    def __init__(self) -> None:
        self.queue: queue.Queue[tuple[bytes, float]] = queue.Queue(maxsize=1)
        self.received = 0
        self.replaced = 0

    def put(self, frame: bytes) -> None:
        self.received += 1
        item = (frame, time.monotonic())
        try:
            self.queue.put_nowait(item)
        except queue.Full:
            _ = self.queue.get_nowait()
            self.replaced += 1
            self.queue.put_nowait(item)

    def get(self) -> tuple[bytes, float]:
        return self.queue.get()

    @property
    def drop_rate(self) -> float:
        return 0.0 if self.received == 0 else self.replaced / self.received


@dataclass
class LatencyStats:
    samples_ms: list[float] = field(default_factory=list)

    def add(self, value_ms: float) -> None:
        self.samples_ms.append(value_ms)
        if len(self.samples_ms) > 2000:
            del self.samples_ms[:1000]

    def summary(self) -> dict[str, float]:
        if not self.samples_ms:
            return {"avg": 0.0, "p50": 0.0, "p95": 0.0, "max": 0.0}
        ordered = sorted(self.samples_ms)
        def percentile(value: float) -> float:
            return ordered[min(len(ordered) - 1, int((len(ordered) - 1) * value))]
        return {"avg": statistics.fmean(ordered), "p50": percentile(0.50), "p95": percentile(0.95), "max": ordered[-1]}


def mjpeg_frames(url: str):
    with urllib.request.urlopen(url, timeout=10) as response:
        while True:
            line = response.readline()
            if not line:
                return
            if not line.startswith(b"--"):
                continue
            headers: dict[bytes, bytes] = {}
            while True:
                line = response.readline()
                if line in (b"\r\n", b"\n", b""):
                    break
                key, _, value = line.partition(b":")
                headers[key.lower()] = value.strip()
            length = int(headers.get(b"content-length", b"0"))
            if length <= 0 or length > 2_000_000:
                continue
            frame = response.read(length)
            if len(frame) == length:
                yield frame


def create_detector(model_path: Path):
    options = vision.FaceLandmarkerOptions(
        base_options=python.BaseOptions(model_asset_path=str(model_path)),
        num_faces=1,
        running_mode=vision.RunningMode.IMAGE,
    )
    return vision.FaceLandmarker.create_from_options(options)


class Observer:
    def __init__(self, client: mqtt.Client, device_id: str, detector) -> None:
        self.client = client
        self.device_id = device_id
        self.detector = detector
        self.sequence = 0
        self.pipeline_latency = LatencyStats()
        self.mediapipe_latency = LatencyStats()

    def publish_frame(self, jpeg: bytes, received_at: float) -> None:
        start = time.monotonic()
        frame = cv2.imdecode(np.frombuffer(jpeg, dtype=np.uint8), cv2.IMREAD_COLOR)
        if frame is None:
            return
        height, width = frame.shape[:2]
        image = mp.Image(image_format=mp.ImageFormat.SRGB, data=cv2.cvtColor(frame, cv2.COLOR_BGR2RGB))
        result = self.detector.detect(image)
        face_valid = bool(result.face_landmarks)
        ear = mar = head_pitch = 0.0
        if face_valid:
            points = [(int(item.x * width), int(item.y * height)) for item in result.face_landmarks[0]]
            left_eye = [points[index] for index in (33, 160, 158, 133, 153, 144)]
            right_eye = [points[index] for index in (362, 385, 387, 263, 373, 380)]
            mouth = [points[index] for index in (78, 81, 13, 311, 308, 178, 14, 87)]
            ear = (compute_ear(left_eye) + compute_ear(right_eye)) / 2.0
            mar = compute_mar(mouth)
            head_pitch = estimate_pitch(points)
        processing_ms = (time.monotonic() - start) * 1000.0
        observation = validate_observation({
            "schema_version": 1,
            "device_id": self.device_id,
            "sequence": self.sequence,
            "source_timestamp_ms": now_epoch_ms(),
            "face_valid": face_valid,
            "ear": ear,
            "mar": mar,
            "head_pitch": head_pitch,
            "processing_time_ms": processing_ms,
        })
        self.sequence = (self.sequence + 1) & 0xFFFFFFFF
        self.client.publish(observation_topic(self.device_id), json.dumps(observation, separators=(",", ":")), qos=1)
        self.mediapipe_latency.add(processing_ms)
        self.pipeline_latency.add((time.monotonic() - received_at) * 1000.0)


def main() -> None:
    load_dotenv()
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", choices=("mjpeg", "webcam"), default=os.getenv("PC_SOURCE", "mjpeg"))
    parser.add_argument("--stream-url", default=os.getenv("CAM_STREAM_URL", "http://ESP32_CAM_LOCAL_IP/stream"))
    args = parser.parse_args()
    device_id = os.getenv("MQTT_DEVICE_ID", "pc_dms_001")
    broker = os.getenv("MQTT_BROKER", "127.0.0.1")
    port = int(os.getenv("MQTT_PORT", "1883"))
    model_path = Path(__file__).with_name("face_landmarker.task")
    if not model_path.exists():
        raise SystemExit("face_landmarker.task is required locally; do not download during a safety test")
    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
    client.connect(broker, port, 60)
    client.loop_start()
    if os.getenv("PC_TTS_ENABLED", "0") == "1":
        advice_queue: queue.Queue[str] = queue.Queue(maxsize=4)
        def on_advice(_: mqtt.Client, _u, message: mqtt.MQTTMessage) -> None:
            try:
                advice = json.loads(message.payload.decode("utf-8"))["advice"]
                try:
                    advice_queue.put_nowait(advice)
                except queue.Full:
                    _ = advice_queue.get_nowait()
                    advice_queue.put_nowait(advice)
            except (UnicodeDecodeError, json.JSONDecodeError, KeyError):
                return
        def tts_worker() -> None:
            try:
                import pyttsx3
                engine = pyttsx3.init()
            except Exception as error:
                print(f"PC TTS unavailable: {error}")
                return
            while True:
                engine.say(advice_queue.get())
                engine.runAndWait()
        client.on_message = on_advice
        client.subscribe(advice_topic(os.getenv("S3_DEVICE_ID", "esp32s3_dms_01")), qos=1)
        threading.Thread(target=tts_worker, daemon=True, name="pc-advice-tts").start()
    observer = Observer(client, device_id, create_detector(model_path))
    frames = LatestFrameQueue()

    if args.source == "mjpeg":
        def reader() -> None:
            for jpeg in mjpeg_frames(args.stream_url):
                frames.put(jpeg)
        threading.Thread(target=reader, daemon=True, name="mjpeg-reader").start()
        while True:
            jpeg, received_at = frames.get()
            observer.publish_frame(jpeg, received_at)
            if observer.sequence % 30 == 0:
                print(json.dumps({"latency_ms": {"pc_receive_to_publish": observer.pipeline_latency.summary(),
                                                 "mediapipe": observer.mediapipe_latency.summary()},
                                  "drop_rate": frames.drop_rate}))
    else:
        capture = cv2.VideoCapture(0, cv2.CAP_DSHOW)
        if not capture.isOpened():
            raise SystemExit("webcam source is unavailable")
        while capture.isOpened():
            ok, frame = capture.read()
            if not ok:
                continue
            ok, encoded = cv2.imencode(".jpg", frame)
            if ok:
                observer.publish_frame(encoded.tobytes(), time.monotonic())


if __name__ == "__main__":
    main()
