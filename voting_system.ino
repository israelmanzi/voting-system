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

#define RED_LED 9
#define GREEN_LED 10

#define LONG_PRESS_DURATION 3000

unsigned int votesCandidate1 = 0;
unsigned int votesCandidate2 = 0;
uint8_t currentVoterID = 0;
uint8_t currentVoteCandidate = 0;

enum State
{
  MAIN_MENU,
  AUTH_SCAN,
  REGISTER_MODE,
  VOTE_CONFIRM,
  VOTE_SCAN,
  VIEW_VOTES,
  RESET_CONFIRM
};

State currentState = MAIN_MENU;

void updateLCD(const char *line1, const char *line2 = "")
{
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(line1);
  if (strlen(line2) > 0)
  {
    lcd.setCursor(0, 1);
    lcd.print(line2);
  }
}

void indicateSuccess()
{
  analogWrite(GREEN_LED, 10);
  analogWrite(RED_LED, 0);
  delay(1000);
  analogWrite(GREEN_LED, 0);
}

void indicateError()
{
  analogWrite(RED_LED, 10);
  analogWrite(GREEN_LED, 0);
  delay(1000);
  analogWrite(RED_LED, 0);
}

int getEEPROMAddress(uint8_t id)
{
  return id * RECORD_SIZE;
}

bool saveFingerprint(uint8_t id)
{
  if (id >= MAX_FINGERPRINTS)
    return false;
  int addr = getEEPROMAddress(id);
  EEPROM.write(addr, 1);
  unsigned long timestamp = millis();
  EEPROM.put(addr + 1, timestamp);
  EEPROM.write(addr + 5, 0);
  return true;
}

bool isFingerSaved(uint8_t id)
{
  if (id >= MAX_FINGERPRINTS)
    return false;
  int addr = getEEPROMAddress(id);
  return EEPROM.read(addr) == 1;
}

bool hasVoted(uint8_t id)
{
  if (id >= MAX_FINGERPRINTS)
    return true;
  int addr = getEEPROMAddress(id);
  return EEPROM.read(addr + 5) == 1;
}

void markAsVoted(uint8_t id)
{
  if (id >= MAX_FINGERPRINTS)
    return;
  int addr = getEEPROMAddress(id);
  EEPROM.write(addr + 5, 1);
}

void resetSystem() {
  votesCandidate1 = 0;
  votesCandidate2 = 0;

  updateLCD("Resetting", "Fingerprints...");

  Serial.println("Registered fingerprints before reset:");

  uint8_t registeredCount = 0;
  for (uint8_t id = 1; id < MAX_FINGERPRINTS; id++) {
    if (isFingerSaved(id)) {
      Serial.print("ID #");
      Serial.print(id);
      Serial.print(" - ");

      int addr = getEEPROMAddress(id);
      unsigned long timestamp;
      EEPROM.get(addr + 1, timestamp);

      bool hasVotedStatus = EEPROM.read(addr + 5) == 1;

      Serial.print("Registered at: ");
      Serial.print(timestamp);
      Serial.print(", Voted: ");
      Serial.println(hasVotedStatus ? "Yes" : "No");

      registeredCount++;
    }
  }

  Serial.print("Total registered fingerprints: ");
  Serial.println(registeredCount);

  for (uint8_t id = 1; id < MAX_FINGERPRINTS; id++) {
    int addr = getEEPROMAddress(id);
    EEPROM.write(addr, 0);
    EEPROM.write(addr + 5, 0);

    unsigned long zeroTimestamp = 0;
    EEPROM.put(addr + 1, zeroTimestamp);

    if (isFingerSaved(id)) {
      finger.deleteModel(id);
    }
  }

  updateLCD("Clearing", "Database...");
  finger.emptyDatabase();

  indicateSuccess();
  updateLCD("System Reset", "Complete");
  delay(2000);

  Serial.println("System reset complete - all fingerprint data cleared");
}

