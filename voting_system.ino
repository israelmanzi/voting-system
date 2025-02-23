#include <Adafruit_Fingerprint.h>
#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>
#include <Wire.h>
#include <SoftwareSerial.h>

SoftwareSerial mySerial(2, 3);
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);
LiquidCrystal_I2C lcd(0x27, 16, 2);

#define RECORD_SIZE 6
#define MAX_FINGERPRINTS 127
#define MAX_SCAN_ATTEMPTS 2
#define SCAN_RETRY_DELAY 1000

#define BUTTON_VIEW 4
#define BUTTON_AUTH 5
#define BUTTON_REG 6

unsigned int votesCandidate1 = 0;
unsigned int votesCandidate2 = 0;
uint8_t currentVoterID = 0;
uint8_t currentVoteCandidate = 0;

enum State {
  MAIN_MENU,
  AUTH_SCAN,
  REGISTER_MODE,
  VOTE_CONFIRM,
  VOTE_SCAN,
  VIEW_VOTES
};

State currentState = MAIN_MENU;

void updateLCD(const char *line1, const char *line2 = "") {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(line1);
  if (strlen(line2) > 0) {
    lcd.setCursor(0, 1);
    lcd.print(line2);
  }
}

int getEEPROMAddress(uint8_t id) {
  return id * RECORD_SIZE;
}

bool saveFingerprint(uint8_t id) {
  if (id >= MAX_FINGERPRINTS)
    return false;
  int addr = getEEPROMAddress(id);
  EEPROM.write(addr, 1);
  unsigned long timestamp = millis();
  EEPROM.put(addr + 1, timestamp);
  EEPROM.write(addr + 5, 0);
  return true;
}

bool isFingerSaved(uint8_t id) {
  if (id >= MAX_FINGERPRINTS)
    return false;
  int addr = getEEPROMAddress(id);
  return EEPROM.read(addr) == 1;
}

bool hasVoted(uint8_t id) {
  if (id >= MAX_FINGERPRINTS)
    return true;
  int addr = getEEPROMAddress(id);
  return EEPROM.read(addr + 5) == 1;
}

void markAsVoted(uint8_t id) {
  if (id >= MAX_FINGERPRINTS)
    return;
  int addr = getEEPROMAddress(id);
  EEPROM.write(addr + 5, 1);
}

uint8_t authFingerprint() {
  int attempts = 0;
  while (attempts < MAX_SCAN_ATTEMPTS) {
    uint8_t p = finger.getImage();

    if (p == FINGERPRINT_NOFINGER) {
      delay(100);
      continue;
    }

    if (p != FINGERPRINT_OK) {
      updateLCD("Scan Error", "Try Again");
      delay(1000);
      return p;
    }

    p = finger.image2Tz();
    if (p != FINGERPRINT_OK) {
      updateLCD("Image Error", "Try Again");
      delay(1000);
      return p;
    }

    p = finger.fingerFastSearch();
    if (p == FINGERPRINT_OK) {
      if (isFingerSaved(finger.fingerID)) {
        if (hasVoted(finger.fingerID)) {
          updateLCD("Already Voted", "Access Denied");
          delay(2000);
          return FINGERPRINT_NOTFOUND;
        }
        currentVoterID = finger.fingerID;
        char buffer[16];
        sprintf(buffer, "ID: %d", finger.fingerID);
        updateLCD("Authorized", buffer);
        delay(2000);
        return FINGERPRINT_OK;
      } else {
        updateLCD("Not Registered", "Please Register");
        delay(2000);
        return FINGERPRINT_NOTFOUND;
      }
    }

    attempts++;
    if (attempts >= MAX_SCAN_ATTEMPTS) {
      updateLCD("Timeout", "Try Again");
      delay(1000);
      return FINGERPRINT_TIMEOUT;
    }

    updateLCD("Not Found", "Try Again");
    delay(1000);
  }
  return FINGERPRINT_NOFINGER;
}

bool verifyVoterFingerprint(uint8_t storedID) {
  int attempts = 0;
  while (attempts < MAX_SCAN_ATTEMPTS) {
    uint8_t p = finger.getImage();

    if (p == FINGERPRINT_NOFINGER) {
      delay(100);
      continue;
    }

    if (p != FINGERPRINT_OK) {
      updateLCD("Scan Error", "Try Again");
      delay(1000);
      return false;
    }

    p = finger.image2Tz();
    if (p != FINGERPRINT_OK) {
      updateLCD("Process Error", "Try Again");
      delay(1000);
      return false;
    }

    p = finger.fingerFastSearch();
    if (p == FINGERPRINT_OK && finger.fingerID == storedID) {
      return true;
    }

    attempts++;
    if (attempts >= MAX_SCAN_ATTEMPTS) {
      updateLCD("Timeout", "Try Again");
      delay(1000);
      return false;
    }

    delay(SCAN_RETRY_DELAY);
  }
  return false;
}

