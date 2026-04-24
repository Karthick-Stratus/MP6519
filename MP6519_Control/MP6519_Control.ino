#include <Wire.h>

/**
 * Project: MP6519 Brake Disk Control with Power Monitoring
 * Hardware: Raspberry Pi Pico 2 (RP2350), MP6519 Motor Driver, INA260 Power Monitor
 * 
 * Pin Configuration:
 * GP15 = SCL (INA260)
 * GP14 = SDA (INA260)
 * GP13 = FT (Fault Trigger) - Input with Pull-up
 * GP12 = EN (Enable) - Output
 * GP11 = MODE - Output
 * GP10 = PWM - Output
 */

// Pin Definitions
const int PIN_SCL = 15;
const int PIN_SDA = 14;
const int PIN_FT = 13;
const int PIN_EN = 12;
const int PIN_MODE = 11;
const int PIN_PWM = 10;
const int PIN_RESET_BTN = 16; // Push button to restart
const int PIN_SUCCESS = 17;   // Pulse on successful detection
const int PIN_FAILURE = 18;   // Trigger on fault
const int PIN_ALERT = 19;     // INA260 Alert Pin (Active Low on Under-Voltage)
const int PIN_STATUS_LED = 9;        // Status LED

// INA260 Configuration
#define INA260_ADDR 0x40
#define REG_CONFIG 0x00
#define REG_CURRENT 0x01
#define REG_VOLTAGE 0x02
#define REG_POWER 0x03
#define REG_MASK_ENABLE 0x06
#define REG_ALERT_LIMIT 0x07

// Power Control Configuration
const unsigned long BOOST_TIME_MS = 3000; // 3 seconds at 100%
const unsigned long RAMP_TIME_MS = 5000;  // 5 seconds to ramp down
float measuredBoostPower = 0.0;
float targetHoldPower = 0.0;

enum SystemState {
  STATE_WAIT_POWER,
  STATE_PEAK,
  STATE_RAMP_DOWN,
  STATE_HOLD,
  STATE_COOLDOWN,
  STATE_FAULT
};
SystemState currentState = STATE_WAIT_POWER;
bool isLongRunEnabled = false;
int cycleCount = 0;
unsigned long holdStartTime = 0;
unsigned long cooldownStartTime = 0;
unsigned long ledPrevTime = 0;
int ledBrightness = 0;
bool ledDirection = true;

enum FaultState {
  FAULT_NONE,
  FAULT_SHORT_CIRCUIT,
  FAULT_OPEN_CIRCUIT,
  FAULT_HARDWARE // New: Triggered by FT pin
};
FaultState currentFault = FAULT_NONE;

// PWM Settings
const int PWM_FREQUENCY = 20000; // 20kHz
const int PWM_RESOLUTION = 1024; // 10-bit range
int dutyCycle = PWM_RESOLUTION;  // Start at 100%

unsigned long startTime = 0;
unsigned long rampStartTime = 0;

// Function Prototypes
uint16_t readRegister(uint8_t reg);
void writeRegister(uint8_t reg, uint16_t value);
float getVoltage();
float getCurrent();
void printJsonTelemetry(float v, float i, float p);
void resetSystem();
void initINA260();
void startSequence();

void setup() {
  Serial1.setTX(0);
  Serial1.setRX(1);
  Serial1.begin(115200);
  
  // Wait 3 seconds to allow time to open Serial Monitor after boot
  delay(3000); 
  Serial1.println("{\"log\": \"=== BOOTING MP6519 SYSTEM ===\"}");

  // Initialize Pins
  pinMode(PIN_EN, OUTPUT);
  pinMode(PIN_MODE, OUTPUT);
  pinMode(PIN_PWM, OUTPUT);
  pinMode(PIN_FT, INPUT_PULLUP);
  pinMode(PIN_RESET_BTN, INPUT_PULLUP); // Push button active LOW
  pinMode(PIN_SUCCESS, OUTPUT);
  pinMode(PIN_FAILURE, OUTPUT);
  digitalWrite(PIN_SUCCESS, LOW);
  digitalWrite(PIN_FAILURE, LOW);

  // Hardware initially OFF
  digitalWrite(PIN_EN, LOW);    
  digitalWrite(PIN_MODE, HIGH); 
  dutyCycle = 0;
  analogWrite(PIN_PWM, 0);

  // Setup I2C on GP14/15 (Wire1)
  Wire1.setSDA(PIN_SDA);
  Wire1.setSCL(PIN_SCL);
  Wire1.begin();

  initINA260();

  // Configure PWM for Driver and LED
  analogWriteFreq(PWM_FREQUENCY);
  analogWriteRange(PWM_RESOLUTION);

  pinMode(PIN_ALERT, INPUT_PULLUP);
  pinMode(PIN_STATUS_LED, OUTPUT);

  startSequence(); 
}

