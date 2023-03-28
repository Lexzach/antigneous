#include <Arduino.h>
#include <EEPROM.h>
#include <LiquidCrystal_I2C.h>
#include <string.h>

bool invertRelay = false; //------------------IF YOUR RELAYS OPERATE THE OPPOSITE OF HOW YOU EXPECT, CHANGE THIS TO 'TRUE' 

unsigned long systemClock; //-----------------SYSTEM CLOCK [VERY IMPORTANT]
unsigned long lastPulse; //-------------------LAST void loop() PULSE
bool failsafeMode = false; // If panel fails to boot, you can enter a reduced-feature down panel that is guaranteed to work

char *firmwareRev = "v1.5"; //VERSION
int EEPROMVersion = 1; //version control to rewrite eeprom after update
int EEPROMBuild = 5;

//patents
// https://patents.google.com/patent/US5559492 - simplex
// https://patents.google.com/patent/US6281789 - simplex smartsync
// https://patents.google.com/patent/US6194994 - wheelock
// https://patents.google.com/patent/US5659287A/en - gentex strobe sync
// https://patents.google.com/patent/US5959528 - gentex smartsync
// https://patents.google.com/patent/US8928191B2/en - potter weird universal sync


//----------------------------------------------------------------------------- RUNTIME VARIABLES
bool fullAlarm = false; //bool to control if this is a full alarm that requres a panel reset to clear
bool silenced = false;
bool panelUnlocked = false; //if the control panel has a key inserted
bool horn = false; //bool to control if the horns are active
bool strobe = false; //bool to control if the strobes are active
bool trouble = false; //bool to control if the panel is in trouble
bool troubleAcked = false; //bool to control if the trouble is acknowledged
bool inConfigMenu = false; //determine if the control panel is in the configuration menu

//---------------------------- Variables that are *always* true if a button is held down at all
bool resetPressed = false;
bool silencePressed = false;
bool drillPressed = false;
//----------------------------

//---------------------------- Variables that are false the *first* iteration through, caused by the variables above being true, this is handy for detecting single button presses, and not repeating code every loop
bool resetStillPressed = false;
bool silenceStillPressed = false;
bool drillStillPressed = false;
//----------------------------

bool runVerification = false; //panel receieved 0 from pull station ciruit and is now investigating
bool walkTest = false; //is the system in walk test
bool silentWalkTest = false;
bool backlightOn = true;
bool keyRequiredVisual; //this variable makes it so the user can change the security settings, exit, return, and the setting they changed persists, but it won't apply until panel restart
bool keylessSilence = false; //can the panel be silenced without a key
bool debug = false;
bool updateLockStatus = false; //if the screen needs to be updated for the lock/unlock icon
bool secondStage = false; //if the panel is in second stage
int characters[] = {32,45,46,47,48,49,50,51,52,53,54,55,56,57,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,80,81,82,83,84,85,86,87,88,89,90}; //characters allowed on name
int panelNameList[16];
int clearTimer = 0; //timer for keeping track of button holding for clearing character in the name editor
int verificationTimer = 0; //number to keep track of ms for verification
int drillTimer = 0; //number to keep track of ms for drill
int buttonCheckTimer = 0; //add a slight delay to avoid double-clicking buttons
int keyCheckTimer = 0;
int troubleTimer = 0; //ms for trouble
int codeWheelTimer = 0; //code wheel timing variable
int troubleLedTimer = 0; //number to keep track of ms for trouble light
int alarmLedTimer = 0; //alarm led timer
int troubleType = 0; //trouble type 0 - general, 1 - eol resistor
int lcdTimeoutTimer = 0; //timer to keep track of how long until lcd off
int currentScreen = -1; //update display if previous screen is not the same
int configPage = 0; //config page for the config menu
int cursorPosition = 0; //which menu item the cursor is over
int zone1Count = 0; //walk test variables
int zone2Count = 0;
int zoneAlarm = 0; //which zone is in alarm 0 - none | 1 - zone 1 | 2 - zone 2 | 3 - zone 1 & 2 | 4 - drill
int zoneTrouble = 0; //which zone is in trouble 0 - none | 1 - zone 1 | 2 - zone 2 | 3 - zone 1 & 2
int powerOnMinutes = 0; //amount of hours the panel has been powered on for, kinda like an odometer for the FACP
int powerOnMinutesCounter = 0; //keep track of when to iterate the EEPROM value for uptime minutes
String configTop; //configuration menu strings for lcd
String configBottom;
String currentConfigTop; //configuration menu strings for current lcd display
String currentConfigBottom;
bool keyRequired = false; //determine if key switch is required to operate buttons
bool verificationEnabled = true; //is verification turned on
bool eolResistor = true; //is the EOL resistor enabled
bool preAlarm = false; //use pre-alarm?
bool smokeDetectorVerification = false; //should smoke detectors activate first stage
bool smokeDetectorCurrentlyInVerification = false; //Is a smoke detector currently in verification?
bool audibleSilence = true;
bool useTwoWire = false; //does the panel use 2 wire for alarms
int twoWireTimer = 0; //timer for 2 wire alarms
int smokeDetectorTimeout = 60000; //how long to wait before toggling smoke detectors back on
int smokeDetectorPostRestartTimer = 0; //variable to keep track of the 60 seconds post-power up that the panel watches the smoke detector
int smokeDetectorPostRestartVerificationTime = 120000; //time in ms that the smoke detector should be monitored
int smokeDetectorTimer = 0; //timer to keep track of the current smoke detector timeout progress
int firstStageTime = 300000; //time in minutes that first stage should last
int firstStageTimer = 0; //timer to keep track of current first stage
int codeWheel = 0; //which alarm pattern to use, code-3 default
int strobeSync = 0; //strobe sync is not on by default 0 - none | 1 - System Sensor | 2 - Wheelock | 3 - Gentex | 4 - Simplex
int strobeSyncTimer = 0; //strobe sync timer
float verificationTime = 2500;
int panelHomescreen = 0;
int lcdTimeout = 0;
String panelName = "";
LiquidCrystal_I2C lcd(0x27,16,2);

//---------------------------------------- CUSTOM LCD CHARACTERS
byte lock[] = { //lock icon
  B01110,
  B10001,
  B10001,
  B11111,
  B11011,
  B11011,
  B11111,
  B00000
};

byte check[] = { //check mark icon
  B00000,
  B00001,
  B00011,
  B10110,
  B11100,
  B01000,
  B00000,
  B00000
};

byte cross[] = { //x mark
  B00000,
  B11011,
  B01110,
  B00100,
  B01110,
  B11011,
  B00000,
  B00000
};
//---------------------------------------- CUSTOM LCD CHARACTERS

//PIN DEFINITIONS
int zone1Pin = 15;
int zone2Pin = 15; //IF YOU WANT A SINGLE ZONE, SET THIS TO 15 AS WELL, ELSE 39.
int hornRelay = 13;
int buzzerPin = 4;
int strobeRelay = 18;
int smokeDetectorRelay = 14;
int readyLed = 27;
int silenceLed = 26;
int alarmLed = 25;
int keySwitchPin = 33;
int resetButtonPin = 32;
int silenceButtonPin = 35;
int drillButtonPin = 34;
int sclPin = 22;
int sdaPin = 21;
//----------------------------------------------------------------------------- RUNTIME VARIABLES


//----------------------------------------------------------------------------- EEPROM RESET
void resetEEPROM() {
  for (int i=0; i<=512; i++){ //write all 255's from 0-1024 address
      EEPROM.write(i,255);
  }
  Serial.println("Erased EEPROM");

  EEPROM.write(0,76);EEPROM.write(1,101);EEPROM.write(2,120);EEPROM.write(3,122);EEPROM.write(4,97);EEPROM.write(5,99);EEPROM.write(6,104); //write Lexzach to addresses 0-6
  Serial.println("Wrote LEXZACH header");
  EEPROM.write(7,0); //code-3 default
  Serial.println("Configured Code-3");
  EEPROM.write(8,0); //key not required by default
  Serial.println("Disabled key");
  EEPROM.write(9,1); //verification is turned on by default
  Serial.println("Turned on verification");
  EEPROM.write(10,25); //default verification time of 2.5 seconds
  Serial.println("Set verification time of 2.5 seconds");

  //system name "ANTIGNEOUS"
  EEPROM.write(11,65);EEPROM.write(12,78);EEPROM.write(13,84);EEPROM.write(14,73);EEPROM.write(15,71);EEPROM.write(16,78);EEPROM.write(17,69);EEPROM.write(18,79);EEPROM.write(19,85);EEPROM.write(20,83);
  Serial.println("Wrote system name 'ANTIGNEOUS'");
  for (int i=21; i<=26; i++){ //write all 0's from 21-26 address
    EEPROM.write(i,32);
  }
  Serial.println("Wrote 6 blank spaces");

  EEPROM.write(27,0); //write keyless silence
  Serial.println("Disabled keyless silence");
  EEPROM.write(28,24); //post restart smoke detector verification time
  Serial.println("Set smoke detector verification to 120 seconds");
  EEPROM.write(29,0); //strobe sync
  Serial.println("Disable strobe sync by default");
  EEPROM.write(30,0); //2 wire
  Serial.println("Disabled 2 wire alarms by default");
  EEPROM.write(50, EEPROMVersion); //write current version and build
  EEPROM.write(51, EEPROMBuild); 
  Serial.println("Wrote EEPROM version and build");
  //EEPROM.write(72,125); //EOL lenience 500 by default (take the value stored and multiply by 4 to get actual value)
  //Serial.println("Set EOL lenience to 500");
  EEPROM.write(73,1); //EOL resistor is enabled by default
  Serial.println("Enabled EOL resistor");
  EEPROM.write(74,0); //pre-alarm disabled by default
  Serial.println("Disabled pre-alarm");
  EEPROM.write(75,5); //pre-alarm first-stage is 1 minute by default
  Serial.println("Set pre-alarm first stage to 5 minutes");
  EEPROM.write(76,0); //smoke detector verification is disable by default
  Serial.println("Disabled smoke detector verification");
  EEPROM.write(77,12); //smoke detector verification is 1 minute by default
  Serial.println("Set smoke detector verification to 1 minute");
  EEPROM.write(78,0); //homescreen is panel name by default
  Serial.println("Set panel name as homescreen");
  EEPROM.write(79,1); //audible silence is enabled by default
  Serial.println("Enabled audible silence");
  EEPROM.write(80,0); //lcd timeout is disabled by default (time in MS found by taking value and multiplying it by 15000)
  Serial.println("Disabled LCD timeout");
  EEPROM.write(81,0);
  EEPROM.write(82,0);
  EEPROM.write(83,0);
  EEPROM.write(84,0);
  Serial.println("Set runtime to 0 minutes");


  EEPROM.commit();
  Serial.println("Wrote changes to EEPROM");
}
//----------------------------------------------------------------------------- EEPROM RESET

