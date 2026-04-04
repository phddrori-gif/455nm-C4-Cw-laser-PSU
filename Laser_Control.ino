#include <Wire.h>
#include <SPI.h>
#include <MFRC522.h>
#include <LiquidCrystal_I2C.h>
#include <math.h>
#include <Watchdog_t4.h>

WDT_T4<WDT1> wdt;

// RFID RC522
#define RST_PIN 9
#define SS_PIN 10
MFRC522 mfrc522(SS_PIN, RST_PIN);

// LCD I2C
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Known UIDs
const byte UID_A[] = { 0x73, 0xD6, 0xEE, 0xAE };  // KONOR
const byte UID_B[] = { 0x53, 0x77, 0xAB, 0x98 };  // SILVIA
const byte UID_A_LEN = sizeof(UID_A);
const byte UID_B_LEN = sizeof(UID_B);

// Pins
const int THERMISTOR_LASER_PIN  = A0;  // pin 14
const int THERMISTOR_MOSFET_PIN = A3;  // pin 17
const int VOLTAGE1_PIN   = A1;  // pin 15
const int VOLTAGE2_PIN   = A2;  // pin 16
const int CURRENT_PIN    = A8;  // pin 22
const int PWM_LASER_PIN  = 23;
const int PWM_ON_PIN     = 8;
const int KONOR_LED_PIN  = 6;
const int SILVIA_LED_PIN = 7;

// Constants
const float ADC_REF = 3.3;
const int ADC_RES = 4095;
const float SHUNT_RESISTANCE = 0.1;
const float VOLT_DIV_RATIO_1 = 57.0;
const float VOLT_DIV_RATIO_2 = 37.0;

// System State
bool isA_loggedIn = false;
bool isB_loggedIn = false;
bool isLocked = false;
bool inCooldown = false;
int unknownScanCount = 0;
int pwmLaserValue = 255;  // default 100%

// Override
bool overrideMode = false;
unsigned long overrideStartTime = 0;
const unsigned long overrideDuration = 60000;
unsigned long lastUIDScanTime = 0;
const unsigned long overrideScanWindow = 3000;
byte lastUID[10];
byte lastUIDLen = 0;

// Timing
unsigned long lastVoltageSwitch = 0;
unsigned long lastDisplayUpdate = 0;
unsigned long cooldownStart = 0;
const unsigned long voltageSwitchInterval = 5000;
const unsigned long displayInterval = 200;
const unsigned long cooldownDuration = 60000;
bool showVoltage1 = true;

// Soft start
unsigned long softStartStartTime = 0;
const unsigned long softStartDuration = 5000;  // 5 seconds

void setup() {
  Serial.begin(115200);
  analogReadResolution(12);
  analogReadAveraging(8);
  pinMode(PWM_LASER_PIN, OUTPUT);
  pinMode(PWM_ON_PIN, OUTPUT);
  pinMode(KONOR_LED_PIN, OUTPUT);
  pinMode(SILVIA_LED_PIN, OUTPUT);
  analogWrite(PWM_LASER_PIN, 0);
  analogWrite(PWM_ON_PIN, 0);

  WDT_timings_t config = {
    .trigger = 8000,
    .timeout = 16000,
    .callback = nullptr
  };
  wdt.begin(config);

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("LCD OK...");
  delay(500);

  SPI.begin();
  mfrc522.PCD_Init();
  delay(100);
  lcd.clear();
  lcd.print("RFID Ready");
  delay(1000);
  lcd.clear();
}