void initINA260() {
  // 1. Re-initialize I2C bus in case it was hung during brownout
  Wire1.end();
  delay(10);
  Wire1.setSDA(PIN_SDA);
  Wire1.setSCL(PIN_SCL);
  Wire1.begin();
  delay(10);
  
  // 2. Configure INA260 Alert for Under-Voltage < 18V
  // 18V / 1.25mV/LSB = 14400 (0x3840)
  writeRegister(REG_ALERT_LIMIT, 0x3840);
  // Enable Bus Under-Voltage Limit (Bit 12) in Transparent Mode (Active Low)
  writeRegister(REG_MASK_ENABLE, 0x1000);
  delay(10); // Small delay for registers to stabilize
}

void startSequence() {
  currentState = STATE_PEAK;
  currentFault = FAULT_NONE;
  measuredBoostPower = 0.0;
  targetHoldPower = 0.0;
  digitalWrite(PIN_SUCCESS, LOW);
  digitalWrite(PIN_FAILURE, LOW);
  
  // IMPORTANT: ENB and PWM Sequence for MP6519
  // 1. Apply initial 100% PWM before enabling EN
  dutyCycle = PWM_RESOLUTION; 
  analogWrite(PIN_PWM, dutyCycle);
  delay(2); // Wait 2ms for PWM to stabilize
  
  // 2. Enable the driver
  digitalWrite(PIN_EN, HIGH);   
  
  startTime = millis();
}

