# MP6519 Brake Control System (RP2350)

This repository contains the firmware for controlling a Brake Disk using the **MP6519** motor driver and monitoring power telemetry via an **INA260** sensor on the **Raspberry Pi Pico 2 (RP2350)**.

## Project Specifications

- **Processor**: RP2350 (Pico 2 EVL Board)
- **Motor Driver**: MP6519 (EV6519-Q-00A)
- **Power Sensor**: INA260 (I2C)
- **Operating Voltage**: 24VDC
- **Brake Logic**:
  - Initial Phase: 20 Watts (5 Seconds)
  - Hold Phase: 10 Watts (Steady State)

## Pin Configuration (Pico 2)
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

The system prints telemetry side-by-side in columns via the RPI Debugger Serial (COM12):

`Voltage(V) | Current(A) | Power(W) | Duty(%) | Freq(Hz) | ENB | FT_Stat | MODE`

## Setup Instructions

1. Connect the hardware according to the pin mapping table.
2. Power the system with 24VDC.
3. Upload the `MP6519_Control.ino` sketch using the Arduino IDE (ensure the Raspberry Pi Pico 2 board is selected).
4. Open the Serial Monitor at **115200 Baud** to view the telemetry.

## Operation

Upon startup, the system will:
1. Initialize the INA260 sensor and PWM output.
2. Ramp up the Brake Disk to **20 Watts** and maintain it for 5 seconds.
3. Automatically drop the power to **10 Watts** after the boost period.
4. Continuously monitor for faults (FT pin).
