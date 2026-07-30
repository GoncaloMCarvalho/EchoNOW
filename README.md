# 📻 EchoNow — ESP32 Digital Walkie-Talkie System

**EchoNow** is a real-time, half-duplex two-way voice communication system built on the ESP32 microcontroller. The project utilizes the proprietary **ESP-NOW** protocol for low-latency wireless transmission and the **I2S** interface for 100% digital audio capture and playback, eliminating analog noise.

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
  * Intelligently disables audio capture during playback to prevent acoustic feedback/echo.

---

## 🧰 Hardware Used

| Component | Function | Interface / Protocol |
| :--- | :--- | :--- |
| **ESP32** | Central processing and RF control | Wi-Fi / ESP-NOW |
| **INMP441** | Omnidirectional MEMS Microphone | I2S (Input) |
| **MAX98357A** | High-efficiency Class-D Amplifier | I2S (Output) |
| **Speaker** | 4Ω 3W Speaker | Analog Output (from Amplifier) |
| **Push-Button** | Physical transmission switch (PTT) | GPIO (Input with Pull-up) |

---

## 🔌 Wiring Diagram (Pinout)

### 🎙️ 1. I2S MEMS Microphone — INMP441
| INMP441 Pin | ESP32 Pin | Description |
| :--- | :--- | :--- |
| **VDD** | 3.3V | Power Supply |
| **GND** | GND | Ground |
| **SD** | `GPIO 32` | Serial Data (Audio Data) |
| **WS** | `GPIO 25` | Word Select (Channel Clock) |
| **SCK** | `GPIO 33` | Bit Clock |
| **L/R** | GND | Channel Selection (GND = Left Channel) |

### 🔊 2. I2S Amplifier — MAX98357A
| MAX98357A Pin | ESP32 Pin | Description |
| :--- | :--- | :--- |
| **VIN** | 5V / 3.3V | Power Supply |
| **GND** | GND | Ground |
| **DIN** | `GPIO 22` | Data In (Audio Data) |
| **LRC** | `GPIO 25` | Left/Right Clock (WS) |
| **BCLK** | `GPIO 26` | Bit Clock |
| **+/- Pins** | Speaker | Connected directly to the 4Ω 3W Speaker |

### 🔘 3. PTT Button (Push-To-Talk)
| Component | ESP32 Pin | Configuration |
| :--- | :--- | :--- |
| **PTT Button** | `GPIO 4` | Internal Pull-Up (`LOW` = Transmit, `HIGH` = Listen) |

---

## ⚙️ Technical Parameters (Audio & ESP-NOW)

* **Sample Rate:** `16000 Hz` (16 kHz) — *Ideal balance between voice clarity and data rate limit.*
* **Resolution:** 16-bit Mono.
* **ESP-NOW Max Payload:** 250 bytes per packet.
* **Transmission Strategy:** Slicing I2S samples into blocks of ~200-240 bytes to optimize the Wi-Fi stack payload.
* **Framework:** ESP-IDF (FreeRTOS Tasks, Queues & Direct-to-Task Notifications).

---

## 🚀 How to Flash

1. Clone this repository to your local machine.
2. Open the desired phase directory (e.g., `01_Transmitter`) in VS Code with the ESP-IDF extension installed.
3. Connect your ESP32 board via USB.
4. Build and upload the firmware.
5. Open the Serial Monitor to verify the ESP-NOW communication and audio processing.

---

> *Engineering a reliable wireless audio transmission, one packet at a time.*