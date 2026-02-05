/*
 *ZONE 1 SMART VAV — SAFE IMPROVEMENTS (MINIMAL CHANGE)
 * 1) VAV stroke modeled as 0–90° (typical quarter-turn damper actuator).
 * 2) Position deadband: when close to target -> motor STOP (prevents continuous spinning).
 * 3) Soft limits: stop if at end stop and command would push further.
 * 4) Clean Modbus setpoint semantics:
 *    - REG_SETPOINT (102) is always temp setpoint (°C x10).
 *    - AUTO uses that setpoint (from knob or Node-RED).
 */

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>
#include <WiFi.h>
#include <ModbusIP_ESP8266.h>
#include "AiEsp32RotaryEncoder.h"

// --- CONFIGURATION ---
#define MAX_SPEED 180

// VAV model (portfolio-grade)
static const float STROKE_DEG = 90.0f;       // damper travel modeled 0..90°
static const float MIN_VENT_DEG = 10.0f;     // minimum ventilation (demo) ~10°
static const float DBAND_C = 0.5f;           // temp deadband for holding min vent
static const float VAV_GAIN_DEG_PER_C = 30.0f; // 3°C error => ~90° open

// Position control stability
static const float POS_DEADBAND_DEG = 0.8f;  // within this error -> stop motor
static const int   PWM_DEADZONE = 18;        // tiny PWM -> stop (avoid whining)

// --- WIFI SETTINGS ---
const char* ssid = "AndroidAP";
const char* pass = "LabView2020";

// --- PINS (VERIFIED) ---
#define PIN_ENC_A      32
#define PIN_ENC_B      33
#define PIN_L298N_ENA  14
#define PIN_L298N_IN1  27
#define PIN_L298N_IN2  26
#define ROT_DT         16
#define ROT_CLK        17
#define ROT_SW         5
#define DHTPIN         4
#define DHTTYPE        DHT11

// --- MODBUS REGISTERS ---
const int REG_TEMP      = 100;  // RoomTemp x10
const int REG_SETPOINT  = 102;  // TempSetpoint x10  (Node-RED writes here)
const int REG_POS       = 103;  // Damper Position (deg 0..90)  (keep your chart)
const int REG_MODE      = 104;  // 0 MANUAL, 1 SELECT, 2 AUTO

ModbusIP mb;

// --- OBJECTS ---
Adafruit_SSD1306 display(128, 64, &Wire, -1);
AiEsp32RotaryEncoder rotary = AiEsp32RotaryEncoder(ROT_DT, ROT_CLK, ROT_SW, -1, 4);
DHT dht(DHTPIN, DHTTYPE);

// --- PID CONSTANTS (kept from you) ---
const float Kp = 0.10f;
const float Ki = 0.15f;
const float Kd = 0.015f;
const float N  = 50.0f;
const float Ts = 0.001f;
const float DEG_PER_COUNT = 360.0f / 408.0f;

// --- SHARED DATA ---
volatile float active_target_deg = 0.0f;   // damper target degrees (0..90)
volatile float setpoint_c = 22.0f;         // temp setpoint in °C
volatile float current_pos_deg = 0.0f;     // damper measured degrees (0..90)
volatile long  encoder_counts = 0;
volatile float room_temp = 24.0f;
bool wifi_connected = false;

// --- MODES ---
enum ControlMode { DIRECT_DRIVE, SETPOINT_SELECT, AUTO_VAV };
volatile ControlMode current_mode = DIRECT_DRIVE;

// --- INTERRUPTS  ---
void IRAM_ATTR readEncoder() {
  if (digitalRead(PIN_ENC_B) > 0) encoder_counts++;
  else encoder_counts--;
}
void IRAM_ATTR readRotaryISR() { rotary.readEncoder_ISR(); }

// --- MOTOR STOP  ---
static inline void motorStop() {
  ledcWrite(PIN_L298N_ENA, 0);
  digitalWrite(PIN_L298N_IN1, LOW);
  digitalWrite(PIN_L298N_IN2, LOW);
}

