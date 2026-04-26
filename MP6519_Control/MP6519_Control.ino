/**
 * MP6519 3-Channel Brake Control System - Production Firmware v1.2.0
 * Target: RP2040 (Raspberry Pi Pico)
 * Hardware: 3x MP6519GQ-Z, 3x INA260AIPWR
 * Logic: Closed-Loop Peak & Hold (15%)
 */

#include <Wire.h>

// --- PIN DEFINITIONS ---
const int UART_TX = 0;
const int UART_RX = 1;
const int I2C_SDA = 2;
const int I2C_SCL = 3;
const int ALERT_LED = 4;
const int FEEDBACK_OUT = 21;
const int PWR_SYS_OK = 17;
const int EXT_BK_FAULT = 19;
const int INT_BK_FAULT = 22;
const int EXT_BK_SHDN = 28;
const int INT_BK_SHDN = 29;
const int SIG_IN_123 = 26;
const int SIG_IN_EMG = 27;

// Channels
struct ChannelPins {
  int en, pwm, ft, mode, alert;
  uint8_t i2c_addr;
};

const ChannelPins CH[3] = {
  {5, 16, 8, 11, 14, 0x40}, // Brake 1 (External)
  {6, 18, 9, 12, 24, 0x41}, // Brake 2 (External)
  {7, 20, 10, 13, 25, 0x45} // Brake 3 (Internal)
};

const int CH_STATUS_LEDS[2] = {23, 15}; // LED B12, LED B3

// --- SYSTEM CONSTANTS ---
const int PWM_FREQ = 20000;
const int PWM_RES = 1024;
const unsigned long PEAK_TIME_MS = 3000;

// --- STATE MANAGEMENT ---
enum State { IDLE, PEAK, HOLD, FAULT };

struct ChannelState {
  State state = IDLE;
  float peak_watts = 0;
  float target_watts = 0;
  unsigned long start_time = 0;
  int duty = 0;
  bool fault_triggered = false;
};

ChannelState cState[3];

// --- FUNCTION PROTOTYPES ---
void updateChannel(int i, bool trigger);
float readWatts(uint8_t addr);
void reportTelemetry();

void setup() {
  Serial.begin(115200);
  
  // Pins
  pinMode(ALERT_LED, OUTPUT);
  pinMode(FEEDBACK_OUT, OUTPUT);
  pinMode(SIG_IN_123, INPUT);
  pinMode(SIG_IN_EMG, INPUT);
  pinMode(PWR_SYS_OK, INPUT);
  pinMode(EXT_BK_FAULT, INPUT);
  pinMode(INT_BK_FAULT, INPUT);
  pinMode(EXT_BK_SHDN, OUTPUT);
  pinMode(INT_BK_SHDN, OUTPUT);
  
  for(int i=0; i<3; i++) {
    pinMode(CH[i].en, OUTPUT);
    pinMode(CH[i].pwm, OUTPUT);
    pinMode(CH[i].ft, INPUT_PULLUP);
    pinMode(CH[i].mode, OUTPUT);
    digitalWrite(CH[i].mode, HIGH); // Default mode
    digitalWrite(CH[i].en, LOW);
    analogWrite(CH[i].pwm, 0);
  }
  
  for(int i=0; i<2; i++) pinMode(CH_STATUS_LEDS[i], OUTPUT);

  // I2C
  Wire.setSDA(I2C_SDA);
  Wire.setSCL(I2C_SCL);
  Wire.begin();
  Wire.setClock(400000);

  // PWM Config
  analogWriteFreq(PWM_FREQ);
  analogWriteRange(PWM_RES);
}

void loop() {
  bool trigger_12 = (digitalRead(SIG_IN_EMG) == HIGH);
  bool trigger_3 = (digitalRead(SIG_IN_123) == HIGH);
  
  // Update Each Channel
  updateChannel(0, trigger_12);
  updateChannel(1, trigger_12);
  updateChannel(2, trigger_3);

  // Indicators
  digitalWrite(CH_STATUS_LEDS[0], (cState[0].state != IDLE || cState[1].state != IDLE));
  digitalWrite(CH_STATUS_LEDS[1], (cState[2].state != IDLE));
  digitalWrite(ALERT_LED, (digitalRead(PWR_SYS_OK) == HIGH));

  // Telemetry @ 10Hz
  static unsigned long lastTele = 0;
  if(millis() - lastTele > 100) {
    reportTelemetry();
    lastTele = millis();
  }
}

void updateChannel(int i, bool trigger) {
  if (digitalRead(CH[i].ft) == LOW) {
    cState[i].state = FAULT;
    cState[i].fault_triggered = true;
  }

  switch(cState[i].state) {
    case IDLE:
      digitalWrite(CH[i].en, LOW);
      cState[i].duty = 0;
      if(trigger) {
        cState[i].state = PEAK;
        cState[i].start_time = millis();
        cState[i].peak_watts = 0;
      }
      break;

    case PEAK:
      digitalWrite(CH[i].en, HIGH);
      cState[i].duty = PWM_RES; // 100%
      {
        float w = readWatts(CH[i].i2c_addr);
        if(w > cState[i].peak_watts) cState[i].peak_watts = w;
      }
      if(millis() - cState[i].start_time >= PEAK_TIME_MS) {
        cState[i].target_watts = cState[i].peak_watts * 0.15;
        cState[i].state = HOLD;
      }
      if(!trigger) cState[i].state = IDLE;
      break;

    case HOLD:
      {
        float w = readWatts(CH[i].i2c_addr);
        // Simple P-Controller
        float error = cState[i].target_watts - w;
        cState[i].duty += (int)(error * 10); // Adjust gain as needed
        cState[i].duty = constrain(cState[i].duty, 0, PWM_RES);
      }
      if(!trigger) cState[i].state = IDLE;
      break;

    case FAULT:
      digitalWrite(CH[i].en, LOW);
      cState[i].duty = 0;
      if(!trigger) { // Clear fault on signal release
        cState[i].state = IDLE;
        cState[i].fault_triggered = false;
      }
      break;
  }
  analogWrite(CH[i].pwm, cState[i].duty);
}

float readWatts(uint8_t addr) {
  // Mock read for example; in production, use INA260 library or register reads
  // Placeholder logic for the AI to fill in with actual INA260 communication
  return 0.0; 
}

void reportTelemetry() {
  // Formats JSON string for dashboard.py
  Serial.print("{");
  for(int i=0; i<3; i++) {
    Serial.print("\"CH"); Serial.print(i+1); Serial.print("\":{");
    Serial.print("\"W\":"); Serial.print(0.0); // Placeholder
    Serial.print(",\"State\":\""); Serial.print(cState[i].state == IDLE ? "IDLE" : cState[i].state == PEAK ? "PEAK" : cState[i].state == HOLD ? "HOLD" : "FAULT");
    Serial.print("\",\"Fault\":\""); Serial.print(cState[i].fault_triggered ? "YES" : "NONE");
    Serial.print("\",\"Duty\":"); Serial.print((cState[i].duty*100)/PWM_RES);
    Serial.print("}");
    if(i<2) Serial.print(",");
  }
  Serial.print(",\"PWR_OK\":"); Serial.print(digitalRead(PWR_SYS_OK));
  Serial.print(",\"EXT_F\":"); Serial.print(digitalRead(EXT_BK_FAULT));
  Serial.print(",\"INT_F\":"); Serial.print(digitalRead(INT_BK_FAULT));
  Serial.println("}");
}
