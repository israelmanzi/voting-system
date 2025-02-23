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
uint8_t id;

// Helper function to clear LCD and print new message
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
  
  updateLCD("Fingerprint", "Sensor Init...");
  delay(1000);

  finger.begin(57600);

  if (finger.verifyPassword()) {
    updateLCD("Sensor Found!", "");
  } else {
    updateLCD("Sensor Not Found", "Check Wiring!");
    while (1) { delay(1); }
  }

  // Display sensor parameters
  updateLCD("Reading Sensor", "Parameters...");
  finger.getParameters();
  delay(1000);
  
  char buffer[16];
  sprintf(buffer, "%d", finger.capacity);
  updateLCD("Capacity:", buffer);
  delay(1000);
}

uint8_t readnumber(void) {
  uint8_t num = 0;
  updateLCD("Enter ID #:", "1-127");
  
  while (num == 0) {
    while (!Serial.available());
    num = Serial.parseInt();
  }
  return num;
}

void loop() {
  updateLCD("Ready to Enroll", "Enter ID#: 1-127");
  id = readnumber();
  
  if (id == 0) {
    return;
  }
  
  char buffer[16];
  sprintf(buffer, "%d", id);
  updateLCD("Enrolling ID #", buffer);
  delay(1000);

  while (!getFingerprintEnroll());
}

uint8_t getFingerprintEnroll() {
  int p = -1;
  char buffer[16];
  sprintf(buffer, "ID #%d", id);
  updateLCD("Place Finger", buffer);

  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    switch (p) {
      case FINGERPRINT_OK:
        updateLCD("Image Taken", "");
        break;
      case FINGERPRINT_NOFINGER:
        updateLCD("Waiting for", "Finger...");
        break;
      case FINGERPRINT_PACKETRECIEVEERR:
        updateLCD("Comm Error", "");
        break;
      case FINGERPRINT_IMAGEFAIL:
        updateLCD("Imaging Error", "");
        break;
      default:
        updateLCD("Unknown Error", "");
        break;
    }
  }

  p = finger.image2Tz(1);
  switch (p) {
    case FINGERPRINT_OK:
      updateLCD("Image Converted", "");
      break;
    case FINGERPRINT_IMAGEMESS:
      updateLCD("Image Too Messy", "");
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      updateLCD("Comm Error", "");
      return p;
    case FINGERPRINT_FEATUREFAIL:
    case FINGERPRINT_INVALIDIMAGE:
      updateLCD("No Fingerprint", "Features Found");
      return p;
    default:
      updateLCD("Unknown Error", "");
      return p;
  }

  updateLCD("Remove Finger", "");
  delay(2000);
  p = 0;
  while (p != FINGERPRINT_NOFINGER) {
    p = finger.getImage();
  }

  p = -1;
  updateLCD("Place Same", "Finger Again");

  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    switch (p) {
      case FINGERPRINT_OK:
        updateLCD("Image Taken", "");
        break;
      case FINGERPRINT_NOFINGER:
        updateLCD("Waiting for", "Finger...");
        break;
      case FINGERPRINT_PACKETRECIEVEERR:
        updateLCD("Comm Error", "");
        break;
      case FINGERPRINT_IMAGEFAIL:
        updateLCD("Imaging Error", "");
        break;
      default:
        updateLCD("Unknown Error", "");
        break;
    }
  }

  p = finger.image2Tz(2);
  switch (p) {
    case FINGERPRINT_OK:
      updateLCD("Image Converted", "");
      break;
    case FINGERPRINT_IMAGEMESS:
      updateLCD("Image Too Messy", "");
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      updateLCD("Comm Error", "");
      return p;
    case FINGERPRINT_FEATUREFAIL:
    case FINGERPRINT_INVALIDIMAGE:
      updateLCD("No Fingerprint", "Features Found");
      return p;
    default:
      updateLCD("Unknown Error", "");
      return p;
  }

  sprintf(buffer, "ID #%d", id);
  updateLCD("Creating Model", buffer);

  p = finger.createModel();
  if (p == FINGERPRINT_OK) {
    updateLCD("Prints Matched!", "");
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    updateLCD("Comm Error", "");
    return p;
  } else if (p == FINGERPRINT_ENROLLMISMATCH) {
    updateLCD("Prints Did Not", "Match!");
    return p;
  } else {
    updateLCD("Unknown Error", "");
    return p;
  }

  sprintf(buffer, "%d", id);
  updateLCD("Storing ID #", buffer);
  p = finger.storeModel(id);
  if (p == FINGERPRINT_OK) {
    updateLCD("Stored!", "");
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    updateLCD("Comm Error", "");
    return p;
  } else if (p == FINGERPRINT_BADLOCATION) {
    updateLCD("Invalid", "Storage Location");
    return p;
  } else if (p == FINGERPRINT_FLASHERR) {
    updateLCD("Flash Write", "Error");
    return p;
  } else {
    updateLCD("Unknown Error", "");
    return p;
  }

  return true;
}
