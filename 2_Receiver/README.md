# Phase 2: Receiver (RX) 🔊

This module receives raw PCM audio via ESP-NOW broadcast and plays it back through the MAX98357A I2S Amplifier.

## Core Architecture
* **Non-blocking RX:** Uses an ESP-NOW callback that quickly copies incoming packets into a FreeRTOS Queue.
* **Dedicated Playback Task:** A high-priority FreeRTOS task drains the queue and writes directly to the I2S DMA buffers, preventing Wi-Fi stack starvation.
* **I2S Configuration:** Set as I2S Master in TX mode (16-bit Mono, 16 kHz).

## Pinout Mapping (ESP32-S3)

| Component | ESP32 Pin | Function |
| :--- | :--- | :--- |
| **MAX98357A LRC** | `GPIO 5` | Word Select (Left/Right Clock) |
| **MAX98357A BCLK**| `GPIO 7` | Bit Clock |
| **MAX98357A DIN** | `GPIO 8` | Serial Data OUT |

## Build & Flash
Ensure the target is set and flash memory configuration is updated to 16MB via `menuconfig` if necessary:
```bash
idf.py build flash monitor