#include <Adafruit_Fingerprint.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>

#if (defined(__AVR__) || defined(ESP8266)) && !defined(__AVR_ATmega2560__)
SoftwareSerial mySerial(2, 3);
#else
#define mySerial Serial1
#endif

Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);
LiquidCrystal_I2C lcd(0x27, 16, 2);

void updateLCD(const char* line1, const char* line2 = "") {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(line1);
  if (strlen(line2) > 0) {
    lcd.setCursor(0, 1);
    lcd.print(line2);
  }
}

void setup() {
  Serial.begin(9600);
  lcd.init();
  lcd.backlight();

  updateLCD("System", "Starting...");
  delay(2000);

  finger.begin(57600);

  if (finger.verifyPassword()) {
    updateLCD("Sensor Ready", "Place finger");
    delay(2000);
  } else {
    updateLCD("Sensor Error!", "Check wiring");
    while (1) {
      delay(1);
    }
  }
}

uint8_t getFingerprintID() {
  uint8_t p = finger.getImage();
  switch (p) {
    case FINGERPRINT_OK:
      break;
    case FINGERPRINT_NOFINGER:
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      updateLCD("Read Error", "Try again");
      delay(2000);
      return p;
    case FINGERPRINT_IMAGEFAIL:
      updateLCD("Imaging Error", "Try again");
      delay(2000);
      return p;
    default:
      updateLCD("Unknown Error", "Try again");
      delay(2000);
      return p;
  }

  // Convert image
  p = finger.image2Tz();
  switch (p) {
    case FINGERPRINT_OK:
      break;
    case FINGERPRINT_IMAGEMESS:
      updateLCD("Image unclear", "Try again");
      delay(2000);
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      updateLCD("Read Error", "Try again");
      delay(2000);
      return p;
    case FINGERPRINT_FEATUREFAIL:
    case FINGERPRINT_INVALIDIMAGE:
      updateLCD("Invalid scan", "Try again");
      delay(2000);
      return p;
    default:
      updateLCD("Unknown Error", "Try again");
      delay(2000);
      return p;
  }

  // Search for match
  p = finger.fingerFastSearch();
  if (p == FINGERPRINT_OK) {
    char buffer[16];
    sprintf(buffer, "ID: %d", finger.fingerID);
    updateLCD("Access Granted", buffer);
    delay(3000);  // Show success message longer
    return p;
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    updateLCD("Read Error", "Try again");
    delay(2000);
    return p;
  } else if (p == FINGERPRINT_NOTFOUND) {
    updateLCD("Not Registered", "Access Denied");
    delay(3000);  // Show denial message longer
    return p;
  } else {
    updateLCD("Search Error", "Try again");
    delay(2000);
    return p;
  }

  return finger.fingerID;
}

void loop() {
  updateLCD("Ready", "Place finger");
  delay(50);  // Small delay to prevent display flicker

  uint8_t result = getFingerprintID();

  if (result == FINGERPRINT_NOFINGER) {
    // No finger detected, continue polling
    return;
  }

  // After showing result message, delay before next scan
  delay(1000);
}
