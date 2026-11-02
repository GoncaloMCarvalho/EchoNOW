# Phase 3: Walkie-Talkie (Half-Duplex) 📻

This is the final, fully-integrated module. It combines both the INMP441 microphone and the MAX98357A amplifier into a single ESP32, creating a half-duplex walkie-talkie node.

## Core Features
* **Shared I2S Bus:** Intelligently shares the `WS` and `SCK` clock lines between the microphone and amplifier, utilizing a single I2S peripheral (`I2S_NUM_0`) with separate Data IN and Data OUT pins.
* **Half-Duplex Logic:** Governed by the Push-To-Talk (PTT) button:
  * **PRESSED:** Captures audio and broadcasts via ESP-NOW. Ignores incoming radio packets to prevent feedback loops.
  * **RELEASED:** Halts transmission and routes incoming ESP-NOW packets to the amplifier.
* **Dual-Core Processing:** TX task is pinned to Core 1, and RX task is pinned to Core 0 for optimal RTOS load balancing.

## Pinout Mapping (ESP32-S3)

| Component | ESP32 Pin | Function |
| :--- | :--- | :--- |
| **WS / LRC** | `GPIO 5` | Shared Clock (Connect to BOTH Mic and Amp) |
| **SCK / BCLK**| `GPIO 7` | Shared Clock (Connect to BOTH Mic and Amp) |
| **INMP441 SD** | `GPIO 6` | Serial Data IN (Microphone) |
| **MAX DIN** | `GPIO 8` | Serial Data OUT (Amplifier) |
| **PTT Button**| `GPIO 4` | Active LOW (Internal Pull-Up) |

## Build & Flash
Remember to set the target and update flash size to 16MB via `menuconfig`:
```bash
idf.py build flash monitor
```