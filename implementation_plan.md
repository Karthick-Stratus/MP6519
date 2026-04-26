# Implementation Plan - MP6519 3-Channel Production (RP2040)

This document records the finalized implementation strategy for the production-grade 3-channel brake controller.

## 1. Hardware Architecture
- **MCU**: RP2040.
- **Drivers**: 3x MP6519GQ-Z.
- **Sensors**: 3x INA260AIPWR (I2C Addresses: 0x40, 0x41, 0x45).
- **Protection**: Dual LTC4367 Protection Controllers.
- **Logic Level**: 3.3V LVCMOS.

## 2. Pin Mapping (RP2040)
| Function | Brake 1 | Brake 2 | Brake 3 |
| :--- | :--- | :--- | :--- |
| **ENABLE** | GPIO 5 | GPIO 6 | GPIO 7 |
| **PWM** | GPIO 16 | GPIO 18 | GPIO 20 |
| **FAULT (FT)** | GPIO 8 | GPIO 9 | GPIO 10 |
| **MODE** | GPIO 11 | GPIO 12 | GPIO 13 |
| **ALERT (INA)** | GPIO 14 | GPIO 24 | GPIO 25 |

## 3. Firmware Logic (v1.2.0)
- **Closed-Loop Control**: 100Hz frequency.
- **Phases**:
    1. **IDLE**: Waiting for trigger.
    2. **PEAK**: 100% duty for 3 seconds; record max power.
    3. **HOLD**: Maintain 15% of recorded max power via P-control loop.
- **Triggers**:
    - `GPIO 26`: Triggers CH3.
    - `GPIO 27`: Triggers CH1 & CH2.

## 4. Host Interface
- **Protocol**: JSON Telemetry @ 10Hz.
- **GUI**: Python Dashboard supporting 3 independent channel views and protection status.