void setup() {
  EEPROM.begin(512); //allocate memory address 0-1024 for EEPROM
  Serial.println("Configured EEPROM for addresses 0-512");
  Serial.begin(115200); //begin serial
  Serial.println("Lexzach's Low-Cost FACP");
  Serial.println("Booting...");

//----------------------------------------------------------------------------- SETUP PINS
  pinMode(resetButtonPin, INPUT); //reset switch
  pinMode(hornRelay, OUTPUT); //horn
  pinMode(strobeRelay, OUTPUT); //strobe
  pinMode(smokeDetectorRelay, OUTPUT); //smoke relay
  pinMode(readyLed, OUTPUT); //ready LED
  pinMode(silenceLed, OUTPUT); //silence LED
  pinMode(alarmLed, OUTPUT); //alarm LED
  pinMode(keySwitchPin, INPUT); //key switch
  pinMode(silenceButtonPin, INPUT); //silence switch
  pinMode(drillButtonPin, INPUT); //drill switch
  if (digitalRead(resetButtonPin) and not digitalRead(silenceButtonPin) and not digitalRead(drillButtonPin)){
    failsafeMode = true;
  }
  
  pinMode(zone1Pin, INPUT); //zone 1
  pinMode(zone2Pin, INPUT); //zone 2

  pinMode(buzzerPin, OUTPUT); //buzzer

  if (not invertRelay){
    digitalWrite(hornRelay, HIGH); //horn
    digitalWrite(strobeRelay, HIGH); //strobe
    digitalWrite(smokeDetectorRelay, HIGH); //smoke relay
  } else {
    digitalWrite(hornRelay, LOW); //horn
    digitalWrite(strobeRelay, LOW); //strobe
    digitalWrite(smokeDetectorRelay, LOW); //smoke relay
  }
  
  pinMode(sclPin, OUTPUT); //scl
  pinMode(sdaPin, OUTPUT); //sda
//----------------------------------------------------------------------------- SETUP PINS

  Serial.println("Initializing LCD...");
  lcd.init(); //initialize LCD
  lcd.createChar(0, lock); //create the lock character
  lcd.createChar(1, check); //create the lock character
  lcd.createChar(2, cross); //create the lock character
  

  lcd.backlight();

//----------------------------------------------------------------------------- EEPROM RESET BUTTONS
  if (digitalRead(resetButtonPin) and digitalRead(silenceButtonPin) and digitalRead(drillButtonPin) and not failsafeMode){ //reset EEPROM if all buttons are pressed
    for(int i=5; i!=0; i--){
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("TO FACTORY RESET");
      lcd.setCursor(0,1);
      lcd.print((String)"HOLD BUTTONS - "+i);
      delay(1000);
    }
    lcd.clear();
    lcd.setCursor(0,0);
    if (digitalRead(resetButtonPin) and digitalRead(silenceButtonPin) and digitalRead(drillButtonPin)){
      lcd.print("RESETTING TO");
      lcd.setCursor(0,1);
      lcd.print("FACTORY SETTINGS");
      resetEEPROM();
    } else {
      lcd.print("FACTORY RESET");
      lcd.setCursor(0,1);
      lcd.print("CANCELLED");
    }
    delay(3000);
    ESP.restart();
  }
//----------------------------------------------------------------------------- EEPROM RESET BUTTONS

  // if (digitalRead(silenceButtonPin)==HIGH){ //debug code
  //   debug = true;
  // }

  lcd.clear();
  lcd.setCursor(4,0);
  lcd.print("BOOTING...");
  lcd.setCursor(0,1);
  lcd.print("IO");

  Serial.println("Configured all pins");

  //EEPROM.write(0,255); //-------------------------------------------------------UNCOMMENT TO INVALIDATE EEPROM AND REFLASH IT AFTER EVERY RESTART
  //EEPROM.commit();

  Serial.println("Verifying EEPROM configuration integrity...");
//----------------------------------------------------------------------------- EEPROM INTEGRETY  
  if ((EEPROM.read(0) != 76 or EEPROM.read(6) != 104 or EEPROM.read(7) > 5 or EEPROM.read(8) > 1 or EEPROM.read(50) != EEPROMVersion or EEPROM.read(51) != EEPROMBuild) and not failsafeMode){ //completely skip eeprom verification if booting into failsafe
    Serial.println("EEPROM verification failed.");
    lcd.clear();
    lcd.setCursor(0,0);
    if (EEPROM.read(50) != EEPROMVersion or EEPROM.read(51) != EEPROMBuild){ //display error 2 if the firmware is different
      lcd.print("ERROR 2");
    } else {
      lcd.print("ERROR 1");
    }
    lcd.setCursor(0,1); 
    lcd.print("check manual");
    while(not digitalRead(resetButtonPin) and not digitalRead(drillButtonPin)){ //wait until button is pressed
      delay(1);
    }
    if(digitalRead(resetButtonPin)){
      failsafeMode = true; // ----- ENTER FAILSAFE MODE
    } else if (digitalRead(drillButtonPin)){
      resetEEPROM();
    }
  } else {
    Serial.println("EEPROM integrity verified");
  }
//----------------------------------------------------------------------------- EEPROM INTEGRETY  
  if (not failsafeMode){ //continue loading if failsafe mode is not enabled
    lcd.clear();
    lcd.setCursor(4,0);
    lcd.print("BOOTING...");
    lcd.setCursor(0,1);
    lcd.print("IO-SLFTST");

    Serial.println("Current EEPROM dump:"); //dump EEPROM into serial monitor
    for (int i=0; i<=511; i++){
      Serial.print(i);
      Serial.print(":");
      Serial.print(EEPROM.read(i));
      Serial.print(" ");
    }
    Serial.println();

    //CONFIG LOADING ROUTINE
    Serial.println("Loading config from EEPROM...");
    codeWheel = EEPROM.read(7); //codeWheel setting address
  //----------------------------- Panel security variables
    keyRequired = EEPROM.read(8) == 1 ? true : false;
    keyRequiredVisual = keyRequired ? true : false;
  //----------------------------- Panel security variables
    verificationEnabled = EEPROM.read(9) == 1 ? true : false;
    eolResistor = EEPROM.read(73) == 1 ? true : false;
    preAlarm = EEPROM.read(74) == 1 ? true : false;
    smokeDetectorVerification = EEPROM.read(76) == 1 ? true : false;
    audibleSilence = EEPROM.read(79) == 1 ? true : false;
    keylessSilence = EEPROM.read(27) == 1 ? true : false;
    useTwoWire = EEPROM.read(30) == 1 ? true : false;
    smokeDetectorTimeout = EEPROM.read(77)*5000;
    smokeDetectorPostRestartVerificationTime = EEPROM.read(28)*5000;
    firstStageTime = EEPROM.read(75)*60000;
    verificationTime = EEPROM.read(10)*100;
    //resistorLenience = EEPROM.read(72)*4; DEPRECATED
    panelHomescreen = EEPROM.read(78);
    lcdTimeout = EEPROM.read(80)*15000;
    strobeSync = EEPROM.read(29);
    byte byte_array[4];
    for (int i = 0; i < 4; i++) {
      byte_array[i] = EEPROM.read(81+i); //read each byte from consecutive addresses
      powerOnMinutes = powerOnMinutes | ((unsigned long)byte_array[i] << (i * 8)); //reconstruct the number from the bytes
    }
    int x=0;
    for (int i=11; i<=26; i++){ //read panel name
      if (EEPROM.read(i) != 0){
        panelName = panelName + (char)EEPROM.read(i);
        panelNameList[x] = EEPROM.read(i);
        x++;
      }
    }
    lcd.clear();
    lcd.setCursor(4,0);
    lcd.print("BOOTING...");
    lcd.setCursor(0,1);
    lcd.print("IO-SLFTST-CONFIG");
    delay(100);
    Serial.println("Config loaded");
    digitalWrite(readyLed, HIGH); //power on ready LED on startup
    updateLockStatus = true;
    panelUnlocked = (digitalRead(keySwitchPin) and keyRequired) ? true : false; //check the key status on startup
    digitalWrite(silenceLed, LOW);
    digitalWrite(alarmLed, LOW);
    if (invertRelay){
      digitalWrite(smokeDetectorRelay, HIGH); //turn on smoke relay
    } else {
      digitalWrite(smokeDetectorRelay, LOW); //turn on smoke relay
    }
    Serial.println("-=- STARTUP COMPLETE -=-");
  } else {
    Serial.println("-=- ENTERING FAILSAFE MODE -=-");
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("ENTERING");
    lcd.setCursor(0,1);
    lcd.print("FAILSAFE...");
    delay(3000);
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("FAILSAFE MODE");
    lcd.setCursor(0,1);
    lcd.print("SYSTEM NORMAL");
    digitalWrite(readyLed, HIGH);
    if (invertRelay){
      digitalWrite(smokeDetectorRelay, HIGH); //turn on smoke relay
    } else {
      digitalWrite(smokeDetectorRelay, LOW); //turn on smoke relay
    }
  }
  lastPulse = millis(); //start last pulse
}


//---------------------------------------------------------------------------------------- functions for turning certain things on and off
void tone() {
  ledcSetup(0, 5000, 8); // setup beeper
  ledcAttachPin(buzzerPin, 0); // attach beeper
  ledcWriteTone(0, 1500); // play tone
}
void noTone() {
  ledcSetup(0, 5000, 8); // setup beeper
  ledcAttachPin(buzzerPin, 0); // attach beeper
  ledcWriteTone(0, 0); // stop tone
}
void hornOn(bool on){
  digitalWrite(hornRelay, (invertRelay ^ on) ? LOW : HIGH);
}
void strobeOn(bool on){
  digitalWrite(strobeRelay, (invertRelay ^ on) ? LOW : HIGH);
}
void smokeDetectorOn(bool on){
  digitalWrite(smokeDetectorRelay, (invertRelay ^ on) ? LOW : HIGH);
}
//---------------------------------------------------------------------------------------- functions for turning certain things on and off

void activateNAC(){
  horn = true;
  strobe = true;
  fullAlarm = true;
  silenced = false;
  inConfigMenu = false;
  codeWheelTimer = 0;
  strobeSyncTimer = 0;
  secondStage = (zoneAlarm == 4 or not preAlarm) ? true : false; //entirely skip first stage if it is a drill or if prealarm is turned off
  tone();
  digitalWrite(readyLed, HIGH);
  digitalWrite(alarmLed, HIGH);
  digitalWrite(silenceLed, LOW);
}

void checkKey(){
  if (digitalRead(keySwitchPin)){
    if (not panelUnlocked and keyRequired){
      panelUnlocked = true;
      updateLockStatus = true;
    }
  } else {
    if (panelUnlocked and keyRequired){
      panelUnlocked = false;
      drillPressed=false;
      silencePressed=false; //make sure all buttons are registered as depressed after key is removed
      resetPressed=false;
      drillStillPressed=false;
      resetStillPressed=false;
      silenceStillPressed=false;
      drillTimer = 0;
      updateLockStatus = true;
    }
  }
}