uint8_t authFingerprint()
{
  int attempts = 0;
  while (attempts < MAX_SCAN_ATTEMPTS)
  {
    uint8_t p = finger.getImage();

    if (p == FINGERPRINT_NOFINGER)
    {
      delay(100);
      continue;
    }

    if (p != FINGERPRINT_OK)
    {
      updateLCD("Scan Error", "Try Again");
      indicateError();
      delay(1000);
      return p;
    }

    p = finger.image2Tz();
    if (p != FINGERPRINT_OK)
    {
      updateLCD("Image Error", "Try Again");
      indicateError();
      delay(1000);
      return p;
    }

    p = finger.fingerFastSearch();
    if (p == FINGERPRINT_OK)
    {
      if (isFingerSaved(finger.fingerID))
      {
        if (hasVoted(finger.fingerID))
        {
          updateLCD("Already Voted", "Access Denied");
          indicateError();
          delay(2000);
          return FINGERPRINT_NOTFOUND;
        }
        currentVoterID = finger.fingerID;
        char buffer[16];
        sprintf(buffer, "ID: %d", finger.fingerID);
        updateLCD("Authorized", buffer);
        indicateSuccess();
        delay(2000);
        return FINGERPRINT_OK;
      }
      else
      {
        updateLCD("Not Registered", "Please Register");
        indicateError();
        delay(2000);
        return FINGERPRINT_NOTFOUND;
      }
    }

    attempts++;
    if (attempts >= MAX_SCAN_ATTEMPTS)
    {
      updateLCD("Timeout", "Try Again");
      indicateError();
      delay(1000);
      return FINGERPRINT_TIMEOUT;
    }

    updateLCD("Not Found", "Try Again");
    indicateError();
    delay(1000);
  }
  return FINGERPRINT_NOFINGER;
}

bool verifyVoterFingerprint(uint8_t storedID)
{
  int attempts = 0;
  while (attempts < MAX_SCAN_ATTEMPTS)
  {
    uint8_t p = finger.getImage();

    if (p == FINGERPRINT_NOFINGER)
    {
      delay(100);
      continue;
    }

    if (p != FINGERPRINT_OK)
    {
      updateLCD("Scan Error", "Try Again");
      indicateError();
      delay(1000);
      return false;
    }

    p = finger.image2Tz();
    if (p != FINGERPRINT_OK)
    {
      updateLCD("Process Error", "Try Again");
      indicateError();
      delay(1000);
      return false;
    }

    p = finger.fingerFastSearch();
    if (p == FINGERPRINT_OK && finger.fingerID == storedID)
    {
      return true;
    }

    attempts++;
    if (attempts >= MAX_SCAN_ATTEMPTS)
    {
      updateLCD("Timeout", "Try Again");
      indicateError();
      delay(1000);
      return false;
    }

    delay(SCAN_RETRY_DELAY);
  }
  return false;
}

uint8_t registerFingerprint()
{
  updateLCD("New Register", "Place Finger");
  delay(2000);

  uint8_t p = finger.getImage();
  if (p != FINGERPRINT_OK)
  {
    indicateError();
    return p;
  }

  p = finger.image2Tz();
  if (p != FINGERPRINT_OK)
  {
    indicateError();
    return p;
  }

  p = finger.fingerFastSearch();
  if (p == FINGERPRINT_OK)
  {
    updateLCD("Already Exists", "Try Different");
    indicateError();
    delay(2000);
    return FINGERPRINT_NOFINGER;
  }

  p = finger.getImage();
  if (p != FINGERPRINT_OK)
  {
    indicateError();
    return p;
  }

  p = finger.image2Tz(1);
  if (p != FINGERPRINT_OK)
  {
    indicateError();
    return p;
  }

  updateLCD("Remove Finger", "");
  delay(2000);

  p = 0;
  while (p != FINGERPRINT_NOFINGER)
  {
    p = finger.getImage();
  }

  updateLCD("Place Same", "Finger Again");

  p = FINGERPRINT_NOFINGER;
  while (p != FINGERPRINT_OK)
  {
    p = finger.getImage();
    if (p == FINGERPRINT_NOFINGER)
    {
      delay(500);
    }
    else if (p != FINGERPRINT_OK)
    {
      updateLCD("Scan Error", "Try Again");
      indicateError();
      delay(1000);
      return p;
    }
  }

  p = finger.image2Tz(2);
  if (p != FINGERPRINT_OK)
  {
    indicateError();
    return p;
  }

  p = finger.createModel();
  if (p != FINGERPRINT_OK)
  {
    indicateError();
    return p;
  }

  uint8_t id = 1;
  while (id < MAX_FINGERPRINTS && isFingerSaved(id))
  {
    id++;
  }

  if (id >= MAX_FINGERPRINTS)
  {
    updateLCD("Database Full", "Cannot Register");
    indicateError();
    delay(2000);
    return FINGERPRINT_PACKETRECIEVEERR;
  }

  p = finger.storeModel(id);
  if (p == FINGERPRINT_OK)
  {
    if (saveFingerprint(id))
    {
      char buffer[16];
      sprintf(buffer, "ID: %d", id);
      updateLCD("Register Success", buffer);
      indicateSuccess();
      delay(2000);
      return FINGERPRINT_OK;
    }
  }

  updateLCD("Register Failed", "Try Again");
  indicateError();
  delay(2000);
  return p;
}

