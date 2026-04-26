# Project Walkthrough - 3-Channel MP6519 Control (Production)

This document provides a guided walkthrough of the system's operation and code structure.

## 1. Core Logic: `MP6519_Control.ino`
The firmware is built around an **Event-Driven State Machine**. Each of the three channels operates independently.

### 1.1 Triggering Mechanism
- The system monitors `GPIO 26` (Combined) and `GPIO 27` (Emergency).
- When a trigger goes `HIGH`, the respective channels enter the `PEAK` state.
- If the trigger goes `LOW` at any point, the channels immediately return to `IDLE` (Power Off).

### 1.2 Peak & Hold Sequence
- **PEAK**: The driver applies full power (100% duty) for 3 seconds. The firmware samples the `INA260` sensor to determine the maximum wattage the brake disk can pull.
- **HOLD**: The system automatically calculates 15% of that peak wattage and maintains it. A **Proportional Control Loop** runs at 100Hz to adjust the PWM duty cycle, compensating for voltage drops or temperature changes.

## 2. Hardware Protection
The system includes multi-layer protection:
- **LTC4367**: Hardware-level monitoring for UV/OV/RV.
- **FT (Fault)**: The `MP6519` FT pin is monitored; if a driver fault occurs, the channel enters a `FAULT` state and shuts down.
- **Software Watchdog**: Prevents firmware hangs.

## 3. Telemetry Dashboard
The `dashboard.py` script provides a 3-channel visualization:
- Real-time V, I, and W for each brake.
- Visual state indicators (IDLE, PEAK, HOLD, FAULT).
- System-wide power protection status.