//----------------------------------------------------------------------------- CHECK ACTIVATION DEVICES [!THIS CODE MUST WORK!]
void checkDevices(){
  if (not walkTest){
    if (not runVerification and not fullAlarm and (analogRead(zone1Pin) == 0 or analogRead(zone2Pin) == 0)){ //reading a single zero flags as a possible alarm
      runVerification = true;
    } 

    if (runVerification and not fullAlarm){ //only execute if the panel flags a possible alarm and there isn't currently a full alarm
      if (verificationTimer >= verificationTime or not verificationEnabled){ //execute the following code if verification surpasses the configured verification time OR verification is disabled
        if (analogRead(zone1Pin) == 0 or analogRead(zone2Pin) == 0){ //check once again if any zeros are read
          if (not smokeDetectorVerification or smokeDetectorCurrentlyInVerification){ //if smoke detector verification is disabled, or a smoke detector has tripped *after* verification, activate NACs
            if (analogRead(zone1Pin) == 0 and analogRead(zone2Pin) == 0){ //read the pins once more to check which zone is in alarm
              zoneAlarm = 3; //both
            } else if (analogRead(zone2Pin) == 0){
              zoneAlarm = 2; //z2
            } else {
              zoneAlarm = 1; //z1
            }
            activateNAC();
            runVerification = false; //dismiss the possible alarm after activating the NACs
            verificationTimer = 0;
          } else if (smokeDetectorVerification and not smokeDetectorCurrentlyInVerification){ //if smoke detector verifcaion is turned on, run this instead
            smokeDetectorOn(false); //turn off the smoke detector relay
            delay(100); //wait for 100 ms
            if (analogRead(zone1Pin) == 0 or analogRead(zone2Pin) == 0){ // if the alarm signal persists after turning off the smoke detectors, it is a pull station, activate the nacs
              if (analogRead(zone1Pin) == 0 and analogRead(zone2Pin) == 0){
                zoneAlarm = 3; //both
              } else if (analogRead(zone2Pin) == 0){
                zoneAlarm = 2; //z2
              } else {
                zoneAlarm = 1; //z1
              }
              activateNAC();
              smokeDetectorOn(true); //re-enable the smoke detector relay after determining that a pull station was pulled.
              runVerification = false;
              verificationTimer = 0;
            } else { //if the signal *does not* persists after disabling the smoke detector relay, it was a smoke detector, run verification.
              smokeDetectorPostRestartTimer = 0;
              smokeDetectorCurrentlyInVerification = true; //tell the smokeDetector() function to run code
              smokeDetectorTimer = 0; //reset the smoke detector timer
              currentScreen = -1; //update the screen to allow the displaying of smoke detector verification
              digitalWrite(alarmLed, HIGH); //LED indicator 
              runVerification = false;
              verificationTimer = 0;
            }
          }
        } else { //if no zeros are read after verification, dismiss possible alarm
          runVerification = false;
          verificationTimer = 0;
        }
      } else {
        verificationTimer++;
      }
    }
  } else if (walkTest){
    if (analogRead(zone1Pin) == 0 or analogRead(zone2Pin) == 0){// or analogRead(zone2Pin) == 0){
      if (analogRead(zone1Pin) == 0){
        zone1Count++;
        smokeDetectorOn(false);
        strobeOn(true);
        delay(500);
        digitalWrite(alarmLed, HIGH);
        if (not silentWalkTest){
          hornOn(true);
          delay(500);
          hornOn(false);
        } else {
          delay(500);
        }
        delay(3000);
        digitalWrite(alarmLed, LOW);
        strobeOn(false);
        delay(4000);
        smokeDetectorOn(true);
      } else if (analogRead(zone2Pin) == 0){
        zone2Count++;
        smokeDetectorOn(false);
        strobeOn(true);
        delay(500);
        digitalWrite(alarmLed, HIGH);
        if (not silentWalkTest){
          hornOn(true);
          delay(500);
          hornOn(false);
          delay(500);
          hornOn(true);
          delay(500);
          hornOn(false);
        } else {
          delay(1500);
        }
        delay(3000);
        digitalWrite(alarmLed, LOW);
        strobeOn(false);
        delay(4000);
        smokeDetectorOn(true);
      }
      currentScreen = -1;
    }
  }

  if ((analogRead(zone1Pin) == 4095 or analogRead(zone2Pin) == 4095) and eolResistor) {
    if (troubleTimer >= 1000){
      trouble = true;
      troubleType=1;
      if (analogRead(zone1Pin) == 4095 and analogRead(zone2Pin) == 4095){
        zoneTrouble = 3;
      } else if (analogRead(zone1Pin) == 4095){
        zoneTrouble = 1;
      } else {
        zoneTrouble = 2;
      }
    } else {
      troubleTimer++;
    }
  } else {
    troubleTimer = 0;
  }
}
//----------------------------------------------------------------------------- CHECK ACTIVATION DEVICES [!THIS CODE MUST WORK!]


//----------------------------------------------------------------------------- TROUBLE RESPONSE
void troubleCheck(){
  if (trouble and not fullAlarm and not walkTest){
    if (troubleLedTimer >= 200){
      if (digitalRead(readyLed)){
        digitalWrite(readyLed, LOW);
        if (not troubleAcked){
          noTone();
        }
      } else {
        digitalWrite(readyLed, HIGH);
        if (not troubleAcked){
          tone();
        }
      }
      troubleLedTimer = 0;
    } else {
      troubleLedTimer++;
    }

  } else {
    troubleLedTimer=0;
    zoneTrouble=0;
  }
}
//----------------------------------------------------------------------------- TROUBLE RESPONSE


//----------------------------------------------------------------------------- RESET CODE
void reboot(){
  lcd.clear();
    lcd.setCursor(2,0);
    lcd.print("Resetting...");
    tone();
    digitalWrite(readyLed, HIGH); //ready LED
    digitalWrite(silenceLed, HIGH); //silence LED
    digitalWrite(alarmLed, HIGH); //alarm LED
    hornOn(false); //horn
    strobeOn(false); //strobe
    smokeDetectorOn(false); //smoke relay
    lcd.backlight();
    delay(2500);
    noTone();
    digitalWrite(readyLed, LOW); //ready LED
    digitalWrite(silenceLed, LOW); //silence LED
    digitalWrite(alarmLed, LOW); //alarm LED  
    ESP.restart();
}
//----------------------------------------------------------------------------- RESET CODE


//----------------------------------------------------------------------------- BUTTON CHECK
void checkButtons(){
  if (digitalRead(resetButtonPin)){ //-----------------------------------------------------------RESET BUTTON
    reboot();
  }

  if (digitalRead(silenceButtonPin)){ //---------------------------------------------------------SILENCE BUTTON
    if (horn){ //if horns are not silenced, silence the horns
      digitalWrite(silenceLed, HIGH);
      digitalWrite(alarmLed, LOW);
      digitalWrite(readyLed, LOW);
      horn = false;
      if (not audibleSilence){
        strobe = false;
      }
      silenced=true;
      noTone();
    } else if (not fullAlarm and trouble and not silencePressed and not troubleAcked){
      troubleAcked = true;
      noTone();
    } else if (not fullAlarm and not silencePressed and not inConfigMenu){
      inConfigMenu = true;
      resetStillPressed = true; //make sure the menu doesn't close out as soon as someone opens it
      silenceStillPressed = true;
      drillStillPressed = true;
      char *main[] = {"Testing","Settings"}; //menu   0
      configTop = (String)main[0];
      configBottom = (String)main[1];
      configPage = 0;
      cursorPosition = 0;
      currentConfigBottom = "";
      currentConfigTop = "";
    }
    silencePressed = true;
  } else {
    silencePressed = false;
  }

  if (digitalRead(drillButtonPin) and not horn and not silenced and not fullAlarm){ //------------------------------------------DRILL BUTTON
    if (drillTimer >= 237){
      zoneAlarm = 4;
      activateNAC();
    } else {
      drillTimer++;
    }
    drillPressed = true;
  } else {
    drillTimer = 0;
    drillPressed = false;
  }
  if (digitalRead(drillButtonPin) and fullAlarm and not secondStage){
    secondStage = true;
    currentScreen = -1;
  }
}
//----------------------------------------------------------------------------- BUTTON CHECK


//----------------------------------------------------------------------------- NAC ACTIVATION
void alarm(){
  if (strobe){
    if (strobeSync == 0){ //add more strobesyncs in the future
      strobeOn(true);
    } else {
      strobeSyncTimer++;
      if (strobeSync == 1 or strobeSync == 4){
        if (strobeSyncTimer >= 10000){
          strobeSyncTimer=0;
        } else if (strobeSyncTimer >= 9940){
          strobeOn(false);
        } else if (strobeSyncTimer <= 60){
          strobeOn(true);
        }
      } else if (strobeSync == 2) {
        if (strobeSyncTimer >= 2000){
          strobeSyncTimer=0;
        } else if (strobeSyncTimer >= 1968){
          strobeOn(false);
        } else if (strobeSyncTimer <= 32){
          strobeOn(true);
        }
      } else if (strobeSync == 3) {
        if (strobeSyncTimer >= 2000){
          strobeSyncTimer=0;
        } else if (strobeSyncTimer >= 1985){
          strobeOn(false);
        } else if (strobeSyncTimer <= 15){
          strobeOn(true);
        }
      } 
    }
  }else{
    strobeOn(false);
    strobeSyncTimer = 0;
  }
  if (horn and not useTwoWire){
    if (not preAlarm or secondStage){ //yes, preAlarm == false is redundant but in the case that second stage == false, but pre-alarm is off, the full alarm will still sound
      if (codeWheel == 0){

        if (codeWheelTimer == 0){ //---------- temporal code 3 
          hornOn(true);
        } else if (codeWheelTimer == 500) {
          hornOn(false);
        } else if (codeWheelTimer == 1000) {
          hornOn(true);
        } else if (codeWheelTimer == 1500) {
          hornOn(false);
        } else if (codeWheelTimer == 2000) {
          hornOn(true);
        } else if (codeWheelTimer == 2500) {
          hornOn(false);
        } else if (codeWheelTimer >= 4000) {
          codeWheelTimer = -1;
        }

        
      } else if (codeWheel == 1) {

        if (codeWheelTimer == 0){ //---------- marchtime
          hornOn(true);
        } else if (codeWheelTimer == 250){
          hornOn(false);
        } else if (codeWheelTimer >= 500){
          codeWheelTimer = -1;
        }

      } else if (codeWheel == 2) { //--------- 4-4
        
        if (codeWheelTimer == 0) {
          hornOn(true);
        } else if (codeWheelTimer == 300) {
          hornOn(false);
        } else if (codeWheelTimer == 600) {
          hornOn(true);
        } else if (codeWheelTimer == 900) {
          hornOn(false);
        } else if (codeWheelTimer == 1200) {
          hornOn(true);
        } else if (codeWheelTimer == 1500) {
          hornOn(false);
        } else if (codeWheelTimer == 1800) {
          hornOn(true);
        } else if (codeWheelTimer == 2100) {
          hornOn(false);

        } else if (codeWheelTimer == 2850) {
          hornOn(true);
        } else if (codeWheelTimer == 3150) {
          hornOn(false);
        } else if (codeWheelTimer == 3450) {
          hornOn(true);
        } else if (codeWheelTimer == 3750) {
          hornOn(false);
        } else if (codeWheelTimer == 4050) {
          hornOn(true);
        } else if (codeWheelTimer == 4350) {
          hornOn(false);
        } else if (codeWheelTimer == 4650) {
          hornOn(true);
        } else if (codeWheelTimer == 4950) {
          hornOn(false);
        } else if (codeWheelTimer >= 14950) {
          codeWheelTimer = -1;
        }
      
      } else if (codeWheel == 3) { //--------- continuous
        hornOn(true);
      } else if (codeWheel == 5) {
        if (codeWheelTimer == 0){ //---------- marchtime slower
          hornOn(true);
        } else if (codeWheelTimer == 500){
          hornOn(false);
        } else if (codeWheelTimer >= 1000){
          codeWheelTimer = -1;
        }

      } else if (codeWheel == 4) {
        if (codeWheelTimer == 0){ //---------- california code
          hornOn(true);
        } else if (codeWheelTimer == 10000){
          hornOn(false);
        } else if (codeWheelTimer >= 15000){
          codeWheelTimer = -1;
        }

      }

      codeWheelTimer++;
    } else if (preAlarm and not secondStage){
      if (codeWheelTimer == 0){
        hornOn(true);
      } else if (codeWheelTimer == 75){
        hornOn(false);
      } else if (codeWheelTimer >= 5075){
        codeWheelTimer = -1;
      }
      
      codeWheelTimer++;
      firstStageTimer++;
      if (firstStageTimer>=firstStageTime){
        codeWheelTimer = 0;
        secondStage = true;
        currentScreen = -1;
      }
    }
  } else if (useTwoWire){       //-------------------------------- DO MORE TESTING WITH THIS!!!!
    if (horn and (not preAlarm or secondStage)){
      if (twoWireTimer >= 1000){
        twoWireTimer = 0;
      } else if (twoWireTimer >= 975){
        hornOn(false);
      } else if (twoWireTimer <= 10){
        hornOn(true);
      }
      twoWireTimer++;
    } else if (not horn and silenced and audibleSilence){
      if (twoWireTimer >= 1000){
        twoWireTimer = 0;
      } else if (twoWireTimer >= 965){
        hornOn(false);
      } else if (twoWireTimer <= 10){
        hornOn(true);
      }
      twoWireTimer++;
    } else if (horn and preAlarm and not secondStage){
      if (codeWheelTimer == 0){
        hornOn(true);
      } else if (codeWheelTimer == 75){
        hornOn(false);
      } else if (codeWheelTimer >= 5075){
        codeWheelTimer = -1;
      }
      
      codeWheelTimer++;
      firstStageTimer++;
      if (firstStageTimer>=firstStageTime){
        codeWheelTimer = 0;
        secondStage = true;
        currentScreen = -1;
      }
    }
  } else {
    hornOn(false);
    codeWheelTimer = 0;
    twoWireTimer = 0;
  }
  if (fullAlarm){
    alarmLedTimer++;
    if (alarmLedTimer >= 750){
      if (not digitalRead(alarmLed)){
        if (silenced){
          tone();
        }
        digitalWrite(alarmLed, HIGH);
      } else {
        if (silenced){
          noTone();
        }
        digitalWrite(alarmLed, LOW);
      }
      alarmLedTimer = 0;
    }
  }
}
//----------------------------------------------------------------------------- NAC ACTIVATION

