"""Strict JSON contracts. Vision observes; only the S3 decides fatigue."""
from __future__ import annotations

import json
import math
import re
from typing import Any, Mapping

SCHEMA_VERSION = 1
DEVICE_RE = re.compile(r"^[A-Za-z0-9_-]{1,48}$")
OBSERVATION_FIELDS = {
    "schema_version", "device_id", "sequence", "source_timestamp_ms", "face_valid",
    "ear", "mar", "head_pitch", "processing_time_ms",
}
ALLOWED_COMMANDS = {"set_status_led"}


class ContractError(ValueError):
    pass


def observation_topic(device_id: str) -> str:
    _device_id(device_id)
    return f"dms/{device_id}/vision/observation"


def state_topic(device_id: str) -> str:
    _device_id(device_id)
    return f"dms/{device_id}/fatigue/state"


def alert_topic(device_id: str) -> str:
    _device_id(device_id)
    return f"dms/{device_id}/fatigue/alert"


def advice_topic(device_id: str) -> str:
    _device_id(device_id)
    return f"dms/{device_id}/ai/advice"


def command_topic(device_id: str) -> str:
    _device_id(device_id)
    return f"dms/{device_id}/command"


def command_ack_topic(device_id: str) -> str:
    _device_id(device_id)
    return f"dms/{device_id}/command/ack"


def _device_id(value: Any) -> str:
    if not isinstance(value, str) or not DEVICE_RE.fullmatch(value):
        raise ContractError("invalid device_id")
    return value


def _uint32(value: Any, name: str) -> int:
    if isinstance(value, bool) or not isinstance(value, int) or not 0 <= value <= 0xFFFFFFFF:
        raise ContractError(f"invalid {name}")
    return value


def _finite_number(value: Any, name: str, minimum: float, maximum: float) -> float:
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise ContractError(f"invalid {name}")
    number = float(value)
    if not math.isfinite(number) or not minimum <= number <= maximum:
        raise ContractError(f"invalid {name}")
    return number


def validate_observation(payload: Mapping[str, Any]) -> dict[str, Any]:
    if not isinstance(payload, Mapping) or set(payload) != OBSERVATION_FIELDS:
        raise ContractError("observation fields do not match schema")
    if _uint32(payload["schema_version"], "schema_version") != SCHEMA_VERSION:
        raise ContractError("unsupported schema_version")
    device_id = _device_id(payload["device_id"])
    if not isinstance(payload["face_valid"], bool):
        raise ContractError("invalid face_valid")
    return {
        "schema_version": SCHEMA_VERSION,
        "device_id": device_id,
        "sequence": _uint32(payload["sequence"], "sequence"),
        "source_timestamp_ms": _uint32(payload["source_timestamp_ms"], "source_timestamp_ms"),
        "face_valid": payload["face_valid"],
        "ear": _finite_number(payload["ear"], "ear", 0.0, 1.5),
        "mar": _finite_number(payload["mar"], "mar", 0.0, 3.0),
        "head_pitch": _finite_number(payload["head_pitch"], "head_pitch", -90.0, 90.0),
        "processing_time_ms": _finite_number(payload["processing_time_ms"], "processing_time_ms", 0.0, 10000.0),
    }


def parse_observation(topic: str, raw: bytes | str) -> dict[str, Any]:
    try:
        decoded = raw.decode("utf-8") if isinstance(raw, bytes) else raw
        value = json.loads(decoded, parse_constant=lambda _: (_ for _ in ()).throw(ContractError("non-finite JSON")))
    except (UnicodeDecodeError, json.JSONDecodeError, ContractError) as error:
        raise ContractError("invalid observation JSON") from error
    observation = validate_observation(value)
    if topic != observation_topic(observation["device_id"]):
        raise ContractError("observation topic/device mismatch")
    return observation


def validate_command(payload: Mapping[str, Any], expected_device_id: str) -> dict[str, Any]:
    required = {"schema_version", "device_id", "command_id", "action", "enabled"}
    if not isinstance(payload, Mapping) or set(payload) != required:
        raise ContractError("command fields do not match schema")
    if _uint32(payload["schema_version"], "schema_version") != SCHEMA_VERSION:
        raise ContractError("unsupported schema_version")
    if _device_id(payload["device_id"]) != expected_device_id:
        raise ContractError("command device mismatch")
    command_id = _device_id(payload["command_id"])
    if payload["action"] not in ALLOWED_COMMANDS or not isinstance(payload["enabled"], bool):
        raise ContractError("command is not whitelisted")
    return {"command_id": command_id, "action": payload["action"], "enabled": payload["enabled"]}
