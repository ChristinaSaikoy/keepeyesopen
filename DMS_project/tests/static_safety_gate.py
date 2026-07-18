"""Source-only guardrails for the G4/G5 architecture boundary."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def read(relative: str) -> str:
    return (ROOT / relative).read_text(encoding="utf-8")


def test_no_application_iram_placement_or_uart_reintroduction():
    source = "\n".join(path.read_text(encoding="utf-8") for path in
                       (ROOT / "esp32_firmware" / "esp32_s3_fw" / "main").glob("*.[ch]"))
    assert "IRAM_ATTR" not in source
    assert "DRAM_ATTR" not in source
    assert "uart_driver_install" not in source


def test_s3_audio_and_command_safety_boundaries_remain():
    source = read("esp32_firmware/esp32_s3_fw/main/main.c")
    parser = read("esp32_firmware/esp32_s3_fw/main/dms_mqtt_contract.c")
    assert "audio_task" in source and "xQueueSend(audio_queue" in source
    assert '"set_status_led"' in parser
    assert "esp_who_detect" not in source


def test_pc_and_cloud_do_not_replace_s3_decision_authority():
    pc = read("pc_prototype/cam_bridge.py")
    cloud = read("cloud_backend/web_backend.py")
    assert "fatigue_level" not in pc
    assert "write_gpio" not in cloud


if __name__ == "__main__":
    test_no_application_iram_placement_or_uart_reintroduction()
    test_s3_audio_and_command_safety_boundaries_remain()
    test_pc_and_cloud_do_not_replace_s3_decision_authority()
    print("static_safety_gate: PASS")