void lcdUpdate(){
  if (not trouble and not fullAlarm and not walkTest and currentScreen != 0 and drillTimer == 0){
    lcd.noAutoscroll();
    lcd.clear();
    lcd.setCursor(2,0);
    if (smokeDetectorCurrentlyInVerification){
      lcd.print("Smoke Verif.");
    } else {
      lcd.print("System Normal");
    }
    lcd.setCursor(0,1);
    if (panelHomescreen == 0){
      lcd.print(panelName);
    } else if (panelHomescreen == 1){
      lcd.print(analogRead(zone1Pin));
    }
    currentScreen = 0;
    updateLockStatus = true;
  }  else if (fullAlarm and not silenced and currentScreen != 3){
    lcd.clear();
    lcd.setCursor(1,0);
    if (secondStage){ //print pre-alarm if it is first stage
      lcd.print("* FIRE ALARM *");
    } else {
      lcd.print("* PRE ALARM *");
    }
    lcd.setCursor(0,1);
    if (zoneAlarm == 1){
      lcd.print("Zone 1");
    } else if (zoneAlarm == 2){
      lcd.print("Zone 2");
    } else if (zoneAlarm == 3){
      lcd.print("Zone 1 & Zone 2");
    } else if (zoneAlarm == 4){
      lcd.print("Fire Drill");
    }
    currentScreen = 3;
    updateLockStatus = true;
  } else if (silenced and currentScreen != 4){
    lcd.clear();
    lcd.setCursor(1,0);
    lcd.print("-- SILENCED --");
    lcd.setCursor(0,1);
    if (zoneAlarm == 1){
      lcd.print("Zone 1");
    } else if (zoneAlarm == 2){
      lcd.print("Zone 2");
    } else if (zoneAlarm == 3){
      lcd.print("Zone 1 & Zone 2");
    } else if (zoneAlarm == 4){
      lcd.print("Fire Drill");
    }
    currentScreen = 4;
    updateLockStatus = true;
  } else if (walkTest and currentScreen != 5) {
    lcd.clear();
    lcd.setCursor(1,0);
    lcd.print("* Supervisory *");
    lcd.setCursor(0,1);
    lcd.print("Z1:"+(String)zone1Count+" Z2:"+(String)zone2Count);
    currentScreen = 5;
    updateLockStatus = true;
    digitalWrite(readyLed, LOW); //ready led off for walk test
  } else if (drillPressed and not fullAlarm and not walkTest and currentScreen != 6) {
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("CONTINUE HOLDING");
    lcd.setCursor(1,1);
    lcd.print("TO START DRILL");
    currentScreen = 6;
  } else if (trouble and not fullAlarm and not drillPressed and not walkTest and currentScreen != 1){
    lcd.clear();
    lcd.setCursor(2,0);
    lcd.print("* Trouble *");
    if (troubleType == 0){
      lcd.setCursor(0,1);
      lcd.print("Unknown");
    } else if (troubleType == 1){
      lcd.setCursor(0,1);
      if (zoneTrouble == 3){
        lcd.print("Gnd Fault Z1&Z2");
      } else if (zoneTrouble == 2){
        lcd.print("Gnd Fault Z2");
      } else if (zoneTrouble == 1){
        lcd.print("Gnd Fault Z1");
      }
    }
    currentScreen = 1;
    updateLockStatus = true;
  }
  if (updateLockStatus and not inConfigMenu and keyRequired){
    lcd.setCursor(0,0);
    if (panelUnlocked){
      lcd.print(" ");  
    } else {
      lcd.write(byte(0));
    }
    updateLockStatus = false;
  }
}

void configLCDUpdate(int cursor, String top, String bottom, bool replaceTop = false, bool replaceBottom = false){ // new cursor position, top lcd text, bottom lcd text, does the top text need a check or cross, does the bottom text need a check or cross
  configTop = top;
  configBottom = bottom;
  cursorPosition = cursor;

  if (replaceTop){
    configTop.replace("1","*");
    configTop.replace("0","$");
  }
  if (replaceBottom){
    configBottom.replace("1","*");
    configBottom.replace("0","$");
  }
}

