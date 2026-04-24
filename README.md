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

## Pin Mapping (Pico 2)

| Pin  | Function | Device        | Notes                      |
|------|----------|---------------|----------------------------|
| GP15 | SCL      | INA260        | External 10K Pull-up       |
| GP14 | SDA      | INA260        | External 10K Pull-up       |
| GP13 | FT       | MP6519        | Fault Trigger (10K Pull-up)|
| GP12 | EN       | MP6519        | Driver Enable              |
| GP11 | MODE9    | MP6519        | Mode Selection             |
| GP10 | PWM      | MP6519        | PWM Control (20kHz)        |

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
