# MP6519 Brake Driver Firmware Flowchart

This document outlines the state machine and logical flow of the RP2350 firmware controlling the MP6519 and reading the INA260 telemetry.

```mermaid
flowchart TD
    Start([Power On / Boot]) --> Setup[System Setup]
    Setup --> Wait3s[Wait 3 seconds<br/>Allow Serial Monitor]
    Wait3s --> InitPins[Initialize Pins & I2C1]
    InitPins --> InitINA[initINA260<br/>Set 18V Under-Voltage Alert]
    InitINA --> HardwareOff[Ensure ENB=LOW, PWM=0]
    HardwareOff --> WaitPower[State = WAIT_POWER]
    
    WaitPower --> CheckAlert{INA260 Alert Pin<br/>(GP19) HIGH?}
    CheckAlert -- No --> WaitPower
    CheckAlert -- Yes --> StartSeq[Start Sequence]
    
    StartSeq --> Seq1[Apply 100% PWM Duty Cycle]
    Seq1 --> Seq2[Wait 2ms for PWM stabilization]
    Seq2 --> Seq3[Set ENB=HIGH to Enable Driver]
    Seq3 --> SetStatePeak[State = PEAK]
    SetStatePeak --> Loop[Main Loop 10Hz]

    Loop --> CheckReset{Reset Button<br/>Pressed?}
    CheckReset -- Yes --> DoReset[Disable ENB & PWM<br/>Clear Variables<br/>Send 0 JSON]
    DoReset --> Wait2s[Wait 2 Seconds]
    Wait2s --> WaitRelease[Wait for Button Release]
    WaitRelease --> StartSeq
    
    CheckReset -- No --> ReadSensors[Read Voltage, Current<br/>Calculate Power<br/>Read FT & Alert Pins]
    
    ReadSensors --> CheckAlert2{V < 18V?<br/>(Alert LOW)}
    CheckAlert2 -- Yes --> DoPowerLoss[Disable Output<br/>State = WAIT_POWER]
    DoPowerLoss --> Loop
    
    CheckAlert2 -- No --> CheckFT{FT Pin<br/>(GP13) LOW?}
    CheckFT -- Yes --> FT_Time{Elapsed > 100ms?}
    FT_Time -- Yes --> TriggerFT[Trigger DRIVER_HARDWARE Fault]
    TriggerFT --> FaultState
    FT_Time -- No --> CheckState
    CheckFT -- No --> CheckState{Current State}
    
    CheckState -- PEAK --> PeakLogic[Force 100% PWM<br/>Track Max Power]
    PeakLogic --> PeakShort{Power > 60W?}
    PeakShort -- Yes --> TriggerShort[Trigger SHORT_CIRCUIT Fault]
    TriggerShort --> FaultState
    PeakShort -- No --> PeakTime{Elapsed >= 3s?}
    PeakTime -- No --> ApplyPWM
    PeakTime -- Yes --> PeakOpen{Max Power < 0.5W?}
    PeakOpen -- Yes --> TriggerOpen[Trigger OPEN_CIRCUIT Fault]
    TriggerOpen --> FaultState
    PeakOpen -- No --> SetRamp[Calculate Target = MaxW * 15%<br/>State = RAMP_DOWN]
    SetRamp --> ApplyPWM
    
    CheckState -- RAMP_DOWN --> RampLogic[Smoothly decrease PWM towards Target]
    RampLogic --> RampTime{Elapsed >= 5s<br/>in Ramp?}
    RampTime -- No --> ApplyPWM
    RampTime -- Yes --> SetHold[State = HOLD]
    SetHold --> ApplyPWM
    
    CheckState -- HOLD --> HoldLogic[Maintain Target Power]
    HoldLogic --> LongRunCheck{Long Run<br/>Enabled?}
    LongRunCheck -- Yes --> HoldTime{Hold >= 3s?}
    HoldTime -- Yes --> SetCD[State = COOLDOWN]
    HoldTime -- No --> ApplyPWM
    LongRunCheck -- No --> ApplyPWM
    
    CheckState -- COOLDOWN --> CDLogic[Disable Output<br/>Wait 1 Second]
    CDLogic --> CDTime{Elapsed >= 1s?}
    CDTime -- Yes --> StartSeq
    CDTime -- No --> ApplyPWM

    CheckState -- FAULT --> FaultState[Set PWM=0, ENB=LOW]
    FaultState --> ApplyPWM
    
    ApplyPWM[Constrain & Apply PWM to PIN 10] --> SendJSON[Print JSON Telemetry via Serial1]
    SendJSON --> Delay[Delay 100ms]
    Delay --> Loop
```