void config(){
  const char *main[] = {"Testing","Settings"}; //menu 0
  const char *mainTesting[] = {"Walk Test","Silent Wlk Test","Strobe Test"}; //menu 1
  const char *mainSettings[] = {"Fire Alarm","Panel"}; //menu 2
  const char *mainSettingsFireAlarmSettings[] = {"Coding","Verification","Pre-Alarm","Audible Sil.:","No-Key Sil.:","Strobe Sync","2 Wire:"}; //menu 3
  const char *mainSettingsVerificationSettings[] = {"Verification:","V.Time:","Det.Verif.:","Det.Timeout:","Det.Watch:"}; //menu 4
  const char *mainSettingsFireAlarmSettingsCoding[] = {"Temporal Three","Marchtime","4-4","Continuous","California","Slow Marchtime"}; //menu 5
  const char *mainSettingsFireAlarmSettingsPreAlarmSettings[] = {"Pre-Alarm:","Stage1 Time:"}; //menu 6
  const char *mainPanelSettings[] = {"Panel Name","Panel Security","LCD Dim:","Factory Reset","About"}; //menu 8
  const char *mainPanelSettingsPanelSecurity[] = {"None","Keyswitch","Passcode"}; //menu 9
  const char *mainPanelSettingsPanelName[] = {"Enter Name:"}; //menu 10
  const char *mainSettingsFireAlarmSettingsStrobeSync[] = {"None","System Sensor","Wheelock", "Gentex","Simplex"}; //menu 11
  const char *mainPanelSettingsAbout[] = {"Antigneous FACP","Nightly ","Made by Lexzach","Hrs On: "}; //menu 12
  
  if (digitalRead(resetButtonPin)){ //RESET BUTTON
    resetPressed = true;
  } else {
    resetPressed = false;
    resetStillPressed = false;
  }
  if (digitalRead(silenceButtonPin)){ //SILENCE BUTTON
    silencePressed = true;
  } else {
    silencePressed = false;
    silenceStillPressed = false;
  }
  if (digitalRead(drillButtonPin)){ //DRILL BUTTON
    drillPressed = true;
  } else {
    drillPressed = false;
    drillStillPressed = false;
  }
//----------------------------------------------------------------------------- MAIN MENU
  if (configPage == 0){
    if (resetPressed and not resetStillPressed){
      if (cursorPosition == 0){ //main screen
        configLCDUpdate(1, (String)main[1], (String)main[0]);
      } else if (cursorPosition == 1){
        configLCDUpdate(0, (String)main[0], (String)main[1]);
      }
    } else if (silencePressed and not silenceStillPressed){
      silencePressed = true;
      inConfigMenu = false;
      currentScreen=-1;
    } else if (drillPressed and not drillStillPressed){
      if (cursorPosition == 0){ //cursor over testing
        configPage = 1; //change screen to testing
        configLCDUpdate(0, (String)mainTesting[0], (String)mainTesting[1]);
      } else if (cursorPosition == 1){ //cursor over settings
        configPage = 2; //change screen to settings
        configLCDUpdate(0, (String)mainSettings[0], (String)mainSettings[1]);
      }
    }
//----------------------------------------------------------------------------- MAIN MENU

//----------------------------------------------------------------------------- TESTING
  } else if (configPage == 1){
    if (resetPressed and not resetStillPressed and not strobe){
      if (cursorPosition == 0){
        configLCDUpdate(1, (String)mainTesting[1], (String)mainTesting[2]);
      } else if (cursorPosition == 1) {
        configLCDUpdate(2, (String)mainTesting[2], (String)mainTesting[0]);
      } else if (cursorPosition == 2) {
        configLCDUpdate(0, (String)mainTesting[0], (String)mainTesting[1]);
      }
    } else if (silencePressed and not silenceStillPressed){
      configPage = 0;
      configLCDUpdate(0, (String)main[0], (String)main[1]);
      strobe = false;
      smokeDetectorOn(true);
    } else if (drillPressed and not drillStillPressed){
        if (cursorPosition == 0){
          walkTest = true;
          silentWalkTest = false;
          silencePressed = true;
          inConfigMenu = false;
          currentScreen=-1;
          zone1Count = 0;
          zone2Count = 0;
        } else if (cursorPosition == 1) {
          walkTest = true;
          silentWalkTest = true;
          silencePressed = true;
          inConfigMenu = false;
          currentScreen=-1;
          zone1Count = 0;
          zone2Count = 0;
        } else if (cursorPosition == 2) {
          if (not strobe){
            strobe = true;
            smokeDetectorOn(false); //prevent (specifically cheap IR) smoke detectors from tripping from the strobe 
            configLCDUpdate(2, (String)mainTesting[2]+" *", (String)mainTesting[0]);
          } else {
            strobe = false;
            smokeDetectorOn(true);
            configLCDUpdate(2, (String)mainTesting[2], (String)mainTesting[0]);
          }
        }
    }
//----------------------------------------------------------------------------- TESTING

//----------------------------------------------------------------------------- SETTINGS
  } else if (configPage == 2){
    if (resetPressed and not resetStillPressed){
      if (cursorPosition == 0){ //main screen
        configLCDUpdate(1, (String)mainSettings[1], (String)mainSettings[0]);
      } else if (cursorPosition == 1){
        configLCDUpdate(0, (String)mainSettings[0], (String)mainSettings[1]);
      }
    } else if (silencePressed and not silenceStillPressed){
      configPage = 0;
      configLCDUpdate(1, (String)main[1], (String)main[0]);
    } else if (drillPressed and not drillStillPressed){
      if (cursorPosition == 0){
        configPage = 3; //change screen to facp settings
        if (useTwoWire){
          configLCDUpdate(0, (String)mainSettingsFireAlarmSettings[0] + " off", (String)mainSettingsFireAlarmSettings[1]);
        } else {
          configLCDUpdate(0, (String)mainSettingsFireAlarmSettings[0], (String)mainSettingsFireAlarmSettings[1]);
        }
      } else if (cursorPosition == 1){
        configPage = 8; //change screen to facp settings
        configLCDUpdate(0, (String)mainPanelSettings[0], (String)mainPanelSettings[1]);
      }
    }
//----------------------------------------------------------------------------- SETTINGS

//----------------------------------------------------------------------------- SETTINGS > FIRE ALARM
  } else if (configPage == 3){
    if (resetPressed and not resetStillPressed){
      if (cursorPosition == 0){
        configLCDUpdate(1, (String)mainSettingsFireAlarmSettings[1], (String)mainSettingsFireAlarmSettings[2]);
      } else if (cursorPosition == 1) {
        configLCDUpdate(2, (String)mainSettingsFireAlarmSettings[2], (String)mainSettingsFireAlarmSettings[3]+audibleSilence, false, true);
      } else if (cursorPosition == 2) {
        if (keyRequiredVisual){
          configLCDUpdate(3, (String)mainSettingsFireAlarmSettings[3]+audibleSilence, (String)mainSettingsFireAlarmSettings[4]+keylessSilence, true, true);
        } else {
          configLCDUpdate(3, (String)mainSettingsFireAlarmSettings[3]+audibleSilence, (String)mainSettingsFireAlarmSettings[4]+"off", true, false);
        }
      } else if (cursorPosition == 3){
        if (keyRequiredVisual){
          configLCDUpdate(4, (String)mainSettingsFireAlarmSettings[4]+keylessSilence, (String)mainSettingsFireAlarmSettings[5], true, false);
        } else {
          configLCDUpdate(4, (String)mainSettingsFireAlarmSettings[4]+"off", (String)mainSettingsFireAlarmSettings[5], false, false);
        }
      } else if (cursorPosition == 4) {
        configLCDUpdate(5, (String)mainSettingsFireAlarmSettings[5], (String)mainSettingsFireAlarmSettings[6]+useTwoWire, false, true);
      } else if (cursorPosition == 5){
        if (useTwoWire){
          configLCDUpdate(6, (String)mainSettingsFireAlarmSettings[6]+useTwoWire, (String)mainSettingsFireAlarmSettings[0]+"off", true, false);
        } else {
          configLCDUpdate(6, (String)mainSettingsFireAlarmSettings[6]+useTwoWire, (String)mainSettingsFireAlarmSettings[0], true, false);
        }
      } else if (cursorPosition == 6){
        if (useTwoWire){
          configLCDUpdate(0, (String)mainSettingsFireAlarmSettings[0] + " off", (String)mainSettingsFireAlarmSettings[1]);
        } else {
          configLCDUpdate(0, (String)mainSettingsFireAlarmSettings[0], (String)mainSettingsFireAlarmSettings[1]);
        }
      }
    } else if (silencePressed and not silenceStillPressed){
      configPage = 2;
      configLCDUpdate(0, (String)mainSettings[0], (String)mainSettings[1]);
    } else if (drillPressed and not drillStillPressed){
      if (cursorPosition == 0 and not useTwoWire){
        configPage = 5;
        if (codeWheel == 0){
          configLCDUpdate(0, (String)mainSettingsFireAlarmSettingsCoding[0] + "*", (String)mainSettingsFireAlarmSettingsCoding[1]);
        } else if (codeWheel == 1){
          configLCDUpdate(0, (String)mainSettingsFireAlarmSettingsCoding[0], (String)mainSettingsFireAlarmSettingsCoding[1] + "*");
        } else {
          configLCDUpdate(0, (String)mainSettingsFireAlarmSettingsCoding[0], (String)mainSettingsFireAlarmSettingsCoding[1]);
        }
      } else if (cursorPosition == 1) {
        configPage = 4;
        if (verificationEnabled){
          configLCDUpdate(0, (String)mainSettingsVerificationSettings[0] + verificationEnabled, (String)mainSettingsVerificationSettings[1] + (verificationTime/1000)+"s", true, false);
        } else {
          configLCDUpdate(0, (String)mainSettingsVerificationSettings[0] + verificationEnabled, (String)mainSettingsVerificationSettings[1] + "off", true, false);
        }
      } else if (cursorPosition == 2) {
        configPage = 6;
        if (preAlarm){
          configLCDUpdate(0, (String)mainSettingsFireAlarmSettingsPreAlarmSettings[0]+preAlarm, (String)mainSettingsFireAlarmSettingsPreAlarmSettings[1] + (firstStageTime/60000) + "m", true, false);
        } else {
          configLCDUpdate(0, (String)mainSettingsFireAlarmSettingsPreAlarmSettings[0]+preAlarm, (String)mainSettingsFireAlarmSettingsPreAlarmSettings[1] + "off", true, false);
        }
      } else if (cursorPosition == 3) {
        if (audibleSilence){
          audibleSilence = false;
          EEPROM.write(79,0);
        } else {
          audibleSilence = true;
          EEPROM.write(79,1);
        }
        EEPROM.commit();
        if (keyRequiredVisual){
          configLCDUpdate(cursorPosition, (String)mainSettingsFireAlarmSettings[3]+audibleSilence, (String)mainSettingsFireAlarmSettings[4]+keylessSilence, true, true);
        } else {
          configLCDUpdate(cursorPosition, (String)mainSettingsFireAlarmSettings[3]+audibleSilence, (String)mainSettingsFireAlarmSettings[4]+"off", true, false);
        }
      } else if (cursorPosition == 4 and keyRequiredVisual){
        if (keylessSilence){
          keylessSilence = false;
          EEPROM.write(27,0);
        } else {
          keylessSilence = true;
          EEPROM.write(27,1);
        }
        EEPROM.commit();
        configLCDUpdate(cursorPosition, (String)mainSettingsFireAlarmSettings[4]+keylessSilence, (String)mainSettingsFireAlarmSettings[5], true, false);
      } else if (cursorPosition == 5){
        configPage = 11;     
        if (strobeSync == 0){
          configLCDUpdate(0, (String)mainSettingsFireAlarmSettingsStrobeSync[0] + "*", (String)mainSettingsFireAlarmSettingsStrobeSync[1]);
        } else if (strobeSync == 1){
          configLCDUpdate(0, (String)mainSettingsFireAlarmSettingsStrobeSync[0], (String)mainSettingsFireAlarmSettingsStrobeSync[1] + "*");
        } else {
          configLCDUpdate(0, (String)mainSettingsFireAlarmSettingsStrobeSync[0], (String)mainSettingsFireAlarmSettingsStrobeSync[1]);
        }
      } else if (cursorPosition == 6){
        if (useTwoWire){
          useTwoWire = false;
          EEPROM.write(30,0);
        } else {
          useTwoWire = true;
          EEPROM.write(30,1);
        }
        EEPROM.commit();
        if (useTwoWire){
          configLCDUpdate(cursorPosition, (String)mainSettingsFireAlarmSettings[6]+useTwoWire, (String)mainSettingsFireAlarmSettings[0] + " off", true, false);
        } else {
          configLCDUpdate(cursorPosition, (String)mainSettingsFireAlarmSettings[6]+useTwoWire, (String)mainSettingsFireAlarmSettings[0], true, false);
        }
      }
    }
//----------------------------------------------------------------------------- SETTINGS > FIRE ALARM

//----------------------------------------------------------------------------- SETTINGS > PANEL
  } else if (configPage == 8){
    if (resetPressed and not resetStillPressed){
      if (cursorPosition == 0){
        if (lcdTimeout == 0){
          configLCDUpdate(1, (String)mainPanelSettings[1], (String)mainPanelSettings[2] + "off");
        } else if (lcdTimeout<=30000) {
          configLCDUpdate(1, (String)mainPanelSettings[1], (String)mainPanelSettings[2] + lcdTimeout/1000+"s");
        } else {
          configLCDUpdate(1, (String)mainPanelSettings[1], (String)mainPanelSettings[2] + lcdTimeout/60000+"m");
        }
      } else if (cursorPosition == 1) {
        cursorPosition = 2;
        if (lcdTimeout == 0){
          configLCDUpdate(2, (String)mainPanelSettings[2] + "off", (String)mainPanelSettings[3]);
        } else if (lcdTimeout<=30000) {
          configLCDUpdate(2, (String)mainPanelSettings[2] + lcdTimeout/1000+"s", (String)mainPanelSettings[3]);
        } else {
          configLCDUpdate(2, (String)mainPanelSettings[2] + lcdTimeout/60000+"m", (String)mainPanelSettings[3]);
        }

      } else if (cursorPosition == 2) {
        configLCDUpdate(3, (String)mainPanelSettings[3], (String)mainPanelSettings[4]);
      } else if (cursorPosition == 3) {
        configLCDUpdate(4, (String)mainPanelSettings[4], (String)mainPanelSettings[0]);
      } else if (cursorPosition == 4) {
        configLCDUpdate(0, (String)mainPanelSettings[0], (String)mainPanelSettings[1]);
      }
    } else if (silencePressed and not silenceStillPressed){
      configPage = 2;
      configLCDUpdate(1, (String)mainSettings[1], (String)mainSettings[0]);
    } else if (drillPressed and not drillStillPressed){
      if (cursorPosition == 0){
        configPage = 10;
        configLCDUpdate(0, "Enter Name:", (String)panelName);
        lcd.blink_on();

      } else if (cursorPosition == 1) {
        configPage = 9;
        if (keyRequiredVisual == true){
          configLCDUpdate(0, (String)mainPanelSettingsPanelSecurity[0], (String)mainPanelSettingsPanelSecurity[1]+"*");
        } else {
          configLCDUpdate(0, (String)mainPanelSettingsPanelSecurity[0]+"*", (String)mainPanelSettingsPanelSecurity[1]);
        }
      } else if (cursorPosition == 2) {
        if (lcdTimeout == 0){
          lcdTimeout = 15000;
          EEPROM.write(80,1);
        } else if (lcdTimeout == 15000){
          lcdTimeout = 30000;
          EEPROM.write(80,2);
        } else if (lcdTimeout == 30000){
          lcdTimeout = 60000;
          EEPROM.write(80,4);
        } else if (lcdTimeout == 60000){
          lcdTimeout = 300000;
          EEPROM.write(80,20);
        } else if (lcdTimeout == 300000){
          lcdTimeout = 600000;
          EEPROM.write(80,40);
        } else if (lcdTimeout >= 600000){
          lcdTimeout = 0;
          EEPROM.write(80,0);
        }
        EEPROM.commit();
        if (lcdTimeout == 0){
          configLCDUpdate(cursorPosition, (String)mainPanelSettings[2] + "off", (String)mainPanelSettings[3]);
        } else if (lcdTimeout<=30000) {
          configLCDUpdate(cursorPosition, (String)mainPanelSettings[2] + lcdTimeout/1000+"s", (String)mainPanelSettings[3]);
        } else {
          configLCDUpdate(cursorPosition, (String)mainPanelSettings[2] + lcdTimeout/60000+"m", (String)mainPanelSettings[3]);
        }
      } else if (cursorPosition == 3){
        configLCDUpdate(0, "reset = yes", "silence = no");
        configPage = -1;
      } else if (cursorPosition == 4){
        configPage = 12;
        configLCDUpdate(0, (String)mainPanelSettingsAbout[0], (String)mainPanelSettingsAbout[1] + firmwareRev);
      }
    }
//----------------------------------------------------------------------------- SETTINGS > PANEL

//----------------------------------------------------------------------------- SETTINGS > PANEL > PANEL NAME
  } else if (configPage == 10){ //panel rename routine 
    
    if (resetPressed and not resetStillPressed){
      clearTimer = 0;
      if (panelNameList[cursorPosition] == 90){
        panelNameList[cursorPosition] = 32;
      } else if (panelNameList[cursorPosition] == 32){
        panelNameList[cursorPosition] = 39;
      } else if (panelNameList[cursorPosition] == 39){
        panelNameList[cursorPosition] = 45;
      } else if (panelNameList[cursorPosition] == 57){
        panelNameList[cursorPosition] = 65;
      } else {
        panelNameList[cursorPosition] = panelNameList[cursorPosition] + 1;
      }
    } else if (resetPressed and resetStillPressed) {
      clearTimer++;
      if (clearTimer >= 35) { //clear character if held down
        panelNameList[cursorPosition] = 32;
      }
    } else if (silencePressed and not silenceStillPressed){
      int x=0;
      for (int i=11; i<=26; i++){ //write new panel name
        EEPROM.write(i,panelNameList[x]);
        x++;
      }
      lcd.blink_off();
      EEPROM.commit();
      configPage = 8;
      configLCDUpdate(0, (String)mainPanelSettings[0], (String)mainPanelSettings[1]);
      x=0;
      panelName = "";
      for (int i=11; i<=26; i++){ //read panel name
        if (EEPROM.read(i) != 0){
          panelName = panelName + (char)EEPROM.read(i);
          panelNameList[x] = EEPROM.read(i);
          x++;
        }
      }
    } else if (drillPressed and not drillStillPressed){
      currentConfigTop = "e"; //make sure the screen re-renders
      if (cursorPosition != 15){
        cursorPosition++;
      } else {
        cursorPosition = 0;
      }
    }
    if (configPage == 10){ //make sure the panel doesn't re-render the text on the previous page when exiting
      configBottom = "";
      for (int i=0; i<=15; i++){ //generate name to print on lcd
        configBottom = configBottom + (char)panelNameList[i];
      }
    }
//----------------------------------------------------------------------------- SETTINGS > PANEL > PANEL NAME

//----------------------------------------------------------------------------- SETTINGS > PANEL > FACTORY RESET
  } else if (configPage == -1){
    if (silencePressed and not silenceStillPressed){
      configPage = 8;
      configLCDUpdate(3, (String)mainPanelSettings[3], (String)mainPanelSettings[4]);
    } else if (resetPressed and not resetStillPressed){
      digitalWrite(readyLed, LOW); //ready LED
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("RESETTING TO");
      lcd.setCursor(0,1);
      lcd.print("FACTORY SETTINGS");
      resetEEPROM();
      delay(4000);
      ESP.restart();
    }
//----------------------------------------------------------------------------- SETTINGS > PANEL > FACTORY RESET

//----------------------------------------------------------------------------- SETTINGS > FIRE ALARM > CODING
  } else if (configPage == 5){
    if (resetPressed and not resetStillPressed){
      if (cursorPosition == 0){
        if (codeWheel == 1){
          configLCDUpdate(1, (String)mainSettingsFireAlarmSettingsCoding[1]+"*", (String)mainSettingsFireAlarmSettingsCoding[2]);
        } else if (codeWheel == 2){
          configLCDUpdate(1, (String)mainSettingsFireAlarmSettingsCoding[1], (String)mainSettingsFireAlarmSettingsCoding[2]+"*");
        } else {
          configLCDUpdate(1, (String)mainSettingsFireAlarmSettingsCoding[1], (String)mainSettingsFireAlarmSettingsCoding[2]);
        }
      } else if (cursorPosition == 1) {
        if (codeWheel == 2){
          configLCDUpdate(2, (String)mainSettingsFireAlarmSettingsCoding[2]+"*", (String)mainSettingsFireAlarmSettingsCoding[3]);
        } else if (codeWheel == 3){
          configLCDUpdate(2, (String)mainSettingsFireAlarmSettingsCoding[2], (String)mainSettingsFireAlarmSettingsCoding[3]+"*");
        } else {
          configLCDUpdate(2, (String)mainSettingsFireAlarmSettingsCoding[2], (String)mainSettingsFireAlarmSettingsCoding[3]);
        }
      } else if (cursorPosition == 2) {
        if (codeWheel == 3){
          configLCDUpdate(3, (String)mainSettingsFireAlarmSettingsCoding[3]+"*", (String)mainSettingsFireAlarmSettingsCoding[4]);
        } else if (codeWheel == 4){
          configLCDUpdate(3, (String)mainSettingsFireAlarmSettingsCoding[3], (String)mainSettingsFireAlarmSettingsCoding[4]+"*");
        } else {
          configLCDUpdate(3, (String)mainSettingsFireAlarmSettingsCoding[3], (String)mainSettingsFireAlarmSettingsCoding[4]);
        }
      } else if (cursorPosition == 3) {
        if (codeWheel == 4){
          configLCDUpdate(4, (String)mainSettingsFireAlarmSettingsCoding[4]+"*", (String)mainSettingsFireAlarmSettingsCoding[5]);
        } else if (codeWheel == 5){
          configLCDUpdate(4, (String)mainSettingsFireAlarmSettingsCoding[4], (String)mainSettingsFireAlarmSettingsCoding[5]+"*");
        } else {
          configLCDUpdate(4, (String)mainSettingsFireAlarmSettingsCoding[4], (String)mainSettingsFireAlarmSettingsCoding[5]);
        }
      } else if (cursorPosition == 4) {
        if (codeWheel == 5){
          configLCDUpdate(5, (String)mainSettingsFireAlarmSettingsCoding[5]+"*", (String)mainSettingsFireAlarmSettingsCoding[0]);
        } else if (codeWheel == 0){
          configLCDUpdate(5, (String)mainSettingsFireAlarmSettingsCoding[5], (String)mainSettingsFireAlarmSettingsCoding[0]+"*");
        } else {
          configLCDUpdate(5, (String)mainSettingsFireAlarmSettingsCoding[5], (String)mainSettingsFireAlarmSettingsCoding[0]);
        }
      } else if (cursorPosition == 5) {
        if (codeWheel == 0){
          configLCDUpdate(0, (String)mainSettingsFireAlarmSettingsCoding[0]+"*", (String)mainSettingsFireAlarmSettingsCoding[1]);
        } else if (codeWheel == 1){
          configLCDUpdate(0, (String)mainSettingsFireAlarmSettingsCoding[0], (String)mainSettingsFireAlarmSettingsCoding[1]+"*");
        } else {
          configLCDUpdate(0, (String)mainSettingsFireAlarmSettingsCoding[0], (String)mainSettingsFireAlarmSettingsCoding[1]);
        }
      }
    } else if (silencePressed and not silenceStillPressed){
      configPage = 3; //change screen to facp settings
      if (useTwoWire){
        configLCDUpdate(0, (String)mainSettingsFireAlarmSettings[0] + " off", (String)mainSettingsFireAlarmSettings[1]);
      } else {
        configLCDUpdate(0, (String)mainSettingsFireAlarmSettings[0], (String)mainSettingsFireAlarmSettings[1]);
      }
    } else if (drillPressed and not drillStillPressed){
      EEPROM.write(7,cursorPosition); //write the new codewheel settings to eeprom
      EEPROM.commit();
      if (cursorPosition == 5){
        configLCDUpdate(cursorPosition, (String)mainSettingsFireAlarmSettingsCoding[cursorPosition]+"*", (String)mainSettingsFireAlarmSettingsCoding[0]);
      } else {
        configLCDUpdate(cursorPosition, (String)mainSettingsFireAlarmSettingsCoding[cursorPosition]+"*", (String)mainSettingsFireAlarmSettingsCoding[cursorPosition+1]);
      }
      codeWheel = EEPROM.read(7); //codeWheel setting address
    }
//----------------------------------------------------------------------------- SETTINGS > FIRE ALARM > CODING

//----------------------------------------------------------------------------- SETTINGS > FIRE ALARM > VERIFICATION
  } else if (configPage == 4){
    if (resetPressed and not resetStillPressed){
      if (cursorPosition == 0){
        if (not verificationEnabled){
          configLCDUpdate(1, (String)mainSettingsVerificationSettings[1] + "off", (String)mainSettingsVerificationSettings[2] + smokeDetectorVerification, false, true);
        } else {
          configLCDUpdate(1, (String)mainSettingsVerificationSettings[1] + (verificationTime/1000)+"s", (String)mainSettingsVerificationSettings[2] + smokeDetectorVerification, false, true);
        }
      } else if (cursorPosition == 1) {
        if (smokeDetectorVerification){
          if (smokeDetectorTimeout<60000){
            configLCDUpdate(2, (String)mainSettingsVerificationSettings[2] + smokeDetectorVerification, (String)mainSettingsVerificationSettings[3] + (smokeDetectorTimeout/1000) + "s", true, false);
          } else {
            configLCDUpdate(2, (String)mainSettingsVerificationSettings[2] + smokeDetectorVerification, (String)mainSettingsVerificationSettings[3] + (smokeDetectorTimeout/60000) + "m", true, false);
          }
        } else {
          configLCDUpdate(2, (String)mainSettingsVerificationSettings[2] + smokeDetectorVerification, (String)mainSettingsVerificationSettings[3] + "off", true, false);
        }
      } else if (cursorPosition == 2) {
        if (smokeDetectorVerification){
          if (smokeDetectorTimeout<60000){
            configTop = (String)mainSettingsVerificationSettings[3] + (smokeDetectorTimeout/1000) + "s";
          } else {
            configTop = (String)mainSettingsVerificationSettings[3] + (smokeDetectorTimeout/60000) + "m";
          }
        } else {
          configTop = (String)mainSettingsVerificationSettings[3] + "off";
        }
        if (smokeDetectorVerification){
          if (smokeDetectorPostRestartVerificationTime<60000){
            configLCDUpdate(3, configTop, (String)mainSettingsVerificationSettings[4] + (smokeDetectorPostRestartVerificationTime/1000) + "s");
          } else {
            configLCDUpdate(3, configTop, (String)mainSettingsVerificationSettings[4] + (smokeDetectorPostRestartVerificationTime/60000) + "m");
          }
        } else {
          configLCDUpdate(3, configTop, (String)mainSettingsVerificationSettings[4] + "off");
        }
      } else if (cursorPosition == 3) {
        if (smokeDetectorVerification){
          if (smokeDetectorPostRestartVerificationTime<60000){
            configLCDUpdate(4, (String)mainSettingsVerificationSettings[4] + (smokeDetectorPostRestartVerificationTime/1000) + "s", (String)mainSettingsVerificationSettings[0] + verificationEnabled, false, true);
          } else {
            configLCDUpdate(4, (String)mainSettingsVerificationSettings[4] + (smokeDetectorPostRestartVerificationTime/60000) + "m", (String)mainSettingsVerificationSettings[0] + verificationEnabled, false, true);
          }
        } else {
          configLCDUpdate(4, (String)mainSettingsVerificationSettings[4] + "off", (String)mainSettingsVerificationSettings[0] + verificationEnabled, false, true);
        }
      } else if (cursorPosition == 4) {
        if (not verificationEnabled){
          configLCDUpdate(0, (String)mainSettingsVerificationSettings[0] + verificationEnabled, (String)mainSettingsVerificationSettings[1] + "off", true, false);
        } else {
          configLCDUpdate(0, (String)mainSettingsVerificationSettings[0] + verificationEnabled, (String)mainSettingsVerificationSettings[1] + (verificationTime/1000)+"s", true, false);
        }
      } 
    } else if (silencePressed and not silenceStillPressed){
      configPage = 3;
      configLCDUpdate(1, (String)mainSettingsFireAlarmSettings[1], (String)mainSettingsFireAlarmSettings[2]);
    } else if (drillPressed and not drillStillPressed){
      if (cursorPosition == 0){
        if (verificationEnabled){
          verificationEnabled = false;
          EEPROM.write(9,0);
        } else {
          verificationEnabled = true;
          EEPROM.write(9,1);
        }
        EEPROM.commit();
        if (not verificationEnabled){
          configLCDUpdate(cursorPosition, (String)mainSettingsVerificationSettings[0]+verificationEnabled, (String)mainSettingsVerificationSettings[1] + "off", true, false);
        } else {
          configLCDUpdate(cursorPosition, (String)mainSettingsVerificationSettings[0]+verificationEnabled, (String)mainSettingsVerificationSettings[1] + (verificationTime/1000)+"s", true, false);
        }
      } else if (cursorPosition == 1 and verificationEnabled) {
        if (verificationTime == 500){
          verificationTime = 1000;
          EEPROM.write(10,10);
        } else if (verificationTime == 1000){
          verificationTime = 1500;
          EEPROM.write(10,15);
        } else if (verificationTime == 1500){
          verificationTime = 2500;
          EEPROM.write(10,25);
        } else if (verificationTime == 2500){
          verificationTime = 4500;
          EEPROM.write(10,45);
        } else if (verificationTime == 4500){
          verificationTime = 7500;
          EEPROM.write(10,75);
        } else if (verificationTime >= 7500){
          verificationTime = 500;
          EEPROM.write(10,5);
        }
        EEPROM.commit();
        if (not verificationEnabled){
          configLCDUpdate(cursorPosition, (String)mainSettingsVerificationSettings[1] + "off", (String)mainSettingsVerificationSettings[2] + smokeDetectorVerification, false, true);
        } else {
          configLCDUpdate(cursorPosition, (String)mainSettingsVerificationSettings[1] + (verificationTime/1000)+"s", (String)mainSettingsVerificationSettings[2] + smokeDetectorVerification, false, true);
        }
      } else if (cursorPosition == 2){
        if (not smokeDetectorVerification){
          EEPROM.write(76,1); //enable pre-alarm
          smokeDetectorVerification = true;
        } else {
          EEPROM.write(76,0); //disable pre-alarm
          smokeDetectorVerification = false;
          smokeDetectorOn(true); //re-enable smoke detectors in the case that smoke detectors are currently in timeout
          digitalWrite(alarmLed, LOW);
          smokeDetectorCurrentlyInVerification=false;
          smokeDetectorPostRestartTimer=0;
          smokeDetectorTimer=0;
        }
        EEPROM.commit();
        if (smokeDetectorVerification){
          if (smokeDetectorTimeout<60000){
            configLCDUpdate(cursorPosition, (String)mainSettingsVerificationSettings[2] + smokeDetectorVerification, (String)mainSettingsVerificationSettings[3] + (smokeDetectorTimeout/1000) + "s", true, false);
          } else {
            configLCDUpdate(cursorPosition, (String)mainSettingsVerificationSettings[2] + smokeDetectorVerification, (String)mainSettingsVerificationSettings[3] + (smokeDetectorTimeout/60000) + "m", true, false);
          }
        } else {
          configLCDUpdate(cursorPosition, (String)mainSettingsVerificationSettings[2] + smokeDetectorVerification, (String)mainSettingsVerificationSettings[3] + "off", true, false);
        }
      } else if (cursorPosition == 3 and smokeDetectorVerification) {
        if (smokeDetectorTimeout == 5000){
          EEPROM.write(77,2);
          smokeDetectorTimeout = 10000;
        } else if (smokeDetectorTimeout == 10000){
          EEPROM.write(77,3);
          smokeDetectorTimeout = 15000;
        } else if (smokeDetectorTimeout == 15000){
          EEPROM.write(77,4);
          smokeDetectorTimeout = 20000;
        } else if (smokeDetectorTimeout == 20000){
          EEPROM.write(77,6);
          smokeDetectorTimeout = 30000;
        } else if (smokeDetectorTimeout == 30000){
          EEPROM.write(77,9);
          smokeDetectorTimeout = 45000;
        } else if (smokeDetectorTimeout == 45000){
          EEPROM.write(77,12);
          smokeDetectorTimeout = 60000;
        } else if (smokeDetectorTimeout == 60000){
          EEPROM.write(77,24);
          smokeDetectorTimeout = 120000;
        } else if (smokeDetectorTimeout == 120000){
          EEPROM.write(77,60);
          smokeDetectorTimeout = 300000;
        } else if (smokeDetectorTimeout == 300000){
          EEPROM.write(77,120);
          smokeDetectorTimeout = 600000;
        } else if (smokeDetectorTimeout == 600000){
          EEPROM.write(77,1);
          smokeDetectorTimeout = 5000;
        }
        EEPROM.commit();
        if (smokeDetectorTimeout<60000){
          configTop = (String)mainSettingsVerificationSettings[3] + (smokeDetectorTimeout/1000) + "s";
        } else {
          configTop = (String)mainSettingsVerificationSettings[3] + (smokeDetectorTimeout/60000) + "m";
        }
        if (smokeDetectorVerification){
          if (smokeDetectorPostRestartVerificationTime<60000){
            configLCDUpdate(cursorPosition, configTop, (String)mainSettingsVerificationSettings[4] + (smokeDetectorPostRestartVerificationTime/1000) + "s");
          } else {
            configLCDUpdate(cursorPosition, configTop, (String)mainSettingsVerificationSettings[4] + (smokeDetectorPostRestartVerificationTime/60000) + "m");
          }
        } else {
          configLCDUpdate(cursorPosition, configTop, (String)mainSettingsVerificationSettings[4] + "off");
        }
      } else if (cursorPosition == 4 and smokeDetectorVerification) {
        if (smokeDetectorPostRestartVerificationTime == 60000){
          EEPROM.write(28,24);
          smokeDetectorPostRestartVerificationTime = 120000;
        } else if (smokeDetectorPostRestartVerificationTime == 120000){
          EEPROM.write(28,36);
          smokeDetectorPostRestartVerificationTime = 180000;
        } else if (smokeDetectorPostRestartVerificationTime == 180000){
          EEPROM.write(28,48);
          smokeDetectorPostRestartVerificationTime = 240000;
        } else if (smokeDetectorPostRestartVerificationTime == 240000){
          EEPROM.write(28,60);
          smokeDetectorPostRestartVerificationTime = 300000;
        } else if (smokeDetectorPostRestartVerificationTime == 300000){
          EEPROM.write(28,120);
          smokeDetectorPostRestartVerificationTime = 600000;
        } else if (smokeDetectorPostRestartVerificationTime == 600000){
          EEPROM.write(28,12);
          smokeDetectorPostRestartVerificationTime = 60000;
        }
        EEPROM.commit();
        if (smokeDetectorPostRestartVerificationTime<60000){
          configLCDUpdate(cursorPosition, (String)mainSettingsVerificationSettings[4] + (smokeDetectorPostRestartVerificationTime/1000) + "s", (String)mainSettingsVerificationSettings[0] + verificationEnabled, false, true);
        } else {
          configLCDUpdate(cursorPosition, (String)mainSettingsVerificationSettings[4] + (smokeDetectorPostRestartVerificationTime/60000) + "m", (String)mainSettingsVerificationSettings[0] + verificationEnabled, false, true);
        }
      }
    }
//----------------------------------------------------------------------------- SETTINGS > FIRE ALARM > VERIFICATION

//----------------------------------------------------------------------------- SETTINGS > PANEL > PANEL SECURITY
  } else if (configPage == 9){
    if (resetPressed and not resetStillPressed){
      if (cursorPosition == 0){
        if (keyRequiredVisual){
          configLCDUpdate(1, (String)mainPanelSettingsPanelSecurity[1]+"*", (String)mainPanelSettingsPanelSecurity[0]);
        } else {
          configLCDUpdate(1, (String)mainPanelSettingsPanelSecurity[1], (String)mainPanelSettingsPanelSecurity[0]+"*");
        }
      } else if (cursorPosition == 1) {
        if (keyRequiredVisual){
          configLCDUpdate(0, (String)mainPanelSettingsPanelSecurity[0], (String)mainPanelSettingsPanelSecurity[1]+"*");
        } else {
          configLCDUpdate(0, (String)mainPanelSettingsPanelSecurity[0]+"*", (String)mainPanelSettingsPanelSecurity[1]);
        }
      }
    } else if (silencePressed and not silenceStillPressed){
      configPage = 8;
      if (lcdTimeout == 0){
        configLCDUpdate(1, (String)mainPanelSettings[1], (String)mainPanelSettings[2] + "off");
      } else if (lcdTimeout<=30000) {
        configLCDUpdate(1, (String)mainPanelSettings[1], (String)mainPanelSettings[2] + lcdTimeout/1000+"s");
      } else {
        configLCDUpdate(1, (String)mainPanelSettings[1], (String)mainPanelSettings[2] + lcdTimeout/60000+"m");
      }
    } else if (drillPressed and not drillStillPressed){
      if (cursorPosition == 0){
        EEPROM.write(8,0); //write the new keyswitch settings to eeprom
        EEPROM.commit();
        keyRequiredVisual = false;
        configLCDUpdate(cursorPosition, (String)mainPanelSettingsPanelSecurity[0]+"*", (String)mainPanelSettingsPanelSecurity[1]);
      } else if (cursorPosition == 1) {
        EEPROM.write(8,1); //write the new keyswitch settings to eeprom
        EEPROM.commit();
        keyRequiredVisual = true;
        configLCDUpdate(cursorPosition, (String)mainPanelSettingsPanelSecurity[1]+"*", (String)mainPanelSettingsPanelSecurity[0]);
      }
    }
//----------------------------------------------------------------------------- SETTINGS > PANEL > PANEL SECURITY

//----------------------------------------------------------------------------- SETTINGS > PANEL > ABOUT
  } else if (configPage == 12){
    if (resetPressed and not resetStillPressed){
      if (cursorPosition == 0){
        configLCDUpdate(1, (String)mainPanelSettingsAbout[1]+firmwareRev, (String)mainPanelSettingsAbout[2]);   
      } else if (cursorPosition == 1) {
        configLCDUpdate(2, (String)mainPanelSettingsAbout[2], (String)mainPanelSettingsAbout[3]+powerOnMinutes/60);   
      } else if (cursorPosition == 2) {
        configLCDUpdate(3, (String)mainPanelSettingsAbout[3]+powerOnMinutes/60, (String)mainPanelSettingsAbout[0]);   
      } else if (cursorPosition == 3){
        configLCDUpdate(0, (String)mainPanelSettingsAbout[0], (String)mainPanelSettingsAbout[1]+firmwareRev);   
      }
    } else if (silencePressed and not silenceStillPressed){
      configPage = 8;
      configLCDUpdate(4, (String)mainPanelSettings[4], (String)mainPanelSettings[0]);
    }
//----------------------------------------------------------------------------- SETTINGS > PANEL > ABOUT

//----------------------------------------------------------------------------- SETTINGS > FIRE ALARM > PRE-ALARM
  } else if (configPage == 6){
    if (resetPressed and not resetStillPressed){
      if (cursorPosition == 0){
        if (preAlarm){
          configLCDUpdate(1, (String)mainSettingsFireAlarmSettingsPreAlarmSettings[1] + (firstStageTime/60000) + "m", (String)mainSettingsFireAlarmSettingsPreAlarmSettings[0] + preAlarm, false, true);
        } else {
          configLCDUpdate(1, (String)mainSettingsFireAlarmSettingsPreAlarmSettings[1] + "off", (String)mainSettingsFireAlarmSettingsPreAlarmSettings[0] + preAlarm, false, true);
        }

      } else if (cursorPosition == 1){
        if (preAlarm){
          configLCDUpdate(0, (String)mainSettingsFireAlarmSettingsPreAlarmSettings[0]+preAlarm, (String)mainSettingsFireAlarmSettingsPreAlarmSettings[1] + (firstStageTime/60000) + "m", true, false);
        } else {
          configLCDUpdate(0, (String)mainSettingsFireAlarmSettingsPreAlarmSettings[0]+preAlarm, (String)mainSettingsFireAlarmSettingsPreAlarmSettings[1] + "off", true, false);
        }
      }
    } else if (silencePressed and not silenceStillPressed){
      configPage = 3;
      configLCDUpdate(2, (String)mainSettingsFireAlarmSettings[2], (String)mainSettingsFireAlarmSettings[3]+audibleSilence, false, true);
    } else if (drillPressed and not drillStillPressed){
      if (cursorPosition == 0){
        if (not preAlarm){
          EEPROM.write(74,1); //enable pre-alarm
          preAlarm = true;
        } else {
          EEPROM.write(74,0); //disable pre-alarm
          preAlarm = false;
        }
        EEPROM.commit();
        if (preAlarm){
          configLCDUpdate(cursorPosition, (String)mainSettingsFireAlarmSettingsPreAlarmSettings[0]+preAlarm, (String)mainSettingsFireAlarmSettingsPreAlarmSettings[1] + (firstStageTime/60000) + "m", true, false);
        } else {
          configLCDUpdate(cursorPosition, (String)mainSettingsFireAlarmSettingsPreAlarmSettings[0]+preAlarm, (String)mainSettingsFireAlarmSettingsPreAlarmSettings[1] + "off", true, false);
        }
      } else if (cursorPosition == 1 and preAlarm) {
        if (firstStageTime == 60000){
          EEPROM.write(75,2);
          firstStageTime = 120000;
        } else if (firstStageTime == 120000){
          EEPROM.write(75,3);
          firstStageTime = 180000;
        } else if (firstStageTime == 180000){
          EEPROM.write(75,4);
          firstStageTime = 240000;
        } else if (firstStageTime == 240000){
          EEPROM.write(75,5);
          firstStageTime = 300000;
        } else if (firstStageTime == 300000){
          EEPROM.write(75,7);
          firstStageTime = 420000;
        } else if (firstStageTime == 420000){
          EEPROM.write(75,10);
          firstStageTime = 600000;
        } else if (firstStageTime == 600000){
          EEPROM.write(75,15);
          firstStageTime = 900000;
        } else if (firstStageTime == 900000){
          EEPROM.write(75,20);
          firstStageTime = 1200000;
        } else if (firstStageTime == 1200000){
          EEPROM.write(75,1);
          firstStageTime = 60000;
        }
        EEPROM.commit();
        configLCDUpdate(cursorPosition, (String)mainSettingsFireAlarmSettingsPreAlarmSettings[1] + (firstStageTime/60000) + "m", (String)mainSettingsFireAlarmSettingsPreAlarmSettings[0] + preAlarm, false, true);
      } 
    }
//----------------------------------------------------------------------------- SETTINGS > FIRE ALARM > PRE-ALARM

//----------------------------------------------------------------------------- SETTINGS > FIRE ALARM > STROBE SYNC
  } else if (configPage == 11){
    if (resetPressed and not resetStillPressed){
      if (cursorPosition == 0){
        if (strobeSync == 1){
          configLCDUpdate(1, (String)mainSettingsFireAlarmSettingsStrobeSync[1]+"*", (String)mainSettingsFireAlarmSettingsStrobeSync[2]);
        } else if (strobeSync == 2){
          configLCDUpdate(1, (String)mainSettingsFireAlarmSettingsStrobeSync[1], (String)mainSettingsFireAlarmSettingsStrobeSync[2]+"*");
        } else{
          configLCDUpdate(1, (String)mainSettingsFireAlarmSettingsStrobeSync[1], (String)mainSettingsFireAlarmSettingsStrobeSync[2]);
        }
      } else if (cursorPosition == 1) {
        if (strobeSync == 2){
          configLCDUpdate(2, (String)mainSettingsFireAlarmSettingsStrobeSync[2]+"*", (String)mainSettingsFireAlarmSettingsStrobeSync[3]);
        } else if (strobeSync == 3){
          configLCDUpdate(2, (String)mainSettingsFireAlarmSettingsStrobeSync[2], (String)mainSettingsFireAlarmSettingsStrobeSync[3]+"*");
        } else{
          configLCDUpdate(2, (String)mainSettingsFireAlarmSettingsStrobeSync[2], (String)mainSettingsFireAlarmSettingsStrobeSync[3]);
        }
      } else if (cursorPosition == 2) {
        if (strobeSync == 3){
          configLCDUpdate(3, (String)mainSettingsFireAlarmSettingsStrobeSync[3]+"*", (String)mainSettingsFireAlarmSettingsStrobeSync[4]);
        } else if (strobeSync == 4){
          configLCDUpdate(3, (String)mainSettingsFireAlarmSettingsStrobeSync[3], (String)mainSettingsFireAlarmSettingsStrobeSync[4]+"*");
        } else{
          configLCDUpdate(3, (String)mainSettingsFireAlarmSettingsStrobeSync[3], (String)mainSettingsFireAlarmSettingsStrobeSync[4]);
        }
      } else if (cursorPosition == 3) {
        if (strobeSync == 4){
          configLCDUpdate(4, (String)mainSettingsFireAlarmSettingsStrobeSync[4]+"*", (String)mainSettingsFireAlarmSettingsStrobeSync[0]);
        } else if (strobeSync == 5){
          configLCDUpdate(4, (String)mainSettingsFireAlarmSettingsStrobeSync[4], (String)mainSettingsFireAlarmSettingsStrobeSync[0]+"*");
        } else{
          configLCDUpdate(4, (String)mainSettingsFireAlarmSettingsStrobeSync[4], (String)mainSettingsFireAlarmSettingsStrobeSync[0]);
        }
      } else if (cursorPosition == 4) {
        if (strobeSync == 0){
          configLCDUpdate(0, (String)mainSettingsFireAlarmSettingsStrobeSync[0]+"*", (String)mainSettingsFireAlarmSettingsStrobeSync[1]);
        } else if (strobeSync == 1){
          configLCDUpdate(0, (String)mainSettingsFireAlarmSettingsStrobeSync[0], (String)mainSettingsFireAlarmSettingsStrobeSync[1]+"*");
        } else{
          configLCDUpdate(0, (String)mainSettingsFireAlarmSettingsStrobeSync[0], (String)mainSettingsFireAlarmSettingsStrobeSync[1]);
        }
      }
    } else if (silencePressed and not silenceStillPressed){
      configPage = 3; //change screen to facp settings
      configLCDUpdate(5, (String)mainSettingsFireAlarmSettings[5], (String)mainSettingsFireAlarmSettings[6]+useTwoWire, false, true);
    } else if (drillPressed and not drillStillPressed){
      EEPROM.write(29,cursorPosition); //write strobe sync settings
      EEPROM.commit();
      if (cursorPosition == 4){
        configLCDUpdate(cursorPosition, (String)mainSettingsFireAlarmSettingsStrobeSync[cursorPosition]+"*", (String)mainSettingsFireAlarmSettingsStrobeSync[0]);
      } else {
        configLCDUpdate(cursorPosition, (String)mainSettingsFireAlarmSettingsStrobeSync[cursorPosition]+"*", (String)mainSettingsFireAlarmSettingsStrobeSync[cursorPosition+1]);
      }
      strobeSync = EEPROM.read(29); //store the new strobe sync settings
    }
  }
//----------------------------------------------------------------------------- SETTINGS > FIRE ALARM > STROBE SYNC


// ---------------------------------------------------------------------------- CONFIG SCREEN REDRAW
  if (configTop != currentConfigTop or configBottom != currentConfigBottom){ //figure out if the screen *really* needs to be re-drawn
    lcd.clear();
    lcd.setCursor(0,0); //RENDER TOP OF CONFIG WINDOW
    if (configPage != -1){
      if (configTop.indexOf("*")>0){ //Replace any "*" with a check mark
        configTop.replace("*",""); 
        lcd.print("]" + configTop);
        lcd.write(byte(1));
        
      } else if (configTop.indexOf("$")>0) { //Replace any "$" with an X icon
        configTop.replace("$",""); 
        lcd.print("]" + configTop);
        lcd.write(byte(2));
      } else {
        lcd.print("]" + configTop);
      }
    } else {
      lcd.print(configTop);
    }
    lcd.setCursor(0,1); //RENDER BOTTOM OF CONFIG WINDOW
    if (configBottom.indexOf("*")>0){ //Replace any "*" with a check mark
        configBottom.replace("*","");
        lcd.print(configBottom);
        lcd.write(byte(1));
        
    } else if (configBottom.indexOf("$")>0) { //Replace any "$" with an X icon
      configBottom.replace("$","");
      lcd.print(configBottom);
      lcd.write(byte(2));

    } else {
      lcd.print(configBottom);
    }
    currentConfigTop = configTop;
    currentConfigBottom = configBottom;
    if (configPage == 10){
      lcd.setCursor(cursorPosition,1);
    }
    
  }
// ---------------------------------------------------------------------------- CONFIG SCREEN REDRAW

// --------- UPDATE BUTTON VARIABLES
  if (digitalRead(resetButtonPin)){ //RESET BUTTON
    resetStillPressed = true;
  }
  if (digitalRead(silenceButtonPin)){ //SILENCE BUTTON
    silenceStillPressed = true;
  }
  if (digitalRead(drillButtonPin)){ //DRILL BUTTON
    drillStillPressed = true;
  }
// --------- UPDATE BUTTON VARIABLES
}

