# Transmitter (TX) 🎙️

This module is responsible for capturing digital audio using the INMP441 I2S microphone and broadcasting it over the air using the ESP-NOW protocol.

## Core Features
* **I2S Audio Capture:** Configured as I2S Master using the ESP-IDF v5 Standard API, sampling 16-bit mono audio at 16 kHz.
* **ESP-NOW Broadcast:** Transmits audio chunks (240 bytes) to the broadcast MAC address (`FF:FF:FF:FF:FF:FF`).
* **Push-To-Talk (PTT):** Hardware polling logic ensures the ESP32-S3 only transmits packets when the PTT button is physically pressed, saving bandwidth and preventing hot-mic situations.

## Pinout Mapping (ESP32-S3)

| Component | ESP32-S3 Pin | Function |
| :--- | :--- | :--- |
| **INMP441 WS** | `GPIO 5` | Word Select (Left/Right Clock) |
| **INMP441 SD** | `GPIO 6` | Serial Data IN |
| **INMP441 SCK** | `GPIO 7` | Bit Clock |
| **PTT Button** | `GPIO 4` | Active LOW (Internal Pull-Up) |

## Build & Flash
Ensure you have set the ESP-IDF target to `esp32s3` and updated the flash memory configuration to 16MB via `menuconfig` if necessary:
```bash
idf.py build flash monitor
```