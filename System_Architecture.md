# MP6519 Brake Driver Test Jig - System Architecture

This document describes the overall system architecture, hardware connections, and data flow of the MP6519 Brake Driver Test Jig project.

## 1. System Overview
The system is designed to intelligently apply power to a 24V DC Brake Disk using a closed-loop control system. It features fault detection, precise power management (boost and hold phases), and live telemetry visualization via a Python desktop application.

```mermaid
graph LR
    subgraph PC["Host Computer (Windows)"]
        GUI["Python Dashboard (dashboard.py)"]
        Serial["USB COM Port (115200 baud)"]
        GUI <-->|JSON Telemetry| Serial
    end

    subgraph Debugger["RPI Debug Probe"]
        D_UART["UART Tx/Rx"]
        D_SWD["SWD Interface"]
        Serial <--> D_UART
    end

    subgraph Pico["Raspberry Pi Pico 2 (RP2350)"]
        Fw["MP6519_Control.ino Firmware"]
        UART0["UART0 (GP0, GP1)"]
        I2C1["I2C1 (GP14, GP15)"]
        PWM_OUT["PWM (GP10)"]
        ENB_OUT["ENB (GP12)"]
        BTN["Reset Button (GP16)"]
        
        UART0 <--> Fw
        Fw --> I2C1
        Fw --> PWM_OUT
        Fw --> ENB_OUT
        BTN --> Fw
    end

    subgraph Sensors["Power Sensing"]
        INA260["INA260EVM Power Monitor"]
        I2C1 <-->|I2C Bus| INA260
    end

    subgraph Driver["Motor Driver"]
        MP6519["EV6519-Q-00A (MP6519)"]
        PWM_OUT --> MP6519
        ENB_OUT --> MP6519
    end

    subgraph Load["Physical Load"]
        Brake["24V DC Brake Disk"]
        Supply["24V DC Power Supply"]
        MP6519 --> Brake
        Supply --> INA260
        INA260 --> MP6519
    end

    D_UART <--> UART0
```

## 2. Hardware Mapping
*   **RP2350 to INA260**: I2C1 (SCL=GP15, SDA=GP14) with external 10K pull-ups.
*   **RP2350 to MP6519**: EN=GP12, MODE=GP11, PWM=GP10, FT=GP13.
*   **RP2350 to Reset Button**: GP16 (Internal Pull-up, connect to GND to trigger).
*   **RP2350 to PC**: RPI Debugger connected to UART0 (GP0, GP1).

## 3. Data Flow
1.  **Sensing**: The INA260 continuously monitors the high-side 24V supply going into the MP6519 driver.
2.  **Control**: The Pico 2 firmware reads the INA260 at 10Hz. Based on the calculated wattage, it runs a closed-loop controller adjusting the PWM duty cycle on GP10.
3.  **Telemetry**: Every 100ms, the Pico 2 formats a JSON string containing V, I, W, PWM frequency, Duty Cycle, Fault State, and Phase State.
4.  **Visualization**: The Python application reads this JSON string over the Serial COM port, parses it, and dynamically updates the Tkinter UI dashboard.
