"""Cloud dashboard and asynchronous advice loop. LLM output never controls hardware."""
from __future__ import annotations

import asyncio
import json
import os
import sys
import threading
import time
from pathlib import Path
from typing import Any, Callable

try:
    import paho.mqtt.client as mqtt
except ModuleNotFoundError:
    mqtt = None
try:
    import requests
    REQUEST_ERRORS = (requests.RequestException,)
except ModuleNotFoundError:
    requests = None
    REQUEST_ERRORS = ()
try:
    import websockets
except ModuleNotFoundError:
    websockets = None
try:
    from dotenv import load_dotenv
except ModuleNotFoundError:
    def load_dotenv() -> bool:
        return False

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from shared.dms_contract import ALLOWED_COMMANDS, ContractError, advice_topic, alert_topic, command_topic, parse_observation, validate_command

load_dotenv()
BROKER = os.getenv("MQTT_BROKER", "127.0.0.1")
PORT = int(os.getenv("MQTT_PORT", "1883"))
LLM_TIMEOUT_SECONDS = float(os.getenv("LLM_TIMEOUT_SECONDS", "2.5"))
FALLBACK_ADVICE = "请立即安全停车并休息，不要继续疲劳驾驶。"

latest: dict[str, Any] = {"ear": 0.0, "mar": 0.0, "level": 0, "perclos": 0.0, "link_status": "waiting", "ai_advice": ""}
latest_lock = threading.Lock()
alert_queue: asyncio.Queue[dict[str, Any]] | None = None
mqtt_client: mqtt.Client | None = None


def _finite_number(value: Any, minimum: float, maximum: float) -> float:
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise ContractError("invalid number")
    value = float(value)
    if not minimum <= value <= maximum:
        raise ContractError("out of range")
    return value


def validate_s3_state(payload: Any) -> dict[str, Any]:
    required = {"schema_version", "device_id", "sequence", "source_timestamp_ms", "current_fatigue_level",
                "alert_due", "perclos", "observation_valid", "link_status", "last_sequence", "cause"}
    if not isinstance(payload, dict) or set(payload) != required or payload["schema_version"] != 1:
        raise ContractError("invalid S3 state schema")
    if not isinstance(payload["device_id"], str) or not isinstance(payload["alert_due"], bool) or not isinstance(payload["observation_valid"], bool):
        raise ContractError("invalid S3 state type")
    if payload["link_status"] not in {"disconnected", "waiting", "active", "stale", "invalid"}:
        raise ContractError("invalid link status")
    level = payload["current_fatigue_level"]
    if isinstance(level, bool) or not isinstance(level, int) or level not in {0, 2, 3}:
        raise ContractError("invalid fatigue level")
    payload = dict(payload)
    payload["perclos"] = _finite_number(payload["perclos"], 0.0, 1.0)
    return payload


def fixed_or_remote_request(alert: dict[str, Any]) -> str:
    api_url = os.getenv("LLM_API_URL", "")
    api_key = os.getenv("LLM_API_KEY", "")
    model = os.getenv("LLM_MODEL", "")
    if not api_url or "YOUR_" in api_url or not api_key or "YOUR_" in api_key or not model or "YOUR_" in model:
        raise RuntimeError("LLM is not configured")
    if requests is None:
        raise RuntimeError("requests dependency is unavailable")
    prompt = (
        "Give one short Chinese non-controlling driver safety suggestion. "
        f"Fatigue level={alert['current_fatigue_level']}, perclos={alert['perclos']:.2f}. "
        "Do not instruct device GPIO, vehicle controls, or autonomous action."
    )
    response = requests.post(api_url, headers={"Authorization": f"Bearer {api_key}"}, json={
        "model": model, "messages": [{"role": "user", "content": prompt}], "temperature": 0.2, "max_tokens": 80,
    }, timeout=LLM_TIMEOUT_SECONDS)
    response.raise_for_status()
    text = response.json()["choices"][0]["message"]["content"].strip()
    if not text:
        raise RuntimeError("empty LLM response")
    return text[:240]


