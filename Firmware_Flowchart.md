# MP6519 3-Channel Firmware Flowchart

This document outlines the logical flow and multi-channel state machine for the RP2040 firmware.

```mermaid
flowchart TD
    Start([Power On / Boot]) --> Init["Initialize System"]
    Init --> Config["Load Pin Config & I2C Bus<br/>(400kHz, 20kHz PWM)"]
    Config --> Safety["Power Safety Check<br/>(LTC4367 Status)"]
    
    Safety -- OK --> MainLoop["Main Loop (100Hz)"]
    Safety -- Fault --> FaultState["Emergency Shutdown<br/>Brake Disable"]

    subgraph Monitoring["Continuous Monitoring"]
        InputCheck["Poll GPIO 26 & 27<br/>(Combined / Emergency)"]
        SensCheck["Read 3x INA260<br/>(0x40, 0x41, 0x45)"]
        HWFault["Monitor FT Pins<br/>(GPIO 8, 9, 10)"]
    end

    MainLoop --> Monitoring
    Monitoring --> StateSelect{"Input Triggered?"}

    StateSelect -- "G26 HIGH" --> TriggerB3["Start Channel 3 Sequence"]
    StateSelect -- "G27 HIGH" --> TriggerB12["Start Channel 1 & 2 Sequence"]
    StateSelect -- None --> Idle["Maintain IDLE"]

    subgraph Sequence["Brake Control Sequence"]
        S_Peak["PEAK PHASE:<br/>100% Duty for 3s<br/>Sample W_peak"]
        S_Hold["HOLD PHASE:<br/>Closed-Loop P-Control<br/>Maintain 0.15 * W_peak"]
    end

    TriggerB3 --> S_Peak
    TriggerB12 --> S_Peak
    
    S_Peak --> S_Hold
    S_Hold -- "Input Signal Change" --> S_Peak
    S_Hold -- "Input LOW" --> Idle
    
    HWFault -- "FT Pin LOW" --> FaultState
```

## 1. Logic Sequence (Per Channel)
1.  **Idle State**: No PWM output. `ENABLE` pin is `LOW`.
2.  **Peak Phase**: 
    - Triggered by respective input.
    - `PWM` set to 100%.
    - `ENABLE` set to `HIGH`.
    - Samples power for 3 seconds.
3.  **Hold Phase**:
    - Calculates target wattage (15% of Peak).
    - Runs a high-frequency control loop (100Hz) adjusting PWM to match target.
4.  **Shutdown**:
    - Triggered if input goes `LOW`.
    - PWM and Enable are immediately disabled.

## 2. Safety Integrations
- **Watchdog**: Hardware watchdog ensures the system restarts if the loop hangs.
- **LTC4367**: Hardware-level protection for over/under voltage.
- **Fault (FT)**: Immediate hardware interrupt if the MP6519 detects a driver-level fault.
