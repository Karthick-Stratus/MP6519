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

// INA260 Configuration
#define INA260_ADDR 0x40
#define REG_CONFIG 0x00
#define REG_CURRENT 0x01
#define REG_VOLTAGE 0x02
#define REG_POWER 0x03

// Power Control Configuration
const unsigned long BOOST_TIME_MS = 3000; // 3 seconds at 100%
const unsigned long RAMP_TIME_MS = 5000;  // 5 seconds to ramp down
float measuredBoostPower = 0.0;
float targetHoldPower = 0.0;

enum SystemState {
  STATE_PEAK,
  STATE_RAMP_DOWN,
  STATE_HOLD,
  STATE_FAULT
};
SystemState currentState = STATE_PEAK;

enum FaultState {
  FAULT_NONE,
  FAULT_SHORT_CIRCUIT,
  FAULT_OPEN_CIRCUIT
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

  // Hardware initially OFF
  digitalWrite(PIN_EN, LOW);    
  digitalWrite(PIN_MODE, HIGH); 
  dutyCycle = 0;
  analogWrite(PIN_PWM, 0);

  // Setup I2C on GP14/15 (Wire1)
  Wire1.setSDA(PIN_SDA);
  Wire1.setSCL(PIN_SCL);
  Wire1.begin();

  // Configure PWM
  analogWriteFreq(PWM_FREQUENCY);
  analogWriteRange(PWM_RESOLUTION);

  startSequence(); 
}

void startSequence() {
  currentState = STATE_PEAK;
  currentFault = FAULT_NONE;
  measuredBoostPower = 0.0;
  targetHoldPower = 0.0;
  
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

  unsigned long elapsed = millis() - startTime;
  
  // Read Telemetry
  float voltage = getVoltage();
  float current = getCurrent();
  float power = voltage * current;

  if (currentState == STATE_PEAK) {
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
  }
  else if (currentState == STATE_FAULT) {
    // Turn off power in fault condition
    dutyCycle = 0;
    digitalWrite(PIN_EN, LOW);
  }

  // Constrain and apply duty cycle
  if (dutyCycle > PWM_RESOLUTION) dutyCycle = PWM_RESOLUTION;
  if (dutyCycle < 0) dutyCycle = 0;
  
  if (currentState != STATE_FAULT) {
    analogWrite(PIN_PWM, dutyCycle);
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

// --- Telemetry Reporting ---

void printJsonTelemetry(float v, float i, float p) {
  int ft_stat = digitalRead(PIN_FT);
  float duty_pct = (dutyCycle * 100.0) / PWM_RESOLUTION;
  
  String stateStr = "";
  if (currentState == STATE_PEAK) stateStr = "PEAK";
  else if (currentState == STATE_RAMP_DOWN) stateStr = "RAMP_DOWN";
  else if (currentState == STATE_HOLD) stateStr = "HOLD";
  else if (currentState == STATE_FAULT) stateStr = "FAULT";

  String faultStr = "NONE";
  if (currentFault == FAULT_SHORT_CIRCUIT) faultStr = "SHORT_CIRCUIT";
  else if (currentFault == FAULT_OPEN_CIRCUIT) faultStr = "OPEN_CIRCUIT";

  // Build JSON String
  Serial1.print("{\"V\":"); Serial1.print(v, 2);
  Serial1.print(",\"I\":"); Serial1.print(i, 3);
  Serial1.print(",\"W\":"); Serial1.print(p, 2);
  Serial1.print(",\"Duty\":"); Serial1.print(duty_pct, 1);
  Serial1.print(",\"Freq\":"); Serial1.print(PWM_FREQUENCY);
  Serial1.print(",\"MaxW\":"); Serial1.print(measuredBoostPower, 2);
  Serial1.print(",\"TgtW\":"); Serial1.print(targetHoldPower, 2);
  Serial1.print(",\"State\":\""); Serial1.print(stateStr);
  Serial1.print("\",\"Fault\":\""); Serial1.print(faultStr);
  Serial1.print("\",\"FT_Pin\":"); Serial1.print(ft_stat);
  Serial1.println("}");
}