void loop() {
  // Check Hardware Reset Button
  if (digitalRead(PIN_RESET_BTN) == LOW) {
    // 1. Disable ENB Immediately to cut power to brake
    digitalWrite(PIN_EN, LOW);
    dutyCycle = 0;
    analogWrite(PIN_PWM, 0);

    // 2. Clear UI variables
    currentState = STATE_PEAK;
    currentFault = FAULT_NONE;
    measuredBoostPower = 0.0;
    targetHoldPower = 0.0;
    cycleCount = 0;
    digitalWrite(PIN_SUCCESS, LOW);
    digitalWrite(PIN_FAILURE, LOW);
    
    // Send zeroed JSON to update UI immediately
    printJsonTelemetry(0.0, 0.0, 0.0);
    Serial1.println("{\"log\": \"SYSTEM RESET! Waiting 2 seconds...\"}");
    
    // 3. Wait 2 seconds before starting code
    delay(2000); 
    
    // Wait until button is released before continuing
    while(digitalRead(PIN_RESET_BTN) == LOW) { delay(10); }
    
    startSequence();
    return;
  }

  // Handle Serial Commands (JSON)
  if (Serial1.available()) {
    String input = Serial1.readStringUntil('\n');
    if (input.length() > 0) {
      if (input.indexOf("\"cmd\":\"LONG_RUN\"") != -1) {
        isLongRunEnabled = (input.indexOf("\"enable\":true") != -1);
      }
    }
  }

  unsigned long elapsed = millis() - startTime;
  
  // Read Telemetry
  float voltage = getVoltage();
  float current = getCurrent();
  float power = voltage * current;
  int alert_stat = digitalRead(PIN_ALERT);

  // --- POWER MONITORING (INA260 ALERT) ---
  // If ALERT goes LOW, it means voltage is < 18V (Power disconnected or sagging heavily)
  if (alert_stat == LOW) {
    if (currentState != STATE_WAIT_POWER) {
      // Immediately shut down
      digitalWrite(PIN_EN, LOW);
      dutyCycle = 0;
      analogWrite(PIN_PWM, 0);
      currentState = STATE_WAIT_POWER;
      Serial1.println("{\"log\": \"POWER LOSS DETECTED (<18V)! Halting operations.\"}");
    }
  } else if (currentState == STATE_WAIT_POWER && alert_stat == HIGH) {
    // Power restored. Wait for stabilization
    delay(200); // Increased to 200ms for deep brownout recovery
    if (digitalRead(PIN_ALERT) == HIGH) {
      Serial1.println("{\"log\": \"POWER RESTORED (>18V)! Re-initializing I2C and starting sequence.\"}");
      initINA260(); 
      startSequence();
    }
  }

  // Monitor Hardware Fault Pin (FT goes LOW on OCP, OTP, etc.)
  // Wait 100ms after startup before checking FT to allow MP6519 to clear any startup transients
  if (digitalRead(PIN_FT) == LOW && currentState != STATE_FAULT && currentState != STATE_WAIT_POWER) {
    if (elapsed > 100) {
      currentFault = FAULT_HARDWARE;
      currentState = STATE_FAULT;
    }
  }

  if (currentState == STATE_WAIT_POWER) {
    // Do nothing, waiting for power
  }
  else if (currentState == STATE_PEAK) {
    dutyCycle = PWM_RESOLUTION; // Force 100%
    
    if (power > measuredBoostPower) {
      measuredBoostPower = power;
    }
    
    // Check for Short Circuit
    if (power > 60.0) {
      currentFault = FAULT_SHORT_CIRCUIT;
      currentState = STATE_FAULT;
    }
    
    // Check phase completion
    if (elapsed >= BOOST_TIME_MS && currentState != STATE_FAULT) {
      if (measuredBoostPower < 0.5) {
        currentFault = FAULT_OPEN_CIRCUIT;
        currentState = STATE_FAULT;
      } else {
        currentState = STATE_RAMP_DOWN;
        rampStartTime = millis();
        targetHoldPower = measuredBoostPower * 0.15; // 15% of peak
        
        // Pulse GP17 Success
        digitalWrite(PIN_SUCCESS, HIGH);
        delay(10); 
        digitalWrite(PIN_SUCCESS, LOW);
      }
    }
  } 
  else if (currentState == STATE_RAMP_DOWN) {
    // Slowly reduce over 5 seconds (50 loops at 10Hz)
    // Max drop is ~1024, so dropping ~20 per loop takes 5 seconds
    if (power > targetHoldPower) {
      dutyCycle -= 20; 
    } else if (power < targetHoldPower - 0.5) {
      dutyCycle += 5;
    }
    
    if (millis() - rampStartTime >= RAMP_TIME_MS) {
      currentState = STATE_HOLD;
      holdStartTime = millis();
    }
  }
  else if (currentState == STATE_HOLD) {
    // Maintain targetHoldPower
    float error = power - targetHoldPower;
    if (power < targetHoldPower - 0.2) {
      dutyCycle += max(1, (int)(abs(error) * 2));
    } else if (power > targetHoldPower + 0.2) {
      dutyCycle -= max(1, (int)(abs(error) * 2));
    }

    // Check if Long Run is enabled and 3s hold is complete
    if (isLongRunEnabled && (millis() - holdStartTime >= 3000)) {
      currentState = STATE_COOLDOWN;
      cooldownStartTime = millis();
      cycleCount++;
    }
  }
  else if (currentState == STATE_COOLDOWN) {
    // Disable output for 1 second
    digitalWrite(PIN_EN, LOW);
    dutyCycle = 0;
    analogWrite(PIN_PWM, 0);

    if (millis() - cooldownStartTime >= 1000) {
      startSequence(); // Repeat cycle
    }
  }
  else if (currentState == STATE_FAULT) {
    // Turn off power in fault condition
    dutyCycle = 0;
    digitalWrite(PIN_EN, LOW);
    
    // Pulse GP18 Failure
    digitalWrite(PIN_FAILURE, HIGH);
  }

  // Constrain and apply duty cycle
  if (dutyCycle > PWM_RESOLUTION) dutyCycle = PWM_RESOLUTION;
  if (dutyCycle < 0) dutyCycle = 0;
  
  if (currentState != STATE_FAULT) {
    analogWrite(PIN_PWM, dutyCycle);
  }

  // --- LED STATUS LOGIC ---
  unsigned long now = millis();
  if (currentState == STATE_PEAK) {
    // Fast Blink (Toggle every 100ms loop)
    digitalWrite(PIN_STATUS_LED, (now / 100) % 2);
  } 
  else if (currentState == STATE_HOLD) {
    // Slow Fade (Approx 2s cycle)
    if (now - ledPrevTime >= 20) { // Update every 20ms for smooth fade
      ledPrevTime = now;
      if (ledDirection) {
        ledBrightness += 10;
        if (ledBrightness >= PWM_RESOLUTION) ledDirection = false;
      } else {
        ledBrightness -= 10;
        if (ledBrightness <= 0) ledDirection = true;
      }
      analogWrite(PIN_STATUS_LED, ledBrightness);
    }
  } 
  else if (currentState == STATE_WAIT_POWER || currentState == STATE_COOLDOWN) {
    // Solid HIGH (Rest Mode)
    digitalWrite(PIN_STATUS_LED, HIGH);
  } 
  else if (currentState == STATE_FAULT) {
    // Solid LOW or optionally fast-fast blink
    digitalWrite(PIN_STATUS_LED, LOW);
  }
  else {
    digitalWrite(PIN_STATUS_LED, LOW);
  }

  // Print 10Hz JSON telemetry
  printJsonTelemetry(voltage, current, power);

  delay(100); // 10Hz Sample Rate
}