void lcdBacklight(){
  if (lcdTimeout!=0){
    if (lcdTimeout <= lcdTimeoutTimer and backlightOn){
      lcd.noBacklight();
      backlightOn = false;
    } else if (backlightOn) {
      lcdTimeoutTimer++;
    }
    if (drillPressed or silencePressed or resetPressed or fullAlarm or trouble or (panelUnlocked and keyRequired) or smokeDetectorCurrentlyInVerification){
      lcdTimeoutTimer = 0;
      if (not backlightOn){
        lcd.backlight();
      }
      backlightOn = true;
    }
  }
}

void smokeDetector(){
  if (smokeDetectorTimer >= smokeDetectorTimeout){ //wait until the configured smoke timeout time has elapsed 
    smokeDetectorOn(true); //re-enable the smoke relay
    if (smokeDetectorPostRestartTimer >= smokeDetectorPostRestartVerificationTime){ //wait for 120 seconds, during this time, smokeDetectorCurrentlyInVerification will be true, so any activating device will cause a full alarm
      smokeDetectorCurrentlyInVerification = false; //stop smoke detector verification, causing the smoke detectors to require double activation again
      currentScreen = -1; //update screen
      digitalWrite(alarmLed, LOW); //disable LED
      smokeDetectorTimer = 0;
    } else {
      smokeDetectorPostRestartTimer++;
    } 
  } else {
    smokeDetectorTimer++;
  }
} 

