# Software Requirements & Dependencies (Production v1.2.0)

This document outlines the software requirements and toolchain necessary to build, flash, and monitor the MP6519 3-Channel Brake Driver Control System.

## 1. Embedded Firmware (RP2040)

### 1.1 Core and Board Manager
*   **Target Board:** RP2040 (Standard Pico or custom board)
*   **Board Package:** `arduino-pico` by Earle F. Philhower, III (Version 3.x.x)
*   **Core Configuration:**
    - I2C Peripheral: `I2C1`
    - UART Peripheral: `UART0`

### 1.2 Library Requirements
*   **`Wire.h`**: Native I2C library.
*   **`HardwarePWM`**: (Optional) For high-frequency 20kHz PWM generation.

### 1.3 Compilation Parameters
- **PWM Frequency**: 20,000 Hz (20kHz)
- **I2C Clock**: 400,000 Hz (400kHz)
- **Serial Baud Rate**: 115,200 bps

## 2. Python Dashboard (Host PC)

### 2.1 Python Environment
*   **Version:** Python 3.8+
*   **Dependencies**:
    - `pyserial`: For COM port communication.
    - `tkinter`: For the graphical user interface.

### 2.2 Installation
```bash
pip install pyserial
```

## 3. Project Configuration Files
*   `hardware_pinout.json`: Authoritative source for physical pin mapping.
*   `FIRMWARE_LOGIC.json`: High-level state machine definition.
*   `dashboard.py`: 3-Channel telemetry visualization tool.
