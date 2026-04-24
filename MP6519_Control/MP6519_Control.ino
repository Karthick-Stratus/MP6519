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

// INA260 Configuration
#define INA260_ADDR 0x40
#define REG_CONFIG 0x00
#define REG_CURRENT 0x01
#define REG_VOLTAGE 0x02
#define REG_POWER 0x03

// Power Control Configuration
const unsigned long BOOST_TIME_MS = 2000; // 2 seconds
float measuredBoostPower = 0.0;
float targetHoldPower = 0.0;

enum SystemState {
  STATE_BOOST,
  STATE_HOLD
};
SystemState currentState = STATE_BOOST;

// PWM Settings
const int PWM_FREQUENCY = 20000; // 20kHz
const int PWM_RESOLUTION = 1024; // 10-bit range
int dutyCycle = PWM_RESOLUTION;  // Start at 100%

unsigned long startTime = 0;

// Function Prototypes
uint16_t readRegister(uint8_t reg);
void writeRegister(uint8_t reg, uint16_t value);
float getVoltage();
float getCurrent();
void printColumnHeader();
void printTelemetry(float v, float i, float p);

void setup() {
  Serial1.setTX(0);
  Serial1.setRX(1);
  Serial1.begin(115200);
  
  // Wait 3 seconds to allow time to open Serial Monitor after boot
  delay(3000); 
  Serial1.println("\n\n=== BOOTING MP6519 SYSTEM ===");

  // Initialize Pins
  pinMode(PIN_EN, OUTPUT);
  pinMode(PIN_MODE, OUTPUT);
  pinMode(PIN_PWM, OUTPUT);
  pinMode(PIN_FT, INPUT_PULLUP);

  // Set initial states
  digitalWrite(PIN_EN, LOW);    // Keep disabled initially to prevent standby mode
  digitalWrite(PIN_MODE, HIGH); // Set default mode
  
  // Setup I2C on GP14/15 (Must use Wire1 because GP14/15 map to I2C1)
  Wire1.setSDA(PIN_SDA);
  Wire1.setSCL(PIN_SCL);
  Wire1.begin();

  // Configure PWM
  analogWriteFreq(PWM_FREQUENCY);
  analogWriteRange(PWM_RESOLUTION);
  
  // Apply initial PWM before enabling to satisfy MP6519 startup sequence
  dutyCycle = PWM_RESOLUTION; // Start at 100% duty cycle
  analogWrite(PIN_PWM, dutyCycle);
  delay(2); // Short delay to ensure PWM is active
  
  digitalWrite(PIN_EN, HIGH);   // Enable the driver now that PWM is active
  
  startTime = millis();
  
  Serial1.println("--- MP6519 Brake Control System Initialized ---");
  printColumnHeader();
}

void loop() {
  unsigned long elapsed = millis() - startTime;
  
  // Read Telemetry
  float voltage = getVoltage();
  float current = getCurrent();
  float power = voltage * current;

  if (currentState == STATE_BOOST) {
    // Keep duty cycle at 100%
    dutyCycle = PWM_RESOLUTION;
    
    // Track the highest power seen during the boost phase
    if (power > measuredBoostPower) {
      measuredBoostPower = power;
    }
    
    // Check if 2 seconds have passed
    if (elapsed >= BOOST_TIME_MS) {
      currentState = STATE_HOLD;
      // Target power is 10% of the measured boost power (reduced by 90%)
      targetHoldPower = measuredBoostPower * 0.10; 
      
      Serial1.print("\n[INFO] Boost Phase Complete. Max Power: ");
      Serial1.print(measuredBoostPower);
      Serial1.print("W. Target Hold Power: ");
      Serial1.print(targetHoldPower);
      Serial1.println("W\n");
      printColumnHeader();
    }
  } else if (currentState == STATE_HOLD) {
    // Tune duty cycle to reach and maintain targetHoldPower
    float error = power - targetHoldPower;
    
    if (power < targetHoldPower - 0.2) {
      // Increase duty cycle (faster response if error is large)
      dutyCycle += max(1, (int)(abs(error) * 2));
    } else if (power > targetHoldPower + 0.2) {
      // Decrease duty cycle
      dutyCycle -= max(1, (int)(abs(error) * 2));
    }
    
    // Constrain duty cycle to valid bounds
    if (dutyCycle > PWM_RESOLUTION) dutyCycle = PWM_RESOLUTION;
    if (dutyCycle < 0) dutyCycle = 0;
  }

  analogWrite(PIN_PWM, dutyCycle);

  // Print side-by-side telemetry
  printTelemetry(voltage, current, power);

  delay(200); // Sample rate ~5Hz
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

void printColumnHeader() {
  Serial1.println("--------------------------------------------------------------------------------------------------");
  Serial1.println("Voltage(V)  Current(A)  Power(W)  Duty(%)  Freq(Hz)  ENB  FT_Stat  MODE");
  Serial1.println("--------------------------------------------------------------------------------------------------");
}

void printTelemetry(float v, float i, float p) {
  // Use fixed width for columns
  char buffer[128];
  
  int ft_stat = digitalRead(PIN_FT);
  int enb_stat = digitalRead(PIN_EN);
  int mode_stat = digitalRead(PIN_MODE);
  float duty_pct = (dutyCycle * 100.0) / PWM_RESOLUTION;

  // Print values side by side
  Serial1.print(v, 2);           Serial1.print(" V\t");
  Serial1.print(i, 3);           Serial1.print(" A\t");
  Serial1.print(p, 2);           Serial1.print(" W\t");
  Serial1.print(duty_pct, 1);    Serial1.print(" %\t");
  Serial1.print(PWM_FREQUENCY);  Serial1.print(" Hz\t");
  Serial1.print(enb_stat ? "HIGH" : "LOW"); Serial1.print("\t");
  Serial1.print(ft_stat ? "OK" : "FAULT");  Serial1.print("\t");
  Serial1.println(mode_stat ? "HIGH" : "LOW");
}