void powerOnTracker(){
  if (powerOnMinutesCounter >= 60000){
    powerOnMinutesCounter = 0;
    powerOnMinutes++;
    byte byte_array[4];
    for (int i = 0; i < 4; i++) {
      byte_array[i] = (powerOnMinutes >> (i * 8)) & 0xFF; // split number into four bytes
      EEPROM.write(81+i, byte_array[i]); // store each byte in consecutive addresses
    }
    EEPROM.commit();
  } else {
    powerOnMinutesCounter++;
  }
}

void failsafe(){ //--------------------------------------------- FAILSAFE MODE IN CASE PANEL CANT BOOT NORMALLY
  if ((analogRead(zone1Pin) == 0 or analogRead(zone2Pin) == 0) and not fullAlarm){
    fullAlarm = true;
    silenced = false;
    digitalWrite(alarmLed, HIGH);
    if (invertRelay){
      digitalWrite(hornRelay, HIGH);
      digitalWrite(strobeRelay, HIGH);
    } else {
      digitalWrite(hornRelay, LOW);
      digitalWrite(strobeRelay, LOW);
    }
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("FAILSAFE MODE");
    lcd.setCursor(0,1);
    lcd.print("FIRE ALARM");
  }
  if (digitalRead(silenceButtonPin) and not silenced and fullAlarm){
    silenced = true;
    if (invertRelay){
      digitalWrite(hornRelay, LOW);
      digitalWrite(strobeRelay, LOW);
    } else {
      digitalWrite(hornRelay, HIGH);
      digitalWrite(strobeRelay, HIGH);
    }
    digitalWrite(alarmLed,LOW);
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("FAILSAFE MODE");
    lcd.setCursor(0,1);
    lcd.print("SILENCED");
  }
  if (digitalRead(resetButtonPin)){
    if (invertRelay){
      digitalWrite(smokeDetectorRelay, LOW);
      digitalWrite(hornRelay, LOW);
      digitalWrite(strobeRelay, LOW);      
    } else {
      digitalWrite(smokeDetectorRelay, HIGH);
      digitalWrite(hornRelay, HIGH);
      digitalWrite(strobeRelay, HIGH);
    }
    digitalWrite(silenceLed,LOW);
    digitalWrite(alarmLed,LOW);
    digitalWrite(readyLed,LOW);
    delay(500);
    ESP.restart();
  }
  if (troubleLedTimer >= 2000){
    if (digitalRead(readyLed)){
      digitalWrite(readyLed, LOW);
    } else {
      digitalWrite(readyLed, HIGH);
    }
    troubleLedTimer = 0;
  } else {
    troubleLedTimer++;
  }
}


