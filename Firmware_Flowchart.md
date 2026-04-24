# MP6519 Brake Driver Firmware Flowchart

This document outlines the state machine and logical flow of the RP2350 firmware controlling the MP6519 and reading the INA260 telemetry.

```mermaid
flowchart TD
    Start([Power On / Boot]) --> Setup[System Setup]
    Setup --> Wait3s[Wait 3 seconds\n(Allow Serial Monitor connection)]
    Wait3s --> InitPins[Initialize Pins & I2C1]
    InitPins --> HardwareOff[Ensure ENB=LOW, PWM=0]
    HardwareOff --> StartSeq[Start Sequence]
    
    StartSeq --> Seq1[Apply 100% PWM Duty Cycle]
    Seq1 --> Seq2[Wait 2ms for PWM stabilization]
    Seq2 --> Seq3[Set ENB=HIGH to Enable Driver]
    Seq3 --> SetStateBoost[State = BOOST]
    SetStateBoost --> Loop[Main Loop 10Hz]

    Loop --> CheckReset{Reset Button\nPressed?}
    CheckReset -- Yes --> DoReset[Disable ENB & PWM\nClear Variables\nSend 0 JSON]
    DoReset --> Wait2s[Wait 2 Seconds]
    Wait2s --> WaitRelease[Wait for Button Release]
    WaitRelease --> StartSeq
    
    CheckReset -- No --> ReadSensors[Read Voltage, Current\nCalculate Power]
    ReadSensors --> CheckState{Current State}
    
    CheckState -- BOOST --> BoostLogic[Force 100% PWM\nTrack Max Power]
    BoostLogic --> BoostShort{Power > 60W?}
    BoostShort -- Yes --> TriggerShort[Trigger SHORT_CIRCUIT Fault]
    TriggerShort --> FaultState
    BoostShort -- No --> BoostTime{Elapsed >= 3s?}
    BoostTime -- No --> ApplyPWM
    BoostTime -- Yes --> BoostOpen{Max Power < 0.5W?}
    BoostOpen -- Yes --> TriggerOpen[Trigger OPEN_CIRCUIT Fault]
    TriggerOpen --> FaultState
    BoostOpen -- No --> SetRamp[Calculate Target = MaxW * 10%\nState = RAMP_DOWN]
    SetRamp --> ApplyPWM
    
    CheckState -- RAMP_DOWN --> RampLogic[Smoothly decrease PWM towards Target]
    RampLogic --> RampTime{Elapsed >= 5s\nin Ramp?}
    RampTime -- No --> ApplyPWM
    RampTime -- Yes --> SetHold[State = HOLD]
    SetHold --> ApplyPWM
    
    CheckState -- HOLD --> HoldLogic[Closed Loop Control:\nIf Power > Target, decrease PWM\nIf Power < Target, increase PWM]
    HoldLogic --> ApplyPWM
    
    CheckState -- FAULT --> FaultState[Set PWM=0, ENB=LOW]
    FaultState --> ApplyPWM
    
    ApplyPWM[Constrain & Apply Duty Cycle to PIN 10] --> SendJSON[Print JSON Telemetry via Serial1]
    SendJSON --> Delay[Delay 100ms]
    Delay --> Loop
```
