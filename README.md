# 📻 EchoNow — ESP32-S3 Digital Walkie-Talkie System

**EchoNow** is a real-time, half-duplex two-way voice communication system built on the ESP32-S3 microcontroller. The project utilizes the proprietary **ESP-NOW** protocol for low-latency wireless transmission and the **I2S** interface for 100% digital audio capture and playback, eliminating analog noise.

---

## 🏗️ Modular Architecture (3 Phases)

To ensure a rigorous engineering methodology, avoid CMake build conflicts in ESP-IDF, and allow isolated hardware/software debugging, the project is divided into **three independent applications**:

### 🔹 Phase 1: `1_Transmitter` (TX)
* **Goal:** Voice signal acquisition and wireless transmission.
* **Details:**
  * Configures the I2S peripheral to read the INMP441 digital microphone.
  * Processes the input signal at **16 kHz / 16-bit Mono**.
  * Chunks the audio buffer into blocks compatible with the radio network.
  * Transmits the packets via **ESP-NOW**.

### 🔹 Phase 2: `2_Receiver` (RX)
* **Goal:** Radio data reception and clean audio playback.
* **Details:**
  * Continuously listens for and receives ESP-NOW packets sent by the TX module.
  * Manages ring/jitter buffers to prevent underruns and audio popping.
  * Configures the I2S bus to send the decoded samples to the MAX98357A amplifier.

### 🔹 Phase 3: `3_WalkieTalkie` (Half-Duplex Transceiver)
* **Goal:** Full system integration into a functional walkie-talkie device.
* **Details:**
  * Implements a state machine (TX / RX / IDLE).
  * Manages priority and state switching via a **Push-To-Talk (PTT)** button.
  * Intelligently disables audio capture/playback overlap to prevent acoustic feedback/echo.

---

## 🧰 Hardware Used

| Component | Function | Interface / Protocol |
| :--- | :--- | :--- |
| **ESP32-S3 (N16R8)** | Central processing and RF control | Wi-Fi / ESP-NOW |
| **INMP441** | Omnidirectional MEMS Microphone | I2S (Input) |
| **MAX98357A** | High-efficiency Class-D Amplifier | I2S (Output) |
| **Speaker** | 4Ω 3W Speaker | Analog Output (from Amplifier) |
| **Push-Button** | Physical transmission switch (PTT) | GPIO (Input with Pull-up) |

---

## 🔌 Wiring Diagram (Pinout — ESP32-S3)

Both the microphone and the amplifier share the same I2S clock lines (`WS` on `GPIO 5` and `SCK/BCLK` on `GPIO 7`) on `I2S_NUM_0`, while using dedicated data pins (`GPIO 6` and `GPIO 8`).

### 🎙️ 1. I2S MEMS Microphone — INMP441
| INMP441 Pin | ESP32-S3 Pin | Description |
| :--- | :--- | :--- |
| **VDD** | 3.3V | Power Supply (3.3V ONLY) |
| **GND** | GND | Ground |
| **SD** | `GPIO 6` | Serial Data IN (Microphone Audio Data) |
| **WS** | `GPIO 5` | Word Select / Left-Right Clock (Shared Clock) |
| **SCK** | `GPIO 7` | Bit Clock (Shared Clock) |
| **L/R** | GND | Channel Selection (GND = Left Channel) |

### 🔊 2. I2S Amplifier — MAX98357A
| MAX98357A Pin | ESP32-S3 Pin | Description |
| :--- | :--- | :--- |
| **VIN** | 5V / 3.3V | Power Supply (5V VBUS recommended) |
| **GND** | GND | Ground |
| **DIN** | `GPIO 8` | Serial Data OUT (Amplifier Audio Data) |
| **LRC** | `GPIO 5` | Left/Right Clock (Shared with INMP441 WS) |
| **BCLK** | `GPIO 7` | Bit Clock (Shared with INMP441 SCK) |
| **+/- Pins** | Speaker | Connected directly to the 4Ω 3W Speaker |

### 🔘 3. PTT Button (Push-To-Talk)
| Component | ESP32-S3 Pin | Configuration |
| :--- | :--- | :--- |
| **PTT Button** | `GPIO 4` | Internal Pull-Up (`LOW` = Transmit, `HIGH` = Listen) |

---

## ⚙️ Technical Parameters (Audio & ESP-NOW)

* **Sample Rate:** `16000 Hz` (16 kHz) — *Ideal balance between voice clarity and data rate limit.*
* **Resolution:** 16-bit Mono.
* **ESP-NOW Max Payload:** 250 bytes per packet.
* **Transmission Strategy:** Slicing I2S samples into blocks of 240 bytes (120 samples) to optimize the Wi-Fi stack payload.
* **Framework:** ESP-IDF v5 (New Standard I2S API, FreeRTOS Tasks & Queues).

---

## 🚀 How to Flash

1. Clone this repository to your local machine.
2. Open the desired phase directory (e.g., `1_Transmitter`, `2_Receiver`, or `3_WalkieTalkie`) in VS Code with the ESP-IDF extension installed.
3. Ensure the target is set to `esp32s3` and flash size is configured to `16MB` via `menuconfig`.
4. Connect your ESP32-S3 board via USB.
5. Build, flash, and open the Serial Monitor:
```bash
idf.py build flash monitor
---

> *Engineering a reliable wireless audio transmission, one packet at a time.*