void loop() {
  systemClock = millis(); //-------------------------------------------------- SYSTEM CLOCK

  if (systemClock-lastPulse >= 1){
    if (not failsafeMode){ //execute the loop only if the panel is not in failsafe mode
      powerOnTracker(); //---------------------------------------------------- INCREMENT THE POWER ON MINUTES

      lcdBacklight(); //------------------------------------------------------ CHECK LCD BACKLIGHT 

      checkDevices(); //------------------------------------------------------ CHECK ACTIVATING DEVICES
      
      troubleCheck(); //------------------------------------------------------ TROUBLE CHECK

      alarm(); //------------------------------------------------------------- ALARM CODEWHEEL

      if (smokeDetectorCurrentlyInVerification and not fullAlarm){ //if a smoke detector is in verification, execute this function
        smokeDetector();
      }

      if (keyCheckTimer >= 100){
        checkKey(); //---------------------------------------------------------- CHECK KEY 
        keyCheckTimer = 0;
      } else {
        keyCheckTimer++;
      }
      
      if (buttonCheckTimer >= 20){
        if (not inConfigMenu){
          if ((panelUnlocked and keyRequired) or (not keyRequired)){
            checkButtons(); //check if certain buttons are pressed
          } else if (keylessSilence and fullAlarm and not silenced){
            if (digitalRead(silenceButtonPin)){
              digitalWrite(silenceLed, HIGH);
              digitalWrite(alarmLed, LOW);
              digitalWrite(readyLed, LOW);
              horn = false;
              if (not audibleSilence){
                strobe = false;
              }
              silenced=true;
              noTone();
            }
          }
          lcdUpdate(); //------------------------------------------------------ UPDATE LCD DISPLAY

        } else if (inConfigMenu) {
          if ((panelUnlocked and keyRequired) or (not panelUnlocked and not keyRequired)){
            config();
          }
        }
        buttonCheckTimer = 0;
      } else {
        buttonCheckTimer++;
      }
    } else { //failsafe mode
      failsafe();
    }
    lastPulse = millis(); //update last pulse
  }
}