uint8_t registerFingerprint() {
  updateLCD("New Register", "Place Finger");
  delay(2000);

  uint8_t p = finger.getImage();
  if (p != FINGERPRINT_OK)
    return p;

  p = finger.image2Tz();
  if (p != FINGERPRINT_OK)
    return p;

  p = finger.fingerFastSearch();
  if (p == FINGERPRINT_OK) {
    updateLCD("Already Exists", "Try Different");
    delay(2000);
    return FINGERPRINT_NOFINGER;
  }

  p = finger.getImage();
  if (p != FINGERPRINT_OK)
    return p;

  p = finger.image2Tz(1);
  if (p != FINGERPRINT_OK)
    return p;

  updateLCD("Remove Finger", "");
  delay(2000);

  p = 0;
  while (p != FINGERPRINT_NOFINGER) {
    p = finger.getImage();
  }

  updateLCD("Place Same", "Finger Again");

  p = FINGERPRINT_NOFINGER;
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    if (p == FINGERPRINT_NOFINGER) {
      delay(500);
    } else if (p != FINGERPRINT_OK) {
      updateLCD("Scan Error", "Try Again");
      delay(1000);
      return p;
    }
  }

  p = finger.image2Tz(2);
  if (p != FINGERPRINT_OK)
    return p;

  p = finger.createModel();
  if (p != FINGERPRINT_OK)
    return p;

  uint8_t id = 1;
  while (id < MAX_FINGERPRINTS && isFingerSaved(id)) {
    id++;
  }

  if (id >= MAX_FINGERPRINTS) {
    updateLCD("Database Full", "Cannot Register");
    delay(2000);
    return FINGERPRINT_PACKETRECIEVEERR;
  }

  p = finger.storeModel(id);
  if (p == FINGERPRINT_OK) {
    if (saveFingerprint(id)) {
      char buffer[16];
      sprintf(buffer, "ID: %d", id);
      updateLCD("Register Success", buffer);
      delay(2000);
      return FINGERPRINT_OK;
    }
  }

  updateLCD("Register Failed", "Try Again");
  delay(2000);
  return p;
}

void setup() {
  Serial.begin(9600);
  lcd.init();
  lcd.backlight();

  pinMode(BUTTON_VIEW, INPUT_PULLUP);
  pinMode(BUTTON_AUTH, INPUT_PULLUP);
  pinMode(BUTTON_REG, INPUT_PULLUP);

  updateLCD("System", "Starting...");
  delay(2000);

  finger.begin(57600);
  if (!finger.verifyPassword()) {
    updateLCD("Sensor Error!", "Check wiring");
    while (1);
  }

  currentState = MAIN_MENU;
  updateLCD("Ready", "Select Option");
}

void loop() {
  switch (currentState) {
    case MAIN_MENU:
      updateLCD("1:Auth 2:Reg", "3:Votes");
      if (digitalRead(BUTTON_AUTH) == LOW) {
        delay(200);
        currentState = AUTH_SCAN;
        updateLCD("Auth Mode", "Place Finger");
      } else if (digitalRead(BUTTON_REG) == LOW) {
        delay(200);
        currentState = REGISTER_MODE;
      } else if (digitalRead(BUTTON_VIEW) == LOW) {
        delay(200);
        currentState = VIEW_VOTES;
      }
      break;

    case AUTH_SCAN:
      {
        uint8_t result = authFingerprint();
        if (result == FINGERPRINT_OK) {
          currentState = VOTE_CONFIRM;
          updateLCD("Select Vote", "1:C1 2:C2");
        } else if (result == FINGERPRINT_NOFINGER) {
          updateLCD("Auth Mode", "Place Finger");
        } else {
          currentState = MAIN_MENU;
        }
      }
      break;

    case REGISTER_MODE:
      {
        uint8_t result = registerFingerprint();
        delay(1000);
        currentState = MAIN_MENU;
      }
      break;

    case VOTE_CONFIRM:
      if (digitalRead(BUTTON_AUTH) == LOW) {
        delay(200);
        updateLCD("Confirm Vote", "Place Finger");
        currentState = VOTE_SCAN;
        currentVoteCandidate = 1;
      } else if (digitalRead(BUTTON_REG) == LOW) {
        delay(200);
        updateLCD("Confirm Vote", "Place Finger");
        currentState = VOTE_SCAN;
        currentVoteCandidate = 2;
      }
      break;

    case VOTE_SCAN:
      if (verifyVoterFingerprint(currentVoterID)) {
        if (currentVoteCandidate == 1) {
          votesCandidate1++;
        } else {
          votesCandidate2++;
        }
        markAsVoted(currentVoterID);
        updateLCD("Vote Recorded", currentVoteCandidate == 1 ? "Candidate 1" : "Candidate 2");
        delay(2000);
        currentVoterID = 0;
        currentVoteCandidate = 0;
        currentState = MAIN_MENU;
      } else {
        updateLCD("Auth Failed", "Vote Cancelled");
        delay(2000);
        currentVoterID = 0;
        currentVoteCandidate = 0;
        currentState = MAIN_MENU;
      }
      break;

    case VIEW_VOTES:
      {
        char line1[17], line2[17];
        sprintf(line1, "C1:%u votes", votesCandidate1);
        sprintf(line2, "C2:%u votes", votesCandidate2);
        updateLCD(line1, line2);
        delay(4000);
        currentState = MAIN_MENU;
      }
      break;
  }
}