void loop() {
  wdt.feed();

  static int lastPwmOnValue = -1;
  static int lastLaserPwmValue = -1;
  static bool lastPwmOn = false;

  digitalWrite(KONOR_LED_PIN, isA_loggedIn);
  digitalWrite(SILVIA_LED_PIN, isB_loggedIn);

  float tempC = readTemperature(THERMISTOR_LASER_PIN);
  if ((tempC >= 60.0 || tempC > 150.0) && !inCooldown) enterCooldown("TEMP ERROR");
  if (inCooldown) { handleCooldown(); return; }

  if (Serial.available()) {
    int input = Serial.parseInt();
    if (input >= 0 && input <= 255) {
      pwmLaserValue = input;
    }
  }

  // RFID logic
  if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
    byte* uid = mfrc522.uid.uidByte;
    byte uidLen = mfrc522.uid.size;
    unsigned long now = millis();

    if (compareUID(uid, uidLen, UID_A, UID_A_LEN)) {
      if (lastUIDLen == UID_A_LEN && compareUID(uid, uidLen, lastUID, lastUIDLen)) {
        if (now - lastUIDScanTime <= overrideScanWindow) {
          overrideMode = true;
          overrideStartTime = now;
          softStartStartTime = now;
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("OVERRIDE MODE");
          delay(1000);
          lcd.clear();
        }
      }
      memcpy(lastUID, uid, uidLen);
      lastUIDLen = uidLen;
      lastUIDScanTime = now;
    }

    if (compareUID(uid, uidLen, UID_A, UID_A_LEN)) {
      isA_loggedIn = !isA_loggedIn;
      unknownScanCount = 0;
    } else if (compareUID(uid, uidLen, UID_B, UID_B_LEN)) {
      if (!isLocked) {
        isB_loggedIn = !isB_loggedIn;
        unknownScanCount = 0;
      }
    } else {
      unknownScanCount++;
      if (unknownScanCount >= 2) {
        isA_loggedIn = false;
        isB_loggedIn = false;
        isLocked = true;
      }
    }

    mfrc522.PICC_HaltA();
  }

  if (overrideMode && millis() - overrideStartTime > overrideDuration) {
    overrideMode = false;
  }

  float currentVoltage = analogRead(CURRENT_PIN) * (ADC_REF / ADC_RES);
  float currentAmps = currentVoltage / SHUNT_RESISTANCE;
  float voltage1 = analogRead(VOLTAGE1_PIN) * (ADC_REF / ADC_RES) * VOLT_DIV_RATIO_1;
  float voltage2 = analogRead(VOLTAGE2_PIN) * (ADC_REF / ADC_RES) * VOLT_DIV_RATIO_2;

  bool pwmOn = ((isA_loggedIn && isB_loggedIn) || overrideMode) && !isLocked;

  // Detect rising edge for soft start
  if (pwmOn && !lastPwmOn) {
    softStartStartTime = millis();
  }
  lastPwmOn = pwmOn;

  // Soft-start PWM (pin 8)
  int pwmOnValue = 0;
  if (pwmOn) {
    unsigned long elapsed = millis() - softStartStartTime;
    pwmOnValue = (elapsed >= softStartDuration) ? 255 : map(elapsed, 0, softStartDuration, 0, 255);
  } else {
    pwmOnValue = 0;
  }
  if (pwmOnValue != lastPwmOnValue) {
    analogWrite(PWM_ON_PIN, pwmOnValue);
    lastPwmOnValue = pwmOnValue;
  }

  // Laser PWM (pin 23)
  int laserPwmValue = (pwmOn ? pwmLaserValue : 0);
  if (laserPwmValue != lastLaserPwmValue) {
    analogWrite(PWM_LASER_PIN, laserPwmValue);
    lastLaserPwmValue = laserPwmValue;
  }

  // Display control
  if (millis() - lastVoltageSwitch > voltageSwitchInterval) {
    showVoltage1 = !showVoltage1;
    lastVoltageSwitch = millis();
  }

  if (millis() - lastDisplayUpdate >= displayInterval) {
    lcd.setCursor(0, 0);

    if (overrideMode) {
      lcd.print("OVERRIDE MODE   ");
      lcd.setCursor(0, 1);
      unsigned long remaining = (overrideDuration - (millis() - overrideStartTime)) / 1000;
      lcd.print("Time Left: ");
      if (remaining < 10) lcd.print(" ");
      lcd.print(remaining);
      lcd.print("s   ");
    } else if (pwmOn) {
      lcd.print("POWER:ON  PWM:");
      lcd.print((int)(pwmLaserValue * 100 / 255));
      lcd.print("% ");
      lcd.setCursor(0, 1);
      float displayV = showVoltage1 ? voltage1 : voltage2;
      lcd.print(showVoltage1 ? "V1:" : "V2:");
      lcd.print((int)displayV);
      if (displayV < 100) lcd.print(" ");
      if (displayV < 10) lcd.print(" ");
      lcd.print(" ");
      printFixed(lcd, currentAmps, 3, 1);
      lcd.print("A");
    } else {
      lcd.print("Silvia:");
      lcd.print(isB_loggedIn ? "Y " : "N ");
      lcd.print("Konor:");
      lcd.print(isA_loggedIn ? "Y" : "N ");
      lcd.print("   ");
      lcd.setCursor(0, 1);
      float displayV = showVoltage1 ? voltage1 : voltage2;
      lcd.print(showVoltage1 ? "V1:" : "V2:");
      lcd.print((int)displayV);
      if (displayV < 100) lcd.print(" ");
      if (displayV < 10) lcd.print(" ");
      lcd.print(" ");
      printFixed(lcd, currentAmps, 3, 1);
      lcd.print("A");
    }

    lastDisplayUpdate = millis();
  }
}

float readTemperature(int pin) {
  const float Vcc = 3.3;
  const float R_FIXED = 10000.0;
  const float BETA = 3950.0;
  const float T0 = 298.15;
  int raw = analogRead(pin);
  if (raw < 5 || raw > 4090) return 999.0;
  float Vout = raw * (Vcc / ADC_RES);
  float R_therm = R_FIXED * ((Vcc / Vout) - 1.0);
  float tempK = 1.0 / (log(R_therm / R_FIXED) / BETA + (1.0 / T0));
  return tempK - 273.15;
}

bool compareUID(const byte *uid1, byte len1, const byte *uid2, byte len2) {
  if (len1 != len2) return false;
  for (byte i = 0; i < len1; i++) {
    if (uid1[i] != uid2[i]) return false;
  }
  return true;
}

void enterCooldown(const char* reason) {
  isA_loggedIn = false;
  isB_loggedIn = false;
  isLocked = true;
  inCooldown = true;
  overrideMode = false;
  analogWrite(PWM_LASER_PIN, 0);
  analogWrite(PWM_ON_PIN, 0);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(reason);
  cooldownStart = millis();
}

void handleCooldown() {
  unsigned long elapsed = millis() - cooldownStart;
  unsigned long remaining = (cooldownDuration - elapsed) / 1000;
  lcd.setCursor(0, 1);
  lcd.print("Cooldown: ");
  if (remaining < 10) lcd.print(" ");
  lcd.print(remaining);
  lcd.print("s   ");
  if (elapsed >= cooldownDuration) {
    inCooldown = false;
    isLocked = false;
    lcd.clear();
    lcd.print("Cooldown done");
    delay(1000);
    lcd.clear();
  }
}

void printFixed(LiquidCrystal_I2C &lcd, float value, int digits, int decimals) {
  char buffer[8];
  dtostrf(value, digits + decimals + 1, decimals, buffer);
  lcd.print(buffer);
}
