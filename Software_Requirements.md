# Software Requirements & Dependencies

This document outlines the software requirements, libraries, and tools necessary to build and run the MP6519 Brake Driver Control System.

## 1. Embedded Firmware (Raspberry Pi Pico 2)

### 1.1 Core and Board Manager
*   **Target Board:** Raspberry Pi Pico 2 (RP2350)
*   **Board Package:** `arduino-pico` by Earle F. Philhower, III (Version 3.5.0 or newer)
*   **FQBN:** `rp2040:rp2040:rpipico2`

### 1.2 Arduino Libraries
*   **`Wire.h`**: Built-in Arduino I2C library (Using `Wire1` specifically for GP14/15 mapping). No external installation required; it is included in the core.

### 1.3 Toolchain Requirements
*   **Arduino CLI** or **Arduino IDE v2.x**
*   **Upload Method:** `picoprobe_cmsis_dap` (Required when flashing via the Raspberry Pi Debug Probe instead of standard UF2 USB drag-and-drop).

## 2. Python Dashboard (Host PC)

### 2.1 Python Environment
*   **Version:** Python 3.8+ (Tested on Python 3.9)
*   **OS Compatibility:** Windows, macOS, Linux

### 2.2 Python Dependencies
The dashboard relies on standard library modules (`tkinter`, `json`, `threading`) and one external dependency for serial communication.

**Required Package:**
*   `pyserial` (Version 3.5 or newer)

**Installation:**
```bash
pip install pyserial
```

## 3. Project Configuration Files
*   `.gitignore`: Prevents build artifacts (`*.elf`, `*.hex`, `*.uf2`, build folders) and IDE configurations (`.vscode/`, `.idea/`) from cluttering the repository.
*   `dashboard.py`: The executable UI script.
*   `MP6519_Control/MP6519_Control.ino`: The main Arduino sketch.
