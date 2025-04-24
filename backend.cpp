#include "Particle.h"
#include <LiquidCrystal.h>

// ─── PIN ASSIGNMENTS ───────────────────────────────────────
#define RS_PIN    D2
#define EN_PIN    D3
#define D4_PIN    D4
#define D5_PIN    D5
#define D6_PIN    D6
#define D7_PIN    D7
#define TEMP_PIN  A0
#define LIGHT_PIN A1
#define PIR_PIN   D0
#define FAN_PIN   D9
#define SERVO_PIN D8

// ─── LCD SETUP ─────────────────────────────────────────────
LiquidCrystal lcd(RS_PIN, EN_PIN, D4_PIN, D5_PIN, D6_PIN, D7_PIN);

// ─── GLOBAL STATE & DEFAULTS ───────────────────────────────
double currentTempC   = 0.0;
double targetTempC    = 22.0;  // settable via cloud
int    currentLight   = 0;
bool   currentMotion  = false;
int    mode           = 0;     // 0=Off, 1=Cool, 2=Heat
int    fanSpeed       = 0;     // 0–3

// ─── CLOUD VARIABLES ──────────────────────────────────────
Particle.variable("cV_temp",         currentTempC);
Particle.variable("cV_targetTemp",   targetTempC);
Particle.variable("cV_light",        currentLight);
Particle.variable("cV_motion",       currentMotion);
Particle.variable("cV_mode",         mode);
Particle.variable("cV_fanSpeed",     fanSpeed);

// ─── CLOUD FUNCTIONS ──────────────────────────────────────
Particle.function("cF_setTargetTemp", setTargetTempFromString);
Particle.function("cF_setMode",        setModeFromString);

// ─── LOW-LEVEL LCD 4-BIT DRIVERS ──────────────────────────
void pulseEnable() {
  digitalWrite(EN_PIN, HIGH);
  delayMicroseconds(1);
  digitalWrite(EN_PIN, LOW);
  delayMicroseconds(50);
}

void sendNibble(uint8_t bits) {
  digitalWrite(D4_PIN, (bits >> 0) & 1);
  digitalWrite(D5_PIN, (bits >> 1) & 1);
  digitalWrite(D6_PIN, (bits >> 2) & 1);
  digitalWrite(D7_PIN, (bits >> 3) & 1);
  pulseEnable();
}

void sendByte(uint8_t b, bool isData) {
  digitalWrite(RS_PIN, isData);
  sendNibble(b >> 4);       // high nibble
  sendNibble(b & 0x0F);     // low nibble
  delayMicroseconds(50);
}

void lcdCommand(uint8_t cmd) { sendByte(cmd, LOW); }
void lcdData   (uint8_t d  ) { sendByte(d,   HIGH); }

void initLCD() {
  pinMode(RS_PIN, OUTPUT);
  pinMode(EN_PIN, OUTPUT);
  pinMode(D4_PIN, OUTPUT);
  pinMode(D5_PIN, OUTPUT);
  pinMode(D6_PIN, OUTPUT);
  pinMode(D7_PIN, OUTPUT);
  delay(50);
  // Init sequence for 4-bit:
  for (int i = 0; i < 3; i++) { sendNibble(0x03); delay(5); }
  sendNibble(0x02); delay(1);
  lcdCommand(0x28); // 2-line, 5×8 font
  lcdCommand(0x0C); // display on, cursor off
  lcdCommand(0x01); // clear
  delay(2);
  lcdCommand(0x06); // entry mode
}

void lcdPrint(const char* txt) {
  while (*txt) lcdData(*txt++);
}

// ─── CLOUD FUNCTION HANDLERS ────────────────────────
// args = "<double>"
int setTargetTempFromString(String s) {
  double v = s.toFloat();
  v = max(10.0, min(40.0, v)); // ensure within range
  targetTempC = v;
  Particle.publish("targetTempChanged", String(v), PRIVATE);
  return 0;
}

// args = "Off" | "Cool" | "Heat"
int setModeFromString(String s) {
  if (s == "Off")  mode = 0;
  else if (s == "Cool") mode = 1;
  else if (s == "Heat") mode = 2;
  else return -1; // invalid mode
  Particle.publish("modeChanged", s, PRIVATE);
  return mode;
}

// ─── PARTICLE SETUP ────────────────────────────────────────
void setup() {
  // initialize LCD
  lcd.begin(16, 2);
  lcd.clear();

  // pins
  pinMode(TEMP_PIN,  INPUT);
  pinMode(LIGHT_PIN, INPUT);
  pinMode(PIR_PIN,   INPUT);
  pinMode(FAN_PIN,   OUTPUT);
  pinMode(SERVO_PIN, OUTPUT);
}

// ─── MAIN LOOP ─────────────────────────────────────────────
void loop() {
  // 1) Read sensors
  currentLight  = analogRead(LIGHT_PIN);
  currentTempC  = (analogRead(TEMP_PIN) - 620) * 0.0806;  // sensor to Celsius
  currentMotion = (digitalRead(PIR_PIN) == HIGH);

  // 2) Decide fanSpeed (0–3)
  if (mode == 0 || !currentMotion) {
    fanSpeed = 0;  // OFF
  }
  else if (mode == 2) {            // Heat
    fanSpeed = (currentTempC < targetTempC) ? 3 : 0;
  }
  else {                            // Cool
    fanSpeed = (currentTempC > targetTempC) ? 3 : 0;
  }

  // 3) Drive fan (0–255)
  analogWrite(FAN_PIN, fanSpeed * 64);  // PWM control

  // 4) Sweep servo on motion
  if (currentMotion) {
    for (int a = 0; a <= 180; a += 15) {
      analogWrite(SERVO_PIN, map(a, 0, 180, 0, 255));
      delay(20);
    }
  }

  // 5) Update LCD
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Temp: ");
  lcd.print(currentTempC, 1);
  lcd.print("C");

  lcd.setCursor(0, 1);
  lcd.print("Setting: ");
  String setting;
  switch (fanSpeed) {
    case 3: setting = "HIGH"; break;
    case 2: setting = "MEDIUM"; break;
    case 1: setting = "LOW"; break;
    default: setting = "OFF"; break;
  }
  lcd.print(setting);

  delay(500);  // delay to keep LCD readable
}
