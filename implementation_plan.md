# Implementation Plan - MP6519 & INA250EVM Control (RP2350)

This project involves controlling a Brake Disk using the MP6519 motor driver and monitoring power consumption via the INA250EVM (I2C) using a Raspberry Pi Pico 2 (RP2350).

## 1. Hardware Configuration

### Pico 2 Pin Mapping
| Pin | Function | Device | Notes |
|---|---|---|---|
| GP15 | SCL | INA250EVM | I2C Clock (External 10K Pull-up) |
| GP14 | SDA | INA250EVM | I2C Data (External 10K Pull-up) |
| GP13 | FT | MP6519 | Fault Trigger (Active Low, External 10K Pull-up) |
| GP12 | EN | MP6519 | Enable Pin |
| GP11 | MODE | MP6519 | Mode Selection (High/Low) |
| GP10 | PWM | MP6519 | PWM Control |

### Power Specifications
- **Source**: 24VDC
- **Initial Phase**: 20W (for 5 seconds)
- **Hold Phase**: 10W (after 5 seconds)

## 2. Software Architecture

### Core Components
- **I2C Interface**: Initialize I2C on GP14/GP15 to communicate with the power monitor.
- **PWM Controller**: Manage PWM frequency and duty cycle on GP10 to achieve target power.
- **Control Logic**: A state machine or timer-based logic to handle the transition from 20W to 10W.
- **Telemetry System**: Formatted Serial output for real-time monitoring.

### Telemetry Columns
- Voltage (V)
- Current (A)
- Duty Cycle (%)
- PWM Frequency (Hz)
- ENB Status (High/Low)
- FT Status (Fault/OK)
- MODE Level (High/Low)
- Power (W)

## 3. Implementation Steps

1. **Project Initialization**: Create the basic Arduino sketch structure and README.
2. **I2C & Sensor Setup**: Configure I2C1 (since GP14/15 are typically I2C1) and verify communication with the sensor.
3. **PWM Configuration**: Set up PWM on GP10 with a default frequency (e.g., 20kHz).
4. **Power Control Logic**:
   - Calculate required duty cycle for 20W and 10W based on voltage readings.
   - Use `millis()` to track the 5-second transition.
5. **Telemetry Formatting**: Implement the column-based Serial print.
6. **Testing & Calibration**: Verify readings and power output.

## 4. Git Integration
- Update `README.md` with setup instructions.
- Upload the source code to the repository.