void setup()
{
  Serial.begin(9600);
  lcd.init();
  lcd.backlight();

  pinMode(BUTTON_VIEW, INPUT_PULLUP);
  pinMode(BUTTON_AUTH, INPUT_PULLUP);
  pinMode(BUTTON_REG, INPUT_PULLUP);

  pinMode(RED_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);

  updateLCD("System", "Starting...");
  delay(2000);

  finger.begin(57600);
  if (!finger.verifyPassword())
  {
    updateLCD("Sensor Error!", "Check wiring");
    indicateError();
    while (1)
      ;
  }

  currentState = MAIN_MENU;
  updateLCD("Ready", "Select Option");
}

void loop()
{
  if (digitalRead(BUTTON_VIEW) == LOW && currentState == MAIN_MENU) {
    updateLCD("Hold for Reset", "");

    unsigned long pressStartTime = millis();
    while (digitalRead(BUTTON_VIEW) == LOW) {
      int elapsedTime = (millis() - pressStartTime) / 1000;
      char countdownText[16];
      sprintf(countdownText, "Wait: %d sec", 3 - elapsedTime);
      updateLCD("Hold for Reset", countdownText);

      if (millis() - pressStartTime >= LONG_PRESS_DURATION) {
        currentState = RESET_CONFIRM;
        break;
      }
      delay(100);
    }

    if (currentState != RESET_CONFIRM) {
      if (millis() - pressStartTime < 500) {
        currentState = VIEW_VOTES;
      } else {
        currentState = MAIN_MENU;
        updateLCD("Ready", "Select Option");
      }
    }
  }

  switch (currentState)
  {
    case MAIN_MENU:
      updateLCD("1:Auth 2:Reg", "3:Votes/Reset");
      if (digitalRead(BUTTON_AUTH) == LOW)
      {
        delay(200);
        currentState = AUTH_SCAN;
        updateLCD("Auth Mode", "Place Finger");
      }
      else if (digitalRead(BUTTON_REG) == LOW)
      {
        delay(200);
        currentState = REGISTER_MODE;
      }
      break;

    case AUTH_SCAN:
      {
        uint8_t result = authFingerprint();
        if (result == FINGERPRINT_OK)
        {
          currentState = VOTE_CONFIRM;
          updateLCD("Select Vote", "1:C1 2:C2");
        }
        else if (result == FINGERPRINT_NOFINGER)
        {
          updateLCD("Auth Mode", "Place Finger");
        }
        else
        {
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
      if (digitalRead(BUTTON_AUTH) == LOW)
      {
        delay(200);
        updateLCD("Confirm Vote", "Place Finger");
        currentState = VOTE_SCAN;
        currentVoteCandidate = 1;
      }
      else if (digitalRead(BUTTON_REG) == LOW)
      {
        delay(200);
        updateLCD("Confirm Vote", "Place Finger");
        currentState = VOTE_SCAN;
        currentVoteCandidate = 2;
      }
      break;

    case VOTE_SCAN:
      if (verifyVoterFingerprint(currentVoterID))
      {
        if (currentVoteCandidate == 1)
        {
          votesCandidate1++;
        }
        else
        {
          votesCandidate2++;
        }
        markAsVoted(currentVoterID);
        updateLCD("Vote Recorded", currentVoteCandidate == 1 ? "Candidate 1" : "Candidate 2");
        indicateSuccess();
        delay(2000);
        currentVoterID = 0;
        currentVoteCandidate = 0;
        currentState = MAIN_MENU;
      }
      else
      {
        updateLCD("Auth Failed", "Vote Cancelled");
        indicateError();
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

    case RESET_CONFIRM:
      updateLCD("Confirm Reset?", "1:Yes 2:No");

      unsigned long confirmStartTime = millis();
      bool confirmationReceived = false;

      while (millis() - confirmStartTime < 5000 && !confirmationReceived) {
        if (digitalRead(BUTTON_AUTH) == LOW) {
          delay(200);
          resetSystem();
          confirmationReceived = true;
        }
        else if (digitalRead(BUTTON_REG) == LOW) {
          delay(200);
          updateLCD("Reset Cancelled", "");
          delay(1000);
          confirmationReceived = true;
        }
        delay(100);
      }

      if (!confirmationReceived) {
        updateLCD("Reset Timeout", "Cancelled");
        delay(1000);
      }

      currentState = MAIN_MENU;
      break;
  }
}