async def generate_advice(alert: dict[str, Any], requester: Callable[[dict[str, Any]], str] = fixed_or_remote_request) -> tuple[str, bool]:
    try:
        return await asyncio.wait_for(asyncio.to_thread(requester, alert), timeout=LLM_TIMEOUT_SECONDS), False
    except Exception as error:
        expected = (asyncio.TimeoutError, RuntimeError, KeyError, ValueError) + REQUEST_ERRORS
        if isinstance(error, expected):
            return FALLBACK_ADVICE, True
        raise


def build_whitelisted_command(device_id: str, command: dict[str, Any]) -> tuple[str, str]:
    validated = validate_command(command, device_id)
    if validated["action"] not in ALLOWED_COMMANDS:
        raise ContractError("command is not whitelisted")
    return command_topic(device_id), json.dumps({"schema_version": 1, "device_id": device_id, **validated}, separators=(",", ":"))


def update_latest(state: dict[str, Any]) -> None:
    with latest_lock:
        latest.update({"level": state["current_fatigue_level"], "perclos": state["perclos"],
                       "link_status": state["link_status"], "observation_valid": state["observation_valid"]})


def mqtt_on_message(_: mqtt.Client, userdata: asyncio.AbstractEventLoop, message: mqtt.MQTTMessage) -> None:
    if message.topic.endswith("/vision/observation"):
        try:
            observation = parse_observation(message.topic, message.payload)
        except ContractError:
            return
        with latest_lock:
            latest.update({"ear": observation["ear"], "mar": observation["mar"],
                           "head_pitch": observation["head_pitch"], "vision_processing_ms": observation["processing_time_ms"]})
        return
    try:
        payload = validate_s3_state(json.loads(message.payload.decode("utf-8")))
    except (UnicodeDecodeError, json.JSONDecodeError, ContractError):
        return
    update_latest(payload)
    if message.topic == alert_topic(payload["device_id"]) and payload["alert_due"] and alert_queue is not None:
        userdata.call_soon_threadsafe(alert_queue.put_nowait, payload)


def mqtt_worker(loop: asyncio.AbstractEventLoop) -> mqtt.Client:
    if mqtt is None:
        raise RuntimeError("paho-mqtt dependency is unavailable")
    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
    def on_connect(current, _u, _f, _r, _p) -> None:
        current.subscribe("dms/+/fatigue/#", qos=1)
        current.subscribe("dms/+/vision/observation", qos=1)
    client.on_connect = on_connect
    client.on_message = lambda current, _u, message: mqtt_on_message(current, loop, message)
    client.connect(BROKER, PORT, 60)
    client.loop_start()
    return client


async def advice_worker() -> None:
    assert alert_queue is not None
    while True:
        alert = await alert_queue.get()
        advice, fallback = await generate_advice(alert)
        payload = {"schema_version": 1, "device_id": alert["device_id"], "source_timestamp_ms": int(time.time() * 1000) & 0xFFFFFFFF,
                   "alert_sequence": alert["sequence"], "advice": advice, "fallback": fallback}
        assert mqtt_client is not None
        mqtt_client.publish(advice_topic(alert["device_id"]), json.dumps(payload, separators=(",", ":")), qos=1)
        with latest_lock:
            latest["ai_advice"] = advice
            latest["ai_fallback"] = fallback


async def dashboard(websocket) -> None:
    while True:
        with latest_lock:
            snapshot = dict(latest)
        await websocket.send(json.dumps(snapshot, ensure_ascii=False))
        await asyncio.sleep(0.1)


async def main() -> None:
    global alert_queue, mqtt_client
    if mqtt is None or websockets is None:
        raise RuntimeError("install requirements.txt before starting the backend")
    alert_queue = asyncio.Queue(maxsize=32)
    mqtt_client = mqtt_worker(asyncio.get_running_loop())
    asyncio.create_task(advice_worker())
    print(f"Dashboard: ws://127.0.0.1:9001 MQTT:{BROKER}:{PORT}")
    async with websockets.serve(dashboard, "127.0.0.1", 9001):
        await asyncio.Future()


if __name__ == "__main__":
    asyncio.run(main())