// --- HMI TASK (CORE 0) ---
void HMITask(void * parameter) {
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.setTextColor(SSD1306_WHITE);
  dht.begin();

  rotary.begin();
  rotary.setup(readRotaryISR);
  rotary.setAcceleration(50);

  // Start in MANUAL damper control
  rotary.setBoundaries(0, (int)STROKE_DEG, false);
  rotary.setEncoderValue(0);

  for(;;) {
    // 1) READ TEMP
    static unsigned long lastDHT = 0;
    if (millis() - lastDHT > 2000) {
      float t = dht.readTemperature();
      if (!isnan(t)) room_temp = t;
      lastDHT = millis();
    }

    // 2) HANDLE BUTTON (CYCLE MODES)
    if (rotary.isEncoderButtonClicked()) {
      if (current_mode == DIRECT_DRIVE) {
        current_mode = SETPOINT_SELECT;
        rotary.setBoundaries(15, 30, false);
        rotary.setEncoderValue((long)roundf(setpoint_c));
      }
      else if (current_mode == SETPOINT_SELECT) {
        current_mode = AUTO_VAV;
        rotary.setBoundaries(15, 30, false);
        rotary.setEncoderValue((long)roundf(setpoint_c));
      }
      else {
        current_mode = DIRECT_DRIVE;
        rotary.setBoundaries(0, (int)STROKE_DEG, false);
        rotary.setEncoderValue((long)roundf(active_target_deg));
      }
    }

    // 3) HANDLE KNOB
    if (rotary.encoderChanged()) {
      float val = (float)rotary.readEncoder();

      if (current_mode == DIRECT_DRIVE) {
        // Manual damper target in degrees (0..90)
        active_target_deg = constrain(val, 0.0f, STROKE_DEG);
      } else {
        // Select/AUTO: knob edits temp setpoint
        setpoint_c = constrain(val, 15.0f, 30.0f);
      }
    }

    // 4) OLED
    display.clearDisplay();
    display.setTextSize(1);

    display.setCursor(0,0);
    if (current_mode == DIRECT_DRIVE) display.print("MANUAL");
    else if (current_mode == SETPOINT_SELECT) display.print("SELECT");
    else display.print("AUTO");

    display.setCursor(50,0);
    display.print(wifi_connected ? " WIFI:OK" : " WIFI:..");

    display.drawLine(0, 10, 128, 10, SSD1306_WHITE);

    display.setCursor(0, 18);
    display.print("RM: "); display.print(room_temp, 1); display.print("C");

   display.setCursor(0, 30);
    if (current_mode == SETPOINT_SELECT) {
      display.print("SP*: "); display.print(setpoint_c, 1); display.print("C"); // editing
    } else {
      display.print("SP: ");  display.print(setpoint_c, 1); display.print("C");
    }


    display.setCursor(0, 42);
    display.print("TGT: "); display.print(active_target_deg, 0); display.print("deg");

    display.setCursor(70, 42);
    display.print("ACT: "); display.print(current_pos_deg, 0);

    display.setCursor(0, 56);
    if(wifi_connected) display.print(WiFi.localIP());
    else display.print("Connecting...");

    display.display();
    vTaskDelay(50 / portTICK_PERIOD_MS);
  }
}

void setup() {
  Serial.begin(115200);

  // I/O
  pinMode(PIN_ENC_A, INPUT_PULLUP);
  pinMode(PIN_ENC_B, INPUT_PULLUP);
  pinMode(PIN_L298N_IN1, OUTPUT);
  pinMode(PIN_L298N_IN2, OUTPUT);

  ledcAttachChannel(PIN_L298N_ENA, 20000, 8, 1);
  motorStop();

  attachInterrupt(digitalPinToInterrupt(PIN_ENC_A), readEncoder, RISING);

  // WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);

  // Wait up to 15 seconds
  int timeout = 30;
  while (WiFi.status() != WL_CONNECTED && timeout > 0) {
    delay(500);
    timeout--;
  }

  if (WiFi.status() == WL_CONNECTED) {
    wifi_connected = true;
    mb.server();
    mb.addHreg(REG_TEMP, 0);
    mb.addHreg(REG_SETPOINT, 220); // 22.0C
    mb.addHreg(REG_POS, 0);        // deg 0..90
    mb.addHreg(REG_MODE, 0);
  } else {
    wifi_connected = false;
  }

  // Start HMI task
  xTaskCreatePinnedToCore(HMITask, "HMI", 10000, NULL, 1, NULL, 0);
}

