# Shared UART Protocol Component

Both firmware projects compile this component. It owns byte order, framing,
CRC-16/CCITT-FALSE, and stream parsing; neither endpoint sends a C struct
directly to `uart_write_bytes`.

Frame bytes are `magic[2]`, `version`, `message_type`, `payload_length` (u16
big-endian), `sequence` (u32 big-endian), payload, and CRC (u16 big-endian).
CRC covers bytes from `version` through the final payload byte. The protocol
version is 1, maximum payload is 64 bytes, CRC initial value is `0xFFFF`,
polynomial is `0x1021`, no reflection, and final XOR is `0x0000`.

`MODEL_RESULT` carries sequence in the frame header plus source timestamp,
three Q0.10000 probabilities, inference time, and status flags. It is valid
only after a real model returns `DMS_MODEL_OK`. `MODEL_UNAVAILABLE`, camera
status, and heartbeat are diagnostic messages and never imply a fatigue result.
