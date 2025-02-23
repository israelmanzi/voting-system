#include <Adafruit_Fingerprint.h>
#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>
#include <Wire.h>

// Software Serial for fingerprint sensor
SoftwareSerial mySerial(2, 3);

Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);
LiquidCrystal_I2C lcd(0x27, 16, 2);

// EEPROM structure:
// For each fingerprint ID (0-127):
// byte 0: validity flag (1 = valid, 0 = invalid)
// bytes 1-4: timestamp
#define RECORD_SIZE 5
#define MAX_FINGERPRINTS 127

void updateLCD(const char* line1, const char* line2 = "") {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(line1);
  if (strlen(line2) > 0) {
    lcd.setCursor(0, 1);
    lcd.print(line2);
  }
}

// Function to get EEPROM address for a fingerprint ID
int getEEPROMAddress(uint8_t id) {
  return id * RECORD_SIZE;
}

// Function to save fingerprint data to EEPROM
bool saveFingerprint(uint8_t id) {
  if (id >= MAX_FINGERPRINTS) return false;

  int addr = getEEPROMAddress(id);

  // Save validity flag
  EEPROM.write(addr, 1);

  // Save timestamp (milliseconds since start)
  unsigned long timestamp = millis();
  EEPROM.put(addr + 1, timestamp);

  return true;
}

// Function to check if fingerprint is saved
bool isFingerSaved(uint8_t id) {
  if (id >= MAX_FINGERPRINTS) return false;

  int addr = getEEPROMAddress(id);
  return EEPROM.read(addr) == 1;
}

void setup() {
  Serial.begin(9600);
  lcd.init();
  lcd.backlight();

  updateLCD("System", "Starting...");
  delay(2000);

  // Initialize fingerprint sensor
  finger.begin(57600);
  if (!finger.verifyPassword()) {
    updateLCD("Sensor Error!", "Check wiring");
    while (1);
  }

  updateLCD("System Ready", "Place finger");
  delay(2000);
}

uint8_t getFingerprintID() {
  uint8_t p = finger.getImage();
  if (p != FINGERPRINT_OK) return p;

  p = finger.image2Tz();
  if (p != FINGERPRINT_OK) return p;

  p = finger.fingerFastSearch();
  if (p == FINGERPRINT_OK) {
    // If found, save to EEPROM if not already saved
    if (!isFingerSaved(finger.fingerID)) {
      if (saveFingerprint(finger.fingerID)) {
        updateLCD("Auth Saved", "to EEPROM");
        delay(2000);
      }
    }

    char buffer[16];
    sprintf(buffer, "ID: %d", finger.fingerID);
    updateLCD("Access Granted", buffer);
    delay(3000);
  } else if (p == FINGERPRINT_NOTFOUND) {
    updateLCD("Not Registered", "Access Denied");
    delay(3000);
  }

  return p;
}

void loop() {
  updateLCD("Ready", "Place finger");
  delay(50);

  uint8_t result = getFingerprintID();

  if (result == FINGERPRINT_NOFINGER) {
    return;
  }

  delay(1000);
}