void loop() {
// -------- Modbus servicing ----------
  if (wifi_connected) {
    mb.task();

    // If Node-RED writes a new setpoint, accept it
    int sp_c10 = mb.Hreg(REG_SETPOINT);
    float net_sp = sp_c10 / 10.0f;

    
    // (prevents the network from constantly overwriting SELECT knob edits)
    static int last_sp_c10 = 220;
    if (sp_c10 != last_sp_c10) {
      setpoint_c = constrain(net_sp, 15.0f, 30.0f);
      last_sp_c10 = sp_c10;
    }

    // Publish live points
    mb.Hreg(REG_TEMP, (int)(room_temp * 10));
    mb.Hreg(REG_MODE, (int)current_mode);

    // publish the current local setpoint 
    mb.Hreg(REG_SETPOINT, (int)(setpoint_c * 10));
  }


  // -------- AUTO VAV math  ----------
  if (current_mode == AUTO_VAV) {
    float e = room_temp - setpoint_c;

    float tgt = MIN_VENT_DEG; // baseline
    if (e <= DBAND_C) {
      tgt = MIN_VENT_DEG;
    } else {
      tgt = MIN_VENT_DEG + (e - DBAND_C) * VAV_GAIN_DEG_PER_C;
    }
    active_target_deg = constrain(tgt, 0.0f, STROKE_DEG);
  }

  // -------- Position feedback ----------
  float pos_deg = encoder_counts * DEG_PER_COUNT;
  // Soft clamp to modeled stroke (important)
  pos_deg = constrain(pos_deg, 0.0f, STROKE_DEG);
  current_pos_deg = pos_deg;

  if (wifi_connected) {
    mb.Hreg(REG_POS, (int)current_pos_deg);
  }

  // -------- Controller (1 kHz) ----------
  static unsigned long last_time = 0;
  unsigned long now = micros();

  if (now - last_time >= 1000) {
    last_time = now;

    float error = active_target_deg - current_pos_deg;

    // 1) STOP if close enough (prevents continuous spinning)
    if (fabs(error) <= POS_DEADBAND_DEG) {
      motorStop();
      return;
    }

    // 2) SOFT LIMITS: prevent pushing at ends
    bool at_min = (current_pos_deg <= 0.5f);
    bool at_max = (current_pos_deg >= (STROKE_DEG - 0.5f));
    if ((at_min && error < 0) || (at_max && error > 0)) {
      motorStop();
      return;
    }

    // Your PID-like engine (kept)
    static float derivative = 0, integral = 0;
    float D_term = (Kd * error - derivative) * N;
    derivative += D_term * Ts;
    integral += error * Ts;

    if (integral > 20) integral = 20;
    if (integral < -20) integral = -20;

    float V_cmd = (Kp * error) + (Ki * integral) + D_term;

    // PWM scaling (kept) + deadzone stop (added)
    int pwm = (int)(fabs(V_cmd) / 12.0f * 255.0f);
    if (pwm > MAX_SPEED) pwm = MAX_SPEED;

    if (pwm < PWM_DEADZONE) {
      motorStop();
      return;
    }

    // Direction
    if (V_cmd > 0) {
      digitalWrite(PIN_L298N_IN1, HIGH);
      digitalWrite(PIN_L298N_IN2, LOW);
    } else {
      digitalWrite(PIN_L298N_IN1, LOW);
      digitalWrite(PIN_L298N_IN2, HIGH);
    }

    ledcWrite(PIN_L298N_ENA, pwm);
  }
}
