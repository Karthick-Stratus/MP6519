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
*   **GP17** = Success Indicator (Pulse High on Peak Detection)
*   **GP18** = Failure Indicator (Trigger High on Fault)
*   **GP16** = Reset Test Sequence Button (Input Pull-up, Active Low)
*   **GP15** = SCL (INA260 - I2C1) - Must have 10K pull-up
*   **GP14** = SDA (INA260 - I2C1) - Must have 10K pull-up
*   **GP13** = FT (Fault Trigger) - Input with 10K pull-up
*   **GP12** = EN (Enable) - Output
*   **GP11** = MODE - Output
*   **GP10** = PWM - Output

---

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
8. **Reset**: Use the hardware button (GP16) to reset the system and wait 2 seconds before restarting.

