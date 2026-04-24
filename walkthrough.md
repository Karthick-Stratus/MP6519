# Project Walkthrough - MP6519 & INA260 on RP2350

This document outlines the implementation details for the Brake Disk control system using the Raspberry Pi Pico 2.

## 1. Code Logic: `MP6519_Control.ino`

The firmware implements a closed-loop power control system:

### Initialization
- **I2C**: Configured on GP14 (SDA) and GP15 (SCL) to communicate with the INA260 power monitor.
- **PWM**: Set to 20kHz on GP10 to drive the MP6519.
- **GPIOs**: GP12 (EN) and GP11 (MODE) are set to HIGH to enable the driver in its default operating mode. GP13 (FT) is monitored as a fault input.

### Power Stages
1. **Boost Phase (0-5s)**: The target power is set to **20 Watts**. The duty cycle is dynamically adjusted based on real-time voltage and current readings from the INA260.
2. **Hold Phase (>5s)**: The target power is reduced to **10 Watts**. The feedback loop continues to adjust the duty cycle to maintain this power level regardless of supply voltage fluctuations.

### Telemetry System
The telemetry is printed in a columnar format to the Serial port (115200 Baud):
```text
Voltage(V)  Current(A)  Power(W)  Duty(%)  Freq(Hz)  ENB  FT_Stat  MODE
24.01 V     0.833 A     20.00 W   100.0 %  20000 Hz  HIGH OK       HIGH
```

## 2. Hardware Connections

| Pico 2 Pin | Signal | Target Device | Notes |
|:---:|:---:|:---:|:---:|
| GP15 | SCL | INA260 | I2C Clock (10K Pull-up) |
| GP14 | SDA | INA260 | I2C Data (10K Pull-up) |
| GP13 | FT | MP6519 | Fault (Active LOW) |
| GP12 | EN | MP6519 | Enable |
| GP11 | MODE | MP6519 | Mode |
| GP10 | PWM | MP6519 | PWM Control |

## 3. Deployment
- The code has been committed and pushed to [Karthick-Stratus/MP6519](https://github.com/Karthick-Stratus/MP6519).
- A `.gitignore` has been included to keep the repository clean of build artifacts.
