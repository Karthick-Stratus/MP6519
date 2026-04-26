# MP6519 Brake Control System (RP2350)

This repository contains the firmware for controlling a Brake Disk using the **MP6519** motor driver and monitoring power telemetry via an **INA260** sensor on the **Raspberry Pi Pico 2 (RP2350)**.

## Project Specifications

- **Processor**: RP2350 (Pico 2 EVL Board)
- **Motor Driver**: MP6519 (EV6519-Q-00A)
- **Power Sensor**: INA260 (I2C)
- **Operating Voltage**: 24VDC
- **Brake Logic**:
  - **PEAK Phase**: 100% Duty Cycle for 3 Seconds.
  - **Hold Phase**: Automatically tuned to **15%** of the measured Peak Power.
  - **Long Run Mode**: Optional continuous testing loop with 1s cooldown between cycles.

## Pin Configuration (Pico 2)

Detailed hardware pin mapping is available in the [hardware_pinout.json](hardware_pinout.json) file for AI-assisted programming and automated integration.

| Signal | Pin (GP) | Mode | Description |
| :--- | :--- | :--- | :--- |
| **TX** | GP0 | UART | Serial1 TX (Telemetry Out) |
| **RX** | GP1 | UART | Serial1 RX (Commands In) |
| **STATUS_LED** | GP9 | OUTPUT | System Status Indicator |
| **PWM** | GP10 | OUTPUT | MP6519 PWM Control |
| **MODE** | GP11 | OUTPUT | MP6519 Mode Control |
| **EN** | GP12 | OUTPUT | MP6519 Enable Control |
| **FT** | GP13 | INPUT | MP6519 Fault Trigger (10K Pull-up) |
| **SDA** | GP14 | I2C | INA260 I2C Data (10K Pull-up) |
| **SCL** | GP15 | I2C | INA260 I2C Clock (10K Pull-up) |
| **RESET_BTN** | GP16 | INPUT | Test Sequence Reset (Active Low) |
| **SUCCESS** | GP17 | OUTPUT | Peak Detection Success Pulse |
| **FAILURE** | GP18 | OUTPUT | Fault/Failure Trigger |
| **ALERT** | GP19 | INPUT | INA260 Alert (18V UVP) |


---

## ⚡ Power-On Reset (POR) & 18V Monitoring

The system uses the **INA260 ALERT pin (GP19)** to monitor the 24V supply. 
- **Under-Voltage Protection**: If the voltage drops below **18V**, the system immediately disables the motor driver and enters a `WAIT_POWER` state to prevent unstable operation or hardware damage.
- **Auto-Recovery**: When the 24V supply is restored (>18V), the system automatically re-initializes the I2C bus and sensor registers, then restarts the full test sequence from the beginning.
- **Robustness**: The firmware includes an I2C recovery mechanism and 100ms startup blanking to handle rapid power cycles (OFF-ON within 1s) and deep brownouts (down to 0V) without hanging.

## ⚠️ CRITICAL: MP6519 ENB and PWM Sequence

The MP6519 motor driver has a strict safety feature called **Switching Auto-Disable**. 
If the PWM signal remains `LOW` while the `EN` (Enable) signal is driven `HIGH` during start-up, the device permanently enters a **Standby Mode** and will not output power.

To successfully power on the brake, the firmware strictly follows this sequence:
1.  **Initialize EN as LOW** (`digitalWrite(PIN_EN, LOW)`).
2.  **Configure and start the PWM output** (Apply `analogWrite` with a `>0` duty cycle).
3.  **Wait briefly** to ensure the PWM signal is stable (e.g. `delay(2)`).
4.  **Drive EN to HIGH** (`digitalWrite(PIN_EN, HIGH)`) to enable the driver.

## Telemetry Format

The system outputs JSON telemetry at **10Hz** via Serial1 (TX=GP0, RX=GP1). This data is consumed by the Python Dashboard.

## Dashboard GUI

A Python-based dashboard (`dashboard.py`) is provided to visualize telemetry and control the **Long Run Test**.

**Requirements:**
- Python 3.x
- `pip install pyserial`

**Run:**
```bash
python dashboard.py
```

## Setup & Operation

1. Connect hardware as per pin configuration.
2. Upload `MP6519_Control.ino` to the Pico 2.
3. Launch `dashboard.py` and select the correct COM port.
4. **PEAK Detection**: The system will run at 100% duty for 3s to detect max power.
5. **Ramp Down**: Smoothly transitions to 15% of the Peak power over 5s.
6. **Hold**: Maintains 15% power indefinitely (or for 3s in Long Run Mode).
7. **Long Run**: Toggle this in the GUI to repeat the test cycle automatically.
8. **Reset**: Use the hardware button (GP16) or wait for a Power Recycle (>18V) to restart the system.

