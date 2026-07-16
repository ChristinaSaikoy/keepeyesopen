import asyncio
import json
import sys
import time
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))

from cloud_backend import web_backend
from shared.dms_contract import ContractError, observation_topic, parse_observation, validate_command


def valid_observation(sequence=1):
    return {"schema_version": 1, "device_id": "pc_dms_001", "sequence": sequence,
            "source_timestamp_ms": 123, "face_valid": True, "ear": 0.25, "mar": 0.15,
            "head_pitch": 0.0, "processing_time_ms": 8.0}


class ContractTests(unittest.TestCase):
    def test_schema_rejects_invalid_json_nan_and_missing_field(self):
        topic = observation_topic("pc_dms_001")
        with self.assertRaises(ContractError):
            parse_observation(topic, b'{bad json')
        with self.assertRaises(ContractError):
            parse_observation(topic, b'{"schema_version":1,"device_id":"pc_dms_001","sequence":1,"source_timestamp_ms":1,"face_valid":true,"ear":NaN,"mar":0.1,"head_pitch":0,"processing_time_ms":1}')
        message = valid_observation()
        del message["mar"]
        with self.assertRaises(ContractError):
            parse_observation(topic, json.dumps(message))

    def test_topic_and_command_whitelist(self):
        message = valid_observation()
        with self.assertRaises(ContractError):
            parse_observation("dms/other/vision/observation", json.dumps(message))
        accepted = validate_command({"schema_version": 1, "device_id": "esp32s3_dms_01", "command_id": "cmd_1",
                                     "action": "set_status_led", "enabled": True}, "esp32s3_dms_01")
        self.assertTrue(accepted["enabled"])
        with self.assertRaises(ContractError):
            validate_command({"schema_version": 1, "device_id": "esp32s3_dms_01", "command_id": "cmd_2",
                              "action": "write_gpio_99", "enabled": True}, "esp32s3_dms_01")

    def test_llm_timeout_uses_fixed_fallback(self):
        old_timeout = web_backend.LLM_TIMEOUT_SECONDS
        web_backend.LLM_TIMEOUT_SECONDS = 0.01
        try:
            def slow_request(_):
                time.sleep(0.1)
                return "late"
            advice, fallback = asyncio.run(web_backend.generate_advice({"current_fatigue_level": 3, "perclos": 0.5}, slow_request))
            self.assertTrue(fallback)
            self.assertEqual(advice, web_backend.FALLBACK_ADVICE)
        finally:
            web_backend.LLM_TIMEOUT_SECONDS = old_timeout


if __name__ == "__main__":
    unittest.main()
