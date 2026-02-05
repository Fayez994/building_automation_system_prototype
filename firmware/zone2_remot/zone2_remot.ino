/*
 * ZONE 2 — REMOTE I/O (MODBUS TCP SERVER)
 * Hardware: ESP32 + HC-SR501 + AEDIKO 2CH Relay (ACTIVE-HIGH) + LEDs + 2x Buttons
 * Role: Remote I/O for PC OpenPLC (Soft PLC). Node-RED supervises.
 *
 * Modbus Mapping (LOCKED):
 * Discrete Inputs:
 *   DI 1: Z2_Occupied (PIR)
 *   DI 2: Z2_ManualBtn
 *   DI 3: Z2_EStop
 *
 * Coils:
 *   Coil 1: Z2_LightCmd (Relay1)
 *   Coil 2: Z2_AuxCmd   (Relay2)
 *   Coil 3: Z2_OccLED
 *   Coil 4: Z2_LightLED
 */

#include <WiFi.h>
#include <ModbusIP_ESP8266.h>

// --- WIFI SETTINGS ---
const char* ssid = "AndroidAP";
const char* pass = "LabView2020";

// ---------- GPIO (LOCKED SUGGESTION) ----------
#define PIN_PIR        34   // DI1
#define PIN_MANUAL_BTN 25   // DI2 (pull-up)
#define PIN_ESTOP_BTN  23   // DI3 optional (pull-up)

#define PIN_RELAY1     26   // Coil1
#define PIN_RELAY2     27   // Coil2
#define PIN_LED_OCC    18   // Coil3
#define PIN_LED_LIGHT  19   // Coil4

// ---------- RELAY POLARITY (LOCKED) ----------
static const uint8_t RELAY_ON  = HIGH; // ACTIVE-HIGH
static const uint8_t RELAY_OFF = LOW;

// ---------- MODBUS ADDRESSES ----------
static const uint16_t DI_OCCUPIED = 1;
static const uint16_t DI_MANUAL   = 2;
static const uint16_t DI_ESTOP    = 3;

static const uint16_t COIL_LIGHT  = 1;
static const uint16_t COIL_AUX    = 2;
static const uint16_t COIL_OCCLED = 3;
static const uint16_t COIL_LLED   = 4;

ModbusIP mb;

void setup() {
  Serial.begin(115200);

  // Inputs
  pinMode(PIN_PIR, INPUT);              // HC-SR501 OUT is TTL
  pinMode(PIN_MANUAL_BTN, INPUT_PULLUP);
  pinMode(PIN_ESTOP_BTN, INPUT_PULLUP);

  // Outputs
  pinMode(PIN_RELAY1, OUTPUT);
  pinMode(PIN_RELAY2, OUTPUT);
  pinMode(PIN_LED_OCC, OUTPUT);
  pinMode(PIN_LED_LIGHT, OUTPUT);

  // Default safe state
  digitalWrite(PIN_RELAY1, RELAY_OFF);
  digitalWrite(PIN_RELAY2, RELAY_OFF);
  digitalWrite(PIN_LED_OCC, LOW);
  digitalWrite(PIN_LED_LIGHT, LOW);

  // WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);

  Serial.print("Connecting WiFi");
  int timeout = 30; // 15s
  while (WiFi.status() != WL_CONNECTED && timeout-- > 0) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi FAILED. Remote I/O will idle (outputs OFF).");
    return;
  }

  Serial.print("WiFi OK. IP: ");
  Serial.println(WiFi.localIP());

  // Modbus TCP server
  mb.server();

  // Discrete Inputs (read-only)
  mb.addIsts(DI_OCCUPIED, 0);
  mb.addIsts(DI_MANUAL,   0);
  mb.addIsts(DI_ESTOP,    0);

  // Coils (read/write)
  mb.addCoil(COIL_LIGHT,  0);
  mb.addCoil(COIL_AUX,    0);
  mb.addCoil(COIL_OCCLED, 0);
  mb.addCoil(COIL_LLED,   0);
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    // Fail-safe outputs OFF if WiFi drops
    digitalWrite(PIN_RELAY1, RELAY_OFF);
    digitalWrite(PIN_RELAY2, RELAY_OFF);
    digitalWrite(PIN_LED_OCC, LOW);
    digitalWrite(PIN_LED_LIGHT, LOW);
    delay(200);
    return;
  }

  mb.task();

  // ---------- READ FIELD INPUTS ----------
  bool occupied = (digitalRead(PIN_PIR) == HIGH);
  bool manual   = (digitalRead(PIN_MANUAL_BTN) == LOW); // pressed = LOW
  bool estop    = (digitalRead(PIN_ESTOP_BTN) == LOW);  // pressed = LOW

  mb.Ists(DI_OCCUPIED, occupied);
  mb.Ists(DI_MANUAL, manual);
  mb.Ists(DI_ESTOP, estop);

  // ---------- APPLY OUTPUT COMMANDS (FROM PC PLC) ----------
  // Safety gating can be done in PC logic; we still force OFF on E-Stop locally as a second layer.
  bool light_cmd  = mb.Coil(COIL_LIGHT);
  bool aux_cmd    = mb.Coil(COIL_AUX);
  bool occ_led    = mb.Coil(COIL_OCCLED);
  bool light_led  = mb.Coil(COIL_LLED);

  if (estop) {
    light_cmd = false;
    aux_cmd = false;
    light_led = false;
  }

  digitalWrite(PIN_RELAY1, light_cmd ? RELAY_ON : RELAY_OFF);
  digitalWrite(PIN_RELAY2, aux_cmd   ? RELAY_ON : RELAY_OFF);

  digitalWrite(PIN_LED_OCC,   occ_led   ? HIGH : LOW);
  digitalWrite(PIN_LED_LIGHT, light_led ? HIGH : LOW);
}