// --- Sensor Interface ---

float getVoltage() {
  uint16_t raw = readRegister(REG_VOLTAGE);
  return (raw * 1.25) / 1000.0; // 1.25mV/LSB
}

float getCurrent() {
  int16_t raw = (int16_t)readRegister(REG_CURRENT);
  return (raw * 1.25) / 1000.0; // 1.25mA/LSB
}

uint16_t readRegister(uint8_t reg) {
  Wire1.beginTransmission(INA260_ADDR);
  Wire1.write(reg);
  if (Wire1.endTransmission() != 0) return 0;
  
  Wire1.requestFrom((uint8_t)INA260_ADDR, (uint8_t)2);
  if (Wire1.available() < 2) return 0;
  
  uint16_t value = Wire1.read() << 8;
  value |= Wire1.read();
  return value;
}

void writeRegister(uint8_t reg, uint16_t value) {
  Wire1.beginTransmission(INA260_ADDR);
  Wire1.write(reg);
  Wire1.write(value >> 8);
  Wire1.write(value & 0xFF);
  Wire1.endTransmission();
}

// --- Telemetry Reporting ---

void printJsonTelemetry(float v, float i, float p) {
  int ft_stat = digitalRead(PIN_FT);
  int alert_stat = digitalRead(PIN_ALERT);
  float duty_pct = (dutyCycle * 100.0) / PWM_RESOLUTION;
  
  String stateStr = "";
  if (currentState == STATE_WAIT_POWER) stateStr = "WAIT_POWER";
  else if (currentState == STATE_PEAK) stateStr = "PEAK";
  else if (currentState == STATE_RAMP_DOWN) stateStr = "RAMP_DOWN";
  else if (currentState == STATE_HOLD) stateStr = "HOLD";
  else if (currentState == STATE_COOLDOWN) stateStr = "COOLDOWN";
  else if (currentState == STATE_FAULT) stateStr = "FAULT";

  String faultStr = "NONE";
  if (currentFault == FAULT_SHORT_CIRCUIT) faultStr = "SHORT_CIRCUIT";
  else if (currentFault == FAULT_OPEN_CIRCUIT) faultStr = "OPEN_CIRCUIT";
  else if (currentFault == FAULT_HARDWARE) faultStr = "DRIVER_HARDWARE_FAULT";

  // Build JSON String
  Serial1.print("{\"V\":"); Serial1.print(v, 2);
  Serial1.print(",\"I\":"); Serial1.print(i, 3);
  Serial1.print(",\"W\":"); Serial1.print(p, 2);
  Serial1.print(",\"Duty\":"); Serial1.print(duty_pct, 1);
  Serial1.print(",\"Freq\":"); Serial1.print(PWM_FREQUENCY);
  Serial1.print(",\"MaxW\":"); Serial1.print(measuredBoostPower, 2);
  Serial1.print(",\"TgtW\":"); Serial1.print(targetHoldPower, 2);
  Serial1.print(",\"Cycles\":"); Serial1.print(cycleCount);
  Serial1.print(",\"State\":\""); Serial1.print(stateStr);
  Serial1.print("\",\"Fault\":\""); Serial1.print(faultStr);
  Serial1.print("\",\"LongRun\":"); Serial1.print(isLongRunEnabled ? "true" : "false");
  Serial1.print(",\"FT_Pin\":"); Serial1.print(ft_stat);
  Serial1.print(",\"Alert_Pin\":"); Serial1.print(alert_stat);
  Serial1.println("}");
}


