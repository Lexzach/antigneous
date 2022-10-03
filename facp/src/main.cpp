#include <Arduino.h>
#include <EEPROM.h>
#include <LiquidCrystal_I2C.h>
#include <string.h>

unsigned long systemClock; //-----------------SYSTEM CLOCK [VERY IMPORTANT]
unsigned long lastPulse; //-------------------LAST void loop() PULSE

char *firmwareRev = "1.2"; //VERSION
int EEPROMVersion = 1; //version control to rewrite eeprom after update
int EEPROMBuild = 2;


//----------------------------------------------------------------------------- RUNTIME VARIABLES
bool fullAlarm = false; //bool to control if this is a full alarm that requres a panel reset to clear
bool silenced = false;
bool keyInserted = false; //if the control panel has a key inserted
bool readyLedStatus = false; //ready led status (this variable is for the trouble response code)
bool horn = false; //bool to control if the horns are active
bool strobe = false; //bool to control if the strobes are active
bool trouble = false; //bool to control if the panel is in trouble
bool troubleAck = false; //bool to control if the trouble is acknowledged
bool configMenu = false; //determine if the control panel is in the configuration menu

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

bool updateScreen = false; //updating the screen in the config menu
bool possibleAlarm = false; //panel receieved 0 from pull station ciruit and is now investigating
bool walkTest = false; //is the system in walk test
bool silentWalkTest = false;
bool backlightOn = true;
bool keyRequiredVisual; //variable for panel security
bool keylessSilence = false; //can the panel be silenced without a key
bool debug = false;
bool updateLockStatus = false; //if the screen needs to be updated for the lock/unlock icon
bool secondStage = false; //if the panel is in second stage
int characters[] = {32,45,46,47,48,49,50,51,52,53,54,55,56,57,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,80,81,82,83,84,85,86,87,88,89,90}; //characters allowed on name
int panelNameList[16];
int clearTimer = 0; //timer for keeping track of button holding for clearing character in the name editor
int verification = 0; //number to keep track of ms for verification
int drill = 0; //number to keep track of ms for drill
int buttonCheckTimer = 0; //add a slight delay to avoid double-clicking buttons
int keyCheckTimer = 0;
int troubleTimer = 0; //ms for trouble
int codeWheelTimer = 0; //code wheel timing variable
int troubleLedTimer = 0; //number to keep track of ms for trouble light
int alarmLedTimer = 0; //alarm led timer
int troubleType = 0; //trouble type 0 - general, 1 - eol resistor
int lcdUpdateTimer = 0; //update delay
int lcdTimeoutTimer = 0; //timer to keep track of how long until lcd off
int walkTestSmokeDetectorTimer = 0;
int currentScreen = -1; //update display if previous screen is not the same
int configPage = 0; //config page for the config menu
int cursorPosition = 0; //which menu item the cursor is over
int zone1Count = 0; //walk test variables
int zone2Count = 0;
int zoneAlarm = 0; //which zone is in alarm 0 - none | 1 - zone 1 | 2 - zone 2 | 3 - zone 1 & 2 | 4 - drill
int zoneTrouble = 0; //which zone is in trouble 0 - none | 1 - zone 1 | 2 - zone 2 | 3 - zone 1 & 2
String configTop; //configuration menu strings for lcd
String configBottom;
String currentConfigTop; //configuration menu strings for current lcd display
String currentConfigBottom;
bool keyRequired = false; //determine if key switch is required to operate buttons
bool isVerification = true; //is verification turned on
bool eolResistor = true; //is the EOL resistor enabled
bool preAlarm = false; //use pre-alarm?
bool smokeDetectorVerification = false; //should smoke detectors activate first stage
bool smokeDetectorCurrentlyInVerification = false; //Is a smoke detector currently in verification?
bool audibleSilence = true;
int smokeDetectorVerificationTime = 60000; //how long to wait before toggling smoke detectors back on
int smokeDetectorPostRestartTimer = 0; //variable to keep track of the 60 seconds post-power up that the panel watches the smoke detector
int smokeDetectorTimer = 0; //timer to keep track of the current smoke detector timeout progress
int firstStageTime = 300000; //time in minutes that first stage should last
int firstStageTimer = 0; //timer to keep track of current first stage
int codeWheel = 0; //which alarm pattern to use, code-3 default
float verificationTime = 2500;
//int resistorLenience = 0; DEPRECATED
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

//PINS
int zone1Pin = 15;
int zone2Pin = 15; //TESTING is set to 15 but is normally 39.
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
  for (int i=0; i<=1024; i++){ //write all 255's from 0-1024 address
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

  EEPROM.write(27,0); //write current version and build
  Serial.println("Disabled keyless silence");
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


  EEPROM.commit();
  Serial.println("Wrote changes to EEPROM");
}
//----------------------------------------------------------------------------- EEPROM RESET

void setup() {
  EEPROM.begin(1025); //allocate memory address 0-1024 for EEPROM
  Serial.println("Configured EEPROM for addresses 0-1024");
  Serial.begin(115200); //begin serial
  Serial.println("Lexzach's Low-Cost FACP v1");
  Serial.println("Booting...");

//----------------------------------------------------------------------------- SETUP PINS
  pinMode(hornRelay, OUTPUT); //horn
  pinMode(strobeRelay, OUTPUT); //strobe
  pinMode(smokeDetectorRelay, OUTPUT); //smoke relay
  pinMode(readyLed, OUTPUT); //ready LED
  pinMode(silenceLed, OUTPUT); //silence LED
  pinMode(alarmLed, OUTPUT); //alarm LED
  pinMode(keySwitchPin, INPUT); //key switch
  pinMode(resetButtonPin, INPUT); //reset switch
  pinMode(silenceButtonPin, INPUT); //silence switch
  pinMode(drillButtonPin, INPUT); //drill switch
  
  pinMode(zone1Pin, INPUT); //zone 1
  pinMode(zone2Pin, INPUT); //zone 2

  pinMode(buzzerPin, OUTPUT); //buzzer
  digitalWrite(hornRelay, HIGH); //horn
  digitalWrite(strobeRelay, HIGH); //strobe
  digitalWrite(smokeDetectorRelay, HIGH); //smoke relay
  
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
  if (digitalRead(resetButtonPin)==HIGH and digitalRead(silenceButtonPin)==HIGH and digitalRead(drillButtonPin)==HIGH){ //reset EEPROM if all buttons are pressed
    for(int i=5; i!=0; i--){
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("TO FACTORY RESET");
      lcd.setCursor(0,1);
      lcd.print((String)"HOLD BUTTONS - "+i);
      delay(1000);
    }
    if (digitalRead(resetButtonPin)==HIGH and digitalRead(silenceButtonPin)==HIGH and digitalRead(drillButtonPin)==HIGH){
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("RESETTING TO");
      lcd.setCursor(0,1);
      lcd.print("FACTORY SETTINGS");
      resetEEPROM();
      delay(4000);
      ESP.restart();
    } else {
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("FACTORY RESET");
      lcd.setCursor(0,1);
      lcd.print("CANCELLED");
      delay(3000);
    }
  }
//----------------------------------------------------------------------------- EEPROM RESET BUTTONS

  if (digitalRead(silenceButtonPin)==HIGH){
    debug = true;
  }

  lcd.clear();
  lcd.setCursor(4,0);
  lcd.print("BOOTING...");
  lcd.setCursor(0,1);
  lcd.print("IO");
  delay(100);

  Serial.println("Configured all pins");

  //EEPROM.write(0,255); //-------------------------------------------------------UNCOMMENT TO INVALIDATE EEPROM AND REFLASH IT AFTER EVERY RESTART
  //EEPROM.commit();

  Serial.println("Verifying EEPROM configuration integrity...");

//----------------------------------------------------------------------------- EEPROM INTEGRETY  
  if (EEPROM.read(0) != 76 or EEPROM.read(6) != 104 or EEPROM.read(7) > 5 or EEPROM.read(8) > 1 or EEPROM.read(50) != EEPROMVersion or EEPROM.read(51) != EEPROMBuild){
    Serial.println("EEPROM verification failed.");
    lcd.clear();
    lcd.setCursor(0,0);
    if (EEPROM.read(50) != EEPROMVersion or EEPROM.read(51) != EEPROMBuild){
      lcd.print("ERROR 2");
    } else {
      lcd.print("ERROR 1");
    }
    lcd.setCursor(0,1); 
    lcd.print("check manual");
    while(digitalRead(resetButtonPin) == LOW and digitalRead(drillButtonPin) == LOW){ //wait until button is pressed
      delay(1);
    }
    if(digitalRead(resetButtonPin) == HIGH){
      Serial.println("Resetting...");
      ESP.restart();
    } else if (digitalRead(drillButtonPin) == HIGH){
      resetEEPROM();
    }
  } else {
    Serial.println("EEPROM integrity verified");
  }
//----------------------------------------------------------------------------- EEPROM INTEGRETY  
  lcd.clear();
  lcd.setCursor(4,0);
  lcd.print("BOOTING...");
  lcd.setCursor(0,1);
  lcd.print("IO-SLFTST");
  delay(100);

  Serial.println("Current EEPROM dump:"); //dump EEPROM into serial monitor
  for (int i=0; i<=1024; i++){
    Serial.print(i);
    Serial.print(":");
    Serial.print(EEPROM.read(i));
    Serial.print(" ");
  }
  Serial.println();

  //CONFIG LOADING ROUTINE
  Serial.println("Loading config from EEPROM...");
  codeWheel = EEPROM.read(7); //codeWheel setting address
  if (EEPROM.read(8) == 1){
    keyRequired = true;
  } else {
    keyRequired = false;
  }
//----------------------------- Panel security variable
  
  if (keyRequired){
    keyRequiredVisual = true;
  } else {
    keyRequiredVisual = false;
  }
//----------------------------- Panel security variable
  if (EEPROM.read(9) == 1){
    isVerification = true;
  } else {
    isVerification = false;
  }
  if (EEPROM.read(73) == 1){
    eolResistor = true;
  } else {
    eolResistor = false;
  }
  if (EEPROM.read(74) == 1){
    preAlarm = true;
  } else {
    preAlarm = false;
  }
  if (EEPROM.read(76) == 1){
    smokeDetectorVerification = true;
  } else {
    smokeDetectorVerification = false;
  }
  if (EEPROM.read(79) == 1){
    audibleSilence = true;
  } else {
    audibleSilence = false;
  }
  if (EEPROM.read(27) == 1){
    keylessSilence = true;
  } else {
    keylessSilence = false;
  }
  smokeDetectorVerificationTime = EEPROM.read(77)*5000;
  firstStageTime = EEPROM.read(75)*60000;
  verificationTime = EEPROM.read(10)*100;
  //resistorLenience = EEPROM.read(72)*4; DEPRECATED
  panelHomescreen = EEPROM.read(78);
  lcdTimeout = EEPROM.read(80)*15000;
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
  readyLedStatus = true;
  updateLockStatus = true;
  if (digitalRead(keySwitchPin) == HIGH and keyRequired == true){ //check the key status on startup
    keyInserted = true;
  } else {
    keyInserted = false;
  }
  digitalWrite(silenceLed, LOW);
  digitalWrite(alarmLed, LOW);
  digitalWrite(smokeDetectorRelay, LOW); //turn on smoke relay
  lastPulse = millis(); //start last pulse
  Serial.println("-=- STARTUP COMPLETE -=-");
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
  if (on == true){
    digitalWrite(hornRelay, LOW); //turn on horn relay
  } else {
    digitalWrite(hornRelay, HIGH); //turn off horn relay
  }
}
void strobeOn(bool on){
  if (on == true){
    digitalWrite(strobeRelay, LOW); //turn on strobe relay
  } else {
    digitalWrite(strobeRelay, HIGH); //turn off strobe relay
  }
}
void smokeDetectorOn(bool on){
  if (on == true){
    digitalWrite(smokeDetectorRelay, LOW); //turn on smoke relay
  } else {
    digitalWrite(smokeDetectorRelay, HIGH); //turn off smoke relay
  }
}
//---------------------------------------------------------------------------------------- functions for turning certain things on and off

void activateNAC(){
  horn = true;
  strobe = true;
  fullAlarm = true;
  silenced = false;
  configMenu = false;
  codeWheelTimer = 0;
  if (zoneAlarm == 4 or preAlarm == false){
    secondStage = true; //entirely skip first stage if it is a drill or if prealarm is turned off
  } else {
    secondStage = false;
  }
  tone();
  digitalWrite(readyLed, HIGH);
  readyLedStatus = true;
  digitalWrite(alarmLed, HIGH);
  digitalWrite(silenceLed, LOW);
}

void checkKey(){
  if (digitalRead(keySwitchPin) == HIGH){
    if (keyInserted == false and keyRequired == true){
      keyInserted = true;
      updateLockStatus = true;
    }
  } else {
    if (keyInserted == true and keyRequired == true){
      keyInserted = false;
      drillPressed=false;
      silencePressed=false; //make sure all buttons are registered as depressed after key is removed
      resetPressed=false;
      drillStillPressed=false;
      resetStillPressed=false;
      silenceStillPressed=false;
      drill = 0;
      updateLockStatus = true;
    }
  }
}

//----------------------------------------------------------------------------- CHECK ACTIVATION DEVICES [!THIS CODE MUST WORK!]
void checkDevices(){
  if (walkTest == false){
    if ((analogRead(zone1Pin) == 0 or analogRead(zone2Pin) == 0) and horn != true and silenced==false){
      possibleAlarm = true;
    } 

    if (possibleAlarm == true and horn != true and strobe != true and silenced==false){
      if (verification >= verificationTime or isVerification == false){
        if (analogRead(zone1Pin) == 0 or analogRead(zone2Pin) == 0){
          if (smokeDetectorVerification == false or smokeDetectorCurrentlyInVerification == true){ // ----------------------------------- SMOKE DETECTOR VERIFICATION
            if (analogRead(zone1Pin) == 0 and analogRead(zone2Pin) == 0){
              zoneAlarm = 3; //both
            } else if (analogRead(zone2Pin) == 0){
              zoneAlarm = 2; //z2
            } else {
              zoneAlarm = 1; //z1
            }
            activateNAC();
            possibleAlarm = false;
            verification = 0;
          } else if (smokeDetectorVerification == true and smokeDetectorCurrentlyInVerification == false){
            smokeDetectorOn(false);
            delay(100);
            if (analogRead(zone1Pin) == 0 or analogRead(zone2Pin) == 0){ // if the alarm signal persists after turning off the smoke detectors, activate the nacs
              if (analogRead(zone1Pin) == 0 and analogRead(zone2Pin) == 0){
                zoneAlarm = 3; //both
              } else if (analogRead(zone2Pin) == 0){
                zoneAlarm = 2; //z2
              } else {
                zoneAlarm = 1; //z1
              }
              smokeDetectorOn(true);
              activateNAC();
              possibleAlarm = false;
              verification = 0;
            } else {
              smokeDetectorPostRestartTimer = 0;
              smokeDetectorCurrentlyInVerification = true; //--------------------------FIGURE OUT HOW TO DO THE SECOND PART OF SMOKE DET VERIFICATION
              smokeDetectorTimer = 0;
              currentScreen = -1;
              digitalWrite(alarmLed, HIGH);
              possibleAlarm = false;
              verification = 0;
            }
          }
        } else {
          possibleAlarm = false;
          verification = 0;
        }
      } else {
        verification++;
      }
    }
  } else if (walkTest == true){
    if (analogRead(zone1Pin) == 0 or analogRead(zone2Pin) == 0){// or analogRead(zone2Pin) == 0){
      if (analogRead(zone1Pin) == 0){
        zone1Count++;
        smokeDetectorOn(false);
        strobeOn(true);
        delay(500);
        digitalWrite(alarmLed, HIGH);
        if (silentWalkTest == false){
          hornOn(true);
        }
        delay(500);
        if (silentWalkTest == false){
          hornOn(false);
        }
        delay(3000);
        digitalWrite(alarmLed, LOW);
        strobeOn(false);
        smokeDetectorOn(true);
      } else if (analogRead(zone2Pin) == 0){
        zone2Count++;
        smokeDetectorOn(false);
        strobeOn(true);
        delay(500);
        digitalWrite(alarmLed, HIGH);
        if (silentWalkTest == false){
          hornOn(true);
        }
        delay(500);
        if (silentWalkTest == false){
          hornOn(false);
        }
        delay(500);
        if (silentWalkTest == false){
          hornOn(true);
        }
        delay(500);
        if (silentWalkTest == false){
          hornOn(false);
        }
        digitalWrite(alarmLed, LOW);
        delay(3000);
        strobeOn(false);
        smokeDetectorOn(true);
      }
      currentScreen = -1;
    }
  }

  if ((analogRead(zone1Pin) == 4095 or analogRead(zone2Pin) == 4095) and eolResistor == true) {
    if (troubleTimer >= 10){
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
  if (trouble == true and fullAlarm == false and walkTest == false){
    if (troubleLedTimer >= 200){
      if (readyLedStatus == true){
        digitalWrite(readyLed, LOW);
        readyLedStatus = false;
        if (troubleAck==false){
          noTone();
        }
      } else {
        digitalWrite(readyLed, HIGH);
        readyLedStatus = true;
        if (troubleAck==false){
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
  if (digitalRead(resetButtonPin) == HIGH){ //-----------------------------------------------------------RESET BUTTON
    reboot();
  }

  if (digitalRead(silenceButtonPin) == HIGH){ //---------------------------------------------------------SILENCE BUTTON
    if (horn == true){ //if horns are not silenced, silence the horns
      digitalWrite(silenceLed, HIGH);
      digitalWrite(alarmLed, LOW);
      digitalWrite(readyLed, LOW);
      readyLedStatus = false;
      horn = false;
      if (audibleSilence == false){
        strobe = false;
      }
      silenced=true;
      noTone();
    } else if (horn == false and strobe == false and trouble == true and silencePressed == false and troubleAck==false){
      troubleAck = true;
      noTone();
    } else if (horn == false and strobe == false and fullAlarm == false and silencePressed == false and configMenu == false){
      configMenu = true;
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

  if (digitalRead(drillButtonPin) == HIGH and horn != true and silenced != true and fullAlarm == false){ //------------------------------------------DRILL BUTTON
    if (drill >= 237){
      zoneAlarm = 4;
      activateNAC();
    } else {
      drill++;
    }
    drillPressed = true;
  } else {
    drill = 0;
    drillPressed = false;
  }
  if (digitalRead(drillButtonPin) == HIGH and fullAlarm == true and secondStage == false){
    secondStage = true;
    currentScreen = -1;
  }
}
//----------------------------------------------------------------------------- BUTTON CHECK


//----------------------------------------------------------------------------- NAC ACTIVATION
void alarm(){
  if (strobe == true){
    strobeOn(true);
  }else{
    strobeOn(false);
  }
  if (horn == true){
    if (preAlarm == false or secondStage == true){
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

      } else if (codeWheel == 2) { //---------- 4-4
        
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
      
      } else if (codeWheel == 3) { //---------- continuous
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
    } else if (preAlarm == true and secondStage == false){
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
  }
  if (fullAlarm == true){
    alarmLedTimer++;
    if (alarmLedTimer >= 750){
      if (digitalRead(alarmLed) == false){
        if (silenced == true){
          tone();
        }
        digitalWrite(alarmLed, HIGH);
      } else {
        if (silenced == true){
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
  if (trouble==false and fullAlarm==false and horn==false and strobe==false and walkTest == false and currentScreen != 0 and drill == 0){
    lcd.noAutoscroll();
    lcd.clear();
    lcd.setCursor(2,0);
    if (smokeDetectorCurrentlyInVerification == true){
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
  }  else if (fullAlarm == true and silenced == false and currentScreen != 3){
    lcd.clear();
    lcd.setCursor(1,0);
    if (secondStage == true){ //print pre-alarm if it is first stage
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
  } else if (silenced == true and currentScreen != 4){
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
    // lcd.setCursor(2,1);
    // lcd.print("Zone 1");
    currentScreen = 4;
    updateLockStatus = true;
  } else if (walkTest == true and currentScreen != 5) {
    lcd.clear();
    lcd.setCursor(1,0);
    lcd.print("* Supervisory *");
    lcd.setCursor(0,1);
    lcd.print("Z1:"+(String)zone1Count+" Z2:"+(String)zone2Count);
    currentScreen = 5;
    updateLockStatus = true;
    digitalWrite(readyLed, LOW); //ready led off for walk test
    readyLedStatus = false;
  } else if (drillPressed == true and fullAlarm == false and horn == false and strobe == false and walkTest == false and currentScreen != 6) {
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("CONTINUE HOLDING");
    lcd.setCursor(1,1);
    lcd.print("TO START DRILL");
    currentScreen = 6;
  } else if (trouble==true and fullAlarm == false and drillPressed == false and walkTest == false and currentScreen != 1){
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
  if (updateLockStatus == true and configMenu == false and keyRequired == true){
    lcd.setCursor(0,0);
    if (keyInserted == false){
      lcd.write(byte(0));
    } else {
      lcd.print(" ");  
    }
    updateLockStatus = false;
  }
}
void config(){
  char *main[] = {"Testing","Settings"}; //menu 0
  char *mainTesting[] = {"Walk Test","Silent Wlk Test","Strobe Test"}; //menu 1
  char *mainSettings[] = {"Fire Alarm","Panel"}; //menu 2
  char *mainSettingsFireAlarmSettings[] = {"Coding","Verification","Pre-Alarm","Audible Sil.:","No-Key Sil.:"}; //menu 3
  char *mainSettingsVerificationSettings[] = {"Verification:","V.Time:","Det.Verif:","Det.V.Time:"}; //menu 4
  char *mainSettingsFireAlarmSettingsCoding[] = {"Temporal Three","Marchtime","4-4","Continuous","California","Slow Marchtime"}; //menu 5
  char *mainSettingsFireAlarmSettingsPreAlarmSettings[] = {"Pre-Alarm:","Stage1 Time:"}; //menu 6
  char *mainPanelSettings[] = {"Panel Name","Panel Security","LCD Dim:","Factory Reset","About"}; //menu 8
  char *mainPanelSettingsPanelSecurity[] = {"None","Keyswitch","Passcode"}; //menu 9
  char *mainPanelSettingsPanelName[] = {"Enter Name:"}; //menu 10
  char *mainPanelSettingsAbout[] = {"Antigneous FACP","Firmware: ","by Lexzach"}; //menu 12
  
  if (digitalRead(resetButtonPin) == HIGH){ //RESET BUTTON
    resetPressed = true;
  } else {
    resetPressed = false;
    resetStillPressed = false;
  }
  if (digitalRead(silenceButtonPin) == HIGH){ //SILENCE BUTTON
    silencePressed = true;
  } else {
    silencePressed = false;
    silenceStillPressed = false;
  }
  if (digitalRead(drillButtonPin) == HIGH){ //DRILL BUTTON
    drillPressed = true;
  } else {
    drillPressed = false;
    drillStillPressed = false;
  }
//----------------------------------------------------------------------------- MAIN MENU
  if (configPage == 0){
    if (resetPressed == true and resetStillPressed == false){
      if (cursorPosition == 0){ //main screen
        cursorPosition = 1;
        configTop = (String)main[1];
        configBottom = (String)main[0];
      } else if (cursorPosition == 1){
        cursorPosition = 0;
        configTop = (String)main[0];
        configBottom = (String)main[1];
      }
    } else if (silencePressed == true and silenceStillPressed == false){
      silencePressed = true;
      configMenu = false;
      currentScreen=-1;
    } else if (drillPressed == true and drillStillPressed == false){
      if (cursorPosition == 0){ //cursor over testing
        configPage = 1; //change screen to testing
        cursorPosition = 0;
        configTop = (String)mainTesting[0];
        configBottom = (String)mainTesting[1];
      } else if (cursorPosition == 1){ //cursor over settings
        configPage = 2; //change screen to settings
        cursorPosition = 0;
        configTop = (String)mainSettings[0];
        configBottom = (String)mainSettings[1];
      }
    }
//----------------------------------------------------------------------------- MAIN MENU

//----------------------------------------------------------------------------- TESTING
  } else if (configPage == 1){
    if (resetPressed == true and resetStillPressed == false){
      if (cursorPosition == 0){
        cursorPosition = 1;
        configTop = (String)mainTesting[1];
        configBottom = (String)mainTesting[2];
      } else if (cursorPosition == 1) {
        cursorPosition = 2;
        configTop = (String)mainTesting[2];
        configBottom = (String)mainTesting[0];
      } else if (cursorPosition == 2) {
        cursorPosition = 0;
        configTop = (String)mainTesting[0];
        configBottom = (String)mainTesting[1];
      }
    } else if (silencePressed == true and silenceStillPressed == false){
      configPage = 0;
      cursorPosition = 0;
      configTop = (String)main[0];
      configBottom = (String)main[1];
      strobe = false;
      smokeDetectorOn(true);
      digitalWrite(readyLed,HIGH);
      readyLedStatus = true;
    } else if (drillPressed == true and drillStillPressed == false){
        if (cursorPosition == 0){
          walkTest = true;
          silentWalkTest = false;
          silencePressed = true;
          configMenu = false;
          currentScreen=-1;
          zone1Count = 0;
          zone2Count = 0;
        } else if (cursorPosition == 1) {
          walkTest = true;
          silentWalkTest = true;
          silencePressed = true;
          configMenu = false;
          currentScreen=-1;
          zone1Count = 0;
          zone2Count = 0;
        } else if (cursorPosition == 2) {
          if (strobe == false){
            strobe = true;
            smokeDetectorOn(false); //prevent (specifically cheap IR) smoke detectors from tripping from the strobe 
            digitalWrite(readyLed,LOW);
            readyLedStatus = false;
            configTop = (String)mainTesting[2]+" *";
          } else {
            strobe = false;
            smokeDetectorOn(true);
            digitalWrite(readyLed,HIGH);
            readyLedStatus = true;
            configTop = (String)mainTesting[2];
          }
        }
    }
//----------------------------------------------------------------------------- TESTING

//----------------------------------------------------------------------------- SETTINGS
  } else if (configPage == 2){
    if (resetPressed == true and resetStillPressed == false){
      if (cursorPosition == 0){ //main screen
        cursorPosition = 1;
        configTop = (String)mainSettings[1];
        configBottom = (String)mainSettings[0];
      } else if (cursorPosition == 1){
        cursorPosition = 0;
        configTop = (String)mainSettings[0];
        configBottom = (String)mainSettings[1];
      }
    } else if (silencePressed == true and silenceStillPressed == false){
      configPage = 0;
      cursorPosition = 1;
      configTop = (String)main[1];
      configBottom = (String)main[0];
    } else if (drillPressed == true and drillStillPressed == false){
      if (cursorPosition == 0){
        configPage = 3; //change screen to facp settings
        cursorPosition = 0;
        configTop = (String)mainSettingsFireAlarmSettings[0];
        configBottom = (String)mainSettingsFireAlarmSettings[1];
      } else if (cursorPosition == 1){
        configPage = 8; //change screen to facp settings
        cursorPosition = 0;
        configTop = (String)mainPanelSettings[0];
        configBottom = (String)mainPanelSettings[1];
      }
    }
//----------------------------------------------------------------------------- SETTINGS

//----------------------------------------------------------------------------- SETTINGS > FIRE ALARM
  } else if (configPage == 3){
    if (resetPressed == true and resetStillPressed == false){
      if (cursorPosition == 0){
        cursorPosition = 1;
        configTop = (String)mainSettingsFireAlarmSettings[1];
        configBottom = (String)mainSettingsFireAlarmSettings[2];
      } else if (cursorPosition == 1) {
        cursorPosition = 2;
        configTop = (String)mainSettingsFireAlarmSettings[2];
        configBottom = (String)mainSettingsFireAlarmSettings[3]+audibleSilence;
        configBottom.replace("1","*");
        configBottom.replace("0","$");
      } else if (cursorPosition == 2) {
        cursorPosition = 3;
        configTop = (String)mainSettingsFireAlarmSettings[3]+audibleSilence;
        if (keyRequired == true){
          configBottom = (String)mainSettingsFireAlarmSettings[4]+keylessSilence;
        } else {
          configBottom = (String)mainSettingsFireAlarmSettings[4]+"off";
        }
        configTop.replace("1","*");
        configTop.replace("0","$");
        configBottom.replace("1","*");
        configBottom.replace("0","$");
      } else if (cursorPosition == 3){
        cursorPosition = 4;
        if (keyRequired == true){
          configTop = (String)mainSettingsFireAlarmSettings[4]+keylessSilence;
        } else {
          configTop = (String)mainSettingsFireAlarmSettings[4]+"off";
        }
        configBottom = (String)mainSettingsFireAlarmSettings[0];
        configTop.replace("1","*");
        configTop.replace("0","$");
      } else if (cursorPosition == 4) {
        cursorPosition = 0;
        configTop = (String)mainSettingsFireAlarmSettings[0];
        configBottom = (String)mainSettingsFireAlarmSettings[1];
      }
    } else if (silencePressed == true and silenceStillPressed == false){
      configPage = 2;
      cursorPosition = 0;
      configTop = (String)mainSettings[0];
      configBottom = (String)mainSettings[1];
    } else if (drillPressed == true and drillStillPressed == false){
      if (cursorPosition == 0){
        configPage = 5;
        cursorPosition = 0;
        
        if (codeWheel == 0){
          configTop = (String)mainSettingsFireAlarmSettingsCoding[0] + "*";
        } else {
          configTop = (String)mainSettingsFireAlarmSettingsCoding[0];
        }
        if (codeWheel == 1){
          configBottom = (String)mainSettingsFireAlarmSettingsCoding[1] + "*";
        } else {
          configBottom = (String)mainSettingsFireAlarmSettingsCoding[1];
        }
      } else if (cursorPosition == 1) {
        configPage = 4;
        cursorPosition = 0;
        configTop = (String)mainSettingsVerificationSettings[0] + isVerification;
        if (isVerification == false){
          configBottom = (String)mainSettingsVerificationSettings[1] + "off";
        } else {
          configBottom = (String)mainSettingsVerificationSettings[1] + (verificationTime/1000)+"s";
        }
        configTop.replace("1","*");
        configTop.replace("0","$");
      } else if (cursorPosition == 2) {
        configPage = 6;
        cursorPosition = 0;
        configTop = (String)mainSettingsFireAlarmSettingsPreAlarmSettings[0]+preAlarm;
        if (preAlarm == true){
          configBottom = (String)mainSettingsFireAlarmSettingsPreAlarmSettings[1] + (firstStageTime/60000) + "m";
        } else {
          configBottom = (String)mainSettingsFireAlarmSettingsPreAlarmSettings[1] + "off";
        }
        configTop.replace("1","*");
        configTop.replace("0","$");
      } else if (cursorPosition == 3) {
        if (audibleSilence == true){
          audibleSilence = false;
          EEPROM.write(79,0);
        } else {
          audibleSilence = true;
          EEPROM.write(79,1);
        }
        EEPROM.commit();
        configTop = (String)mainSettingsFireAlarmSettings[3]+audibleSilence;
        if (keyRequired == true){
          configBottom = (String)mainSettingsFireAlarmSettings[4]+keylessSilence;
        } else {
          configBottom = (String)mainSettingsFireAlarmSettings[4]+"off";
        }
        configTop.replace("1","*");
        configTop.replace("0","$");
        configBottom.replace("1","*");
        configBottom.replace("0","$");
      } else if (cursorPosition == 4 and keyRequired == true){
        if (keylessSilence == true){
          keylessSilence = false;
          EEPROM.write(27,0);
        } else {
          keylessSilence = true;
          EEPROM.write(27,1);
        }
        EEPROM.commit();
        configTop = (String)mainSettingsFireAlarmSettings[4]+keylessSilence;
        configBottom = (String)mainSettingsFireAlarmSettings[0];
        configTop.replace("1","*");
        configTop.replace("0","$");
      }
    }
//----------------------------------------------------------------------------- SETTINGS > FIRE ALARM

//----------------------------------------------------------------------------- SETTINGS > PANEL
  } else if (configPage == 8){
    if (resetPressed == true and resetStillPressed == false){
      if (cursorPosition == 0){
        cursorPosition = 1;
        configTop = (String)mainPanelSettings[1];
        if (lcdTimeout == 0){
          configBottom = (String)mainPanelSettings[2] + "off";
        } else if (lcdTimeout<=30000) {
          configBottom = (String)mainPanelSettings[2] + lcdTimeout/1000+"s";
        } else {
          configBottom = (String)mainPanelSettings[2] + lcdTimeout/60000+"m";
        }
      } else if (cursorPosition == 1) {
        cursorPosition = 2;
        if (lcdTimeout == 0){
          configTop = (String)mainPanelSettings[2] + "off";
        } else if (lcdTimeout<=30000) {
          configTop = (String)mainPanelSettings[2] + lcdTimeout/1000+"s";
        } else {
          configTop = (String)mainPanelSettings[2] + lcdTimeout/60000+"m";
        }
        configBottom = (String)mainPanelSettings[3];

      } else if (cursorPosition == 2) {
        cursorPosition = 3;
        configTop = (String)mainPanelSettings[3];
        configBottom = (String)mainPanelSettings[4];
      } else if (cursorPosition == 3) {
        cursorPosition = 4;
        configTop = (String)mainPanelSettings[4];
        configBottom = (String)mainPanelSettings[0];
      } else if (cursorPosition == 4) {
        cursorPosition = 0;
        configTop = (String)mainPanelSettings[0];
        configBottom = (String)mainPanelSettings[1];
      }
    } else if (silencePressed == true and silenceStillPressed == false){
      configPage = 2;
      cursorPosition = 1;
      configTop = (String)mainSettings[1];
      configBottom = (String)mainSettings[0];
    } else if (drillPressed == true and drillStillPressed == false){
      if (cursorPosition == 0){
        configPage = 10;
        cursorPosition = 0;
        configTop = "Enter Name:";
        configBottom = (String)panelName;
        lcd.blink_on();

      } else if (cursorPosition == 1) {
        configPage = 9;
        cursorPosition = 0;
        if (keyRequiredVisual == true){
          configTop = (String)mainPanelSettingsPanelSecurity[0];
          configBottom = (String)mainPanelSettingsPanelSecurity[1]+"*";
        } else {
          configTop = (String)mainPanelSettingsPanelSecurity[0]+"*";
          configBottom = (String)mainPanelSettingsPanelSecurity[1];
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
          configTop = (String)mainPanelSettings[2] + "off";
        } else if (lcdTimeout<=30000) {
          configTop = (String)mainPanelSettings[2] + lcdTimeout/1000+"s";
        } else {
          configTop = (String)mainPanelSettings[2] + lcdTimeout/60000+"m";
        }
      } else if (cursorPosition == 3){
        configBottom = "drill = yes";
        configTop = "silence = no";
        configPage = -1;
      } else if (cursorPosition == 4){
        configPage = 12;
        cursorPosition = 0;
        configTop = (String)mainPanelSettingsAbout[0];
        configBottom = (String)mainPanelSettingsAbout[1] + firmwareRev;
      }
    }
//----------------------------------------------------------------------------- SETTINGS > PANEL

//----------------------------------------------------------------------------- SETTINGS > PANEL > PANEL NAME
  } else if (configPage == 10){ //panel rename routine 
    
    if (resetPressed == true and resetStillPressed == false){
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
    } else if (resetPressed==true and resetStillPressed==true) {
      clearTimer++;
      if (clearTimer >= 35) { //clear character if held down
        panelNameList[cursorPosition] = 32;
      }
    } else if (silencePressed == true and silenceStillPressed == false){
      int x=0;
      for (int i=11; i<=26; i++){ //write new panel name
        EEPROM.write(i,panelNameList[x]);
        x++;
      }
      lcd.blink_off();
      EEPROM.commit();
      configPage = 8;
      cursorPosition = 0;
      configTop = (String)mainPanelSettings[0];
      configBottom = (String)mainPanelSettings[1];
      x=0;
      panelName = "";
      for (int i=11; i<=26; i++){ //read panel name
        if (EEPROM.read(i) != 0){
          panelName = panelName + (char)EEPROM.read(i);
          panelNameList[x] = EEPROM.read(i);
          x++;
        }
      }
    } else if (drillPressed == true and drillStillPressed == false){
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
    configBottom = "drill = yes";
    configTop = "silence = no";
    if (silencePressed == true and silenceStillPressed == false){
      configPage = 8;
      cursorPosition = 3;
      configTop = (String)mainPanelSettings[3];
      configBottom = (String)mainPanelSettings[4];
    } else if (drillPressed == true and drillStillPressed == false){
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
    if (resetPressed == true and resetStillPressed == false){
      if (cursorPosition == 0){
        cursorPosition = 1;
        if (codeWheel == 1){
          configTop = (String)mainSettingsFireAlarmSettingsCoding[1]+"*";
        } else{
          configTop = (String)mainSettingsFireAlarmSettingsCoding[1];
        }
        if (codeWheel == 2){
          configBottom = (String)mainSettingsFireAlarmSettingsCoding[2]+"*";
        } else {
          configBottom = (String)mainSettingsFireAlarmSettingsCoding[2];
        }
      } else if (cursorPosition == 1) {
        cursorPosition = 2;
        if (codeWheel == 2){
          configTop = (String)mainSettingsFireAlarmSettingsCoding[2]+"*";
        } else {
          configTop = (String)mainSettingsFireAlarmSettingsCoding[2];
        }
        if (codeWheel == 3){
          configBottom = (String)mainSettingsFireAlarmSettingsCoding[3]+"*";
        } else {
          configBottom = (String)mainSettingsFireAlarmSettingsCoding[3];
        }
      } else if (cursorPosition == 2) {
        cursorPosition = 3;
        if (codeWheel == 3){
          configTop = (String)mainSettingsFireAlarmSettingsCoding[3]+"*";
        } else {
          configTop = (String)mainSettingsFireAlarmSettingsCoding[3];
        }
        if (codeWheel == 4){
          configBottom = (String)mainSettingsFireAlarmSettingsCoding[4]+"*";
        } else {
          configBottom = (String)mainSettingsFireAlarmSettingsCoding[4];
        }
      } else if (cursorPosition == 3) {
        cursorPosition = 4;
        if (codeWheel == 4){
          configTop = (String)mainSettingsFireAlarmSettingsCoding[4]+"*";
        } else {
          configTop = (String)mainSettingsFireAlarmSettingsCoding[4];
        }
        if (codeWheel == 5){
          configBottom = (String)mainSettingsFireAlarmSettingsCoding[5]+"*";
        } else {
          configBottom = (String)mainSettingsFireAlarmSettingsCoding[5];
        }
      } else if (cursorPosition == 4) {
        cursorPosition = 5;
        if (codeWheel == 5){
          configTop = (String)mainSettingsFireAlarmSettingsCoding[5]+"*";
        } else {
          configTop = (String)mainSettingsFireAlarmSettingsCoding[5];
        }
        if (codeWheel == 0){
          configBottom = (String)mainSettingsFireAlarmSettingsCoding[0]+"*";
        } else {
          configBottom = (String)mainSettingsFireAlarmSettingsCoding[0];
        }
      } else if (cursorPosition == 5) {
        cursorPosition = 0;
        if (codeWheel == 0){
          configTop = (String)mainSettingsFireAlarmSettingsCoding[0]+"*";
        } else {
          configTop = (String)mainSettingsFireAlarmSettingsCoding[0];
        }
        if (codeWheel == 1){
          configBottom = (String)mainSettingsFireAlarmSettingsCoding[1]+"*";
        } else{
          configBottom = (String)mainSettingsFireAlarmSettingsCoding[1];
        }
      }
    } else if (silencePressed == true and silenceStillPressed == false){
      configPage = 3; //change screen to facp settings
      cursorPosition = 0;
      configTop = (String)mainSettingsFireAlarmSettings[0];
      configBottom = (String)mainSettingsFireAlarmSettings[1];
    } else if (drillPressed == true and drillStillPressed == false){
      EEPROM.write(7,cursorPosition); //write the new codewheel settings to eeprom
      EEPROM.commit();
      configTop = (String)mainSettingsFireAlarmSettingsCoding[cursorPosition]+"*";
      if (cursorPosition == 5){
        configBottom = (String)mainSettingsFireAlarmSettingsCoding[0];
      } else {
        configBottom = (String)mainSettingsFireAlarmSettingsCoding[cursorPosition+1];
      }
      codeWheel = EEPROM.read(7); //codeWheel setting address
    }
//----------------------------------------------------------------------------- SETTINGS > FIRE ALARM > CODING

//----------------------------------------------------------------------------- SETTINGS > FIRE ALARM > VERIFICATION
  } else if (configPage == 4){
    if (resetPressed == true and resetStillPressed == false){
      if (cursorPosition == 0){
        cursorPosition = 1;
        if (isVerification == false){
          configTop = (String)mainSettingsVerificationSettings[1] + "off";
        } else {
          configTop = (String)mainSettingsVerificationSettings[1] + (verificationTime/1000)+"s";
        }
        configBottom = (String)mainSettingsVerificationSettings[2] + smokeDetectorVerification;
        configBottom.replace("1","*");
        configBottom.replace("0","$");
      } else if (cursorPosition == 1) {
        cursorPosition = 2;
        configTop = (String)mainSettingsVerificationSettings[2] + smokeDetectorVerification;
        if (smokeDetectorVerification == true){
          if (smokeDetectorVerificationTime<60000){
            configBottom = (String)mainSettingsVerificationSettings[3] + (smokeDetectorVerificationTime/1000) + "s";
          } else {
            configBottom = (String)mainSettingsVerificationSettings[3] + (smokeDetectorVerificationTime/60000) + "m";
          }
        } else {
          configBottom = (String)mainSettingsVerificationSettings[3] + "off";
        }
        configTop.replace("1","*");
        configTop.replace("0","$");
      } else if (cursorPosition == 2) {
        cursorPosition = 3;
        if (smokeDetectorVerification == true){
          if (smokeDetectorVerificationTime<60000){
            configTop = (String)mainSettingsVerificationSettings[3] + (smokeDetectorVerificationTime/1000) + "s";
          } else {
            configTop = (String)mainSettingsVerificationSettings[3] + (smokeDetectorVerificationTime/60000) + "m";
          }
        } else {
          configTop = (String)mainSettingsVerificationSettings[3] + "off";
        }
        configBottom = (String)mainSettingsVerificationSettings[0] + isVerification;
        configBottom.replace("1","*");
        configBottom.replace("0","$");
      } else if (cursorPosition == 3) {
        cursorPosition = 0;
        configTop = (String)mainSettingsVerificationSettings[0] + isVerification;
        if (isVerification == false){
          configBottom = (String)mainSettingsVerificationSettings[1] + "off";
        } else {
          configBottom = (String)mainSettingsVerificationSettings[1] + (verificationTime/1000)+"s";
        }
        configTop.replace("1","*");
        configTop.replace("0","$");
      } 
    } else if (silencePressed == true and silenceStillPressed == false){
      configPage = 3;
      cursorPosition = 1;
      configTop = (String)mainSettingsFireAlarmSettings[1];
      configBottom = (String)mainSettingsFireAlarmSettings[2];
    } else if (drillPressed == true and drillStillPressed == false){
      if (cursorPosition == 0){
        if (isVerification == true){
          isVerification = false;
          EEPROM.write(9,0);
        } else {
          isVerification = true;
          EEPROM.write(9,1);
        }
        EEPROM.commit();
        configTop = (String)mainSettingsVerificationSettings[0]+isVerification;
        if (isVerification == false){
          configBottom = (String)mainSettingsVerificationSettings[1] + "off";
        } else {
          configBottom = (String)mainSettingsVerificationSettings[1] + (verificationTime/1000)+"s";
        }
        configTop.replace("1","*");
        configTop.replace("0","$");
      } else if (cursorPosition == 1 and isVerification == true) {
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
        if (isVerification == false){
          configTop = (String)mainSettingsVerificationSettings[1] + "off";
        } else {
          configTop = (String)mainSettingsVerificationSettings[1] + (verificationTime/1000)+"s";
        }
        configBottom = (String)mainSettingsVerificationSettings[2] + smokeDetectorVerification;
        configBottom.replace("1","*");
        configBottom.replace("0","$");
      } else if (cursorPosition == 2){
        if (smokeDetectorVerification == false){
          EEPROM.write(76,1); //enable pre-alarm
          smokeDetectorVerification = true;
        } else {
          EEPROM.write(76,0); //disable pre-alarm
          smokeDetectorVerification = false;
          smokeDetectorOn(true); //re-enable smoke detectors in the case that one turned off because it was in verification
          digitalWrite(alarmLed, LOW);
          smokeDetectorCurrentlyInVerification=false;
          smokeDetectorPostRestartTimer=0;
          smokeDetectorTimer=0;
        }
        EEPROM.commit();
        configTop = (String)mainSettingsVerificationSettings[2] + smokeDetectorVerification;
        if (smokeDetectorVerification == true){
          if (smokeDetectorVerificationTime<60000){
            configBottom = (String)mainSettingsVerificationSettings[3] + (smokeDetectorVerificationTime/1000) + "s";
          } else {
            configBottom = (String)mainSettingsVerificationSettings[3] + (smokeDetectorVerificationTime/60000) + "m";
          }
        } else {
          configBottom = (String)mainSettingsVerificationSettings[3] + "off";
        }
        configTop.replace("1","*");
        configTop.replace("0","$");
      } else if (cursorPosition == 3 and smokeDetectorVerification == true) {
        if (smokeDetectorVerificationTime == 5000){
          EEPROM.write(77,2);
          smokeDetectorVerificationTime = 10000;
        } else if (smokeDetectorVerificationTime == 10000){
          EEPROM.write(77,3);
          smokeDetectorVerificationTime = 15000;
        } else if (smokeDetectorVerificationTime == 15000){
          EEPROM.write(77,4);
          smokeDetectorVerificationTime = 20000;
        } else if (smokeDetectorVerificationTime == 20000){
          EEPROM.write(77,6);
          smokeDetectorVerificationTime = 30000;
        } else if (smokeDetectorVerificationTime == 30000){
          EEPROM.write(77,9);
          smokeDetectorVerificationTime = 45000;
        } else if (smokeDetectorVerificationTime == 45000){
          EEPROM.write(77,18);
          smokeDetectorVerificationTime = 60000;
        } else if (smokeDetectorVerificationTime == 60000){
          EEPROM.write(77,24);
          smokeDetectorVerificationTime = 120000;
        } else if (smokeDetectorVerificationTime == 120000){
          EEPROM.write(77,60);
          smokeDetectorVerificationTime = 300000;
        } else if (smokeDetectorVerificationTime == 300000){
          EEPROM.write(77,120);
          smokeDetectorVerificationTime = 600000;
        } else if (smokeDetectorVerificationTime == 600000){
          EEPROM.write(77,1);
          smokeDetectorVerificationTime = 5000;
        }
        EEPROM.commit();
        if (smokeDetectorVerificationTime<60000){
          configTop = (String)mainSettingsVerificationSettings[3] + (smokeDetectorVerificationTime/1000) + "s";
        } else {
          configTop = (String)mainSettingsVerificationSettings[3] + (smokeDetectorVerificationTime/60000) + "m";
        }
        configBottom = (String)mainSettingsVerificationSettings[0]+isVerification;
        configBottom.replace("1","*");
        configBottom.replace("0","$");
      }
  }
//----------------------------------------------------------------------------- SETTINGS > FIRE ALARM > VERIFICATION

//----------------------------------------------------------------------------- SETTINGS > PANEL > PANEL SECURITY
  } else if (configPage == 9){
    if (resetPressed == true and resetStillPressed == false){
      if (cursorPosition == 0){
        cursorPosition = 1;
        if (keyRequiredVisual == true){
          configTop = (String)mainPanelSettingsPanelSecurity[1]+"*";
          configBottom = (String)mainPanelSettingsPanelSecurity[0];
        } else {
          configTop = (String)mainPanelSettingsPanelSecurity[1];
          configBottom = (String)mainPanelSettingsPanelSecurity[0]+"*";
        }
      } else if (cursorPosition == 1) {
        cursorPosition = 0;
        if (keyRequiredVisual == true){
          configTop = (String)mainPanelSettingsPanelSecurity[0];
          configBottom = (String)mainPanelSettingsPanelSecurity[1]+"*";
        } else {
          configTop = (String)mainPanelSettingsPanelSecurity[0]+"*";
          configBottom = (String)mainPanelSettingsPanelSecurity[1];
        }
      }
    } else if (silencePressed == true and silenceStillPressed == false){
      configPage = 8;
      cursorPosition = 1;
      configTop = (String)mainPanelSettings[1];
      if (lcdTimeout == 0){
        configBottom = (String)mainPanelSettings[2] + "off";
      } else if (lcdTimeout<=30000) {
        configBottom = (String)mainPanelSettings[2] + lcdTimeout/1000+"s";
      } else {
        configBottom = (String)mainPanelSettings[2] + lcdTimeout/60000+"m";
      }
    } else if (drillPressed == true and drillStillPressed == false){
      if (cursorPosition == 0){
        EEPROM.write(8,0); //write the new keyswitch settings to eeprom
        EEPROM.commit();
        keyRequiredVisual = false;
        configTop = (String)mainPanelSettingsPanelSecurity[0]+"*";
        configBottom = (String)mainPanelSettingsPanelSecurity[1];
      } else if (cursorPosition == 1) {
        EEPROM.write(8,1); //write the new keyswitch settings to eeprom
        EEPROM.commit();
        keyRequiredVisual = true;
        configTop = (String)mainPanelSettingsPanelSecurity[1]+"*";
        configBottom = (String)mainPanelSettingsPanelSecurity[0];
      }
    }
//----------------------------------------------------------------------------- SETTINGS > PANEL > PANEL SECURITY

//----------------------------------------------------------------------------- SETTINGS > PANEL > ABOUT
  } else if (configPage == 12){
    if (resetPressed == true and resetStillPressed == false){
      if (cursorPosition == 0){
        cursorPosition = 1;
        configTop = (String)mainPanelSettingsAbout[1]+firmwareRev;
        configBottom = (String)mainPanelSettingsAbout[2];        
      } else if (cursorPosition == 1) {
        cursorPosition = 2;
        configTop = (String)mainPanelSettingsAbout[2];
        configBottom = (String)mainPanelSettingsAbout[0];   
      } else if (cursorPosition == 2) {
        cursorPosition = 0;
        configTop = (String)mainPanelSettingsAbout[0];
        configBottom = (String)mainPanelSettingsAbout[1]+firmwareRev;   
      }
    } else if (silencePressed == true and silenceStillPressed == false){
      configPage = 8;
      cursorPosition = 4;
      configTop = (String)mainPanelSettings[4];
      configBottom = (String)mainPanelSettings[0];
    }
//----------------------------------------------------------------------------- SETTINGS > PANEL > ABOUT

//----------------------------------------------------------------------------- SETTINGS > FIRE ALARM > PRE-ALARM
  } else if (configPage == 6){
    if (resetPressed == true and resetStillPressed == false){
      if (cursorPosition == 0){
        cursorPosition = 1;
        if (preAlarm == true){
          configTop = (String)mainSettingsFireAlarmSettingsPreAlarmSettings[1] + (firstStageTime/60000) + "m";
        } else {
          configTop = (String)mainSettingsFireAlarmSettingsPreAlarmSettings[1] + "off";
        }
        configBottom = (String)mainSettingsFireAlarmSettingsPreAlarmSettings[0] + preAlarm;
        configBottom.replace("1","*");
        configBottom.replace("0","$");

      } else if (cursorPosition == 1){
        cursorPosition = 0;
        configTop = (String)mainSettingsFireAlarmSettingsPreAlarmSettings[0]+preAlarm;
        if (preAlarm == true){
          configBottom = (String)mainSettingsFireAlarmSettingsPreAlarmSettings[1] + (firstStageTime/60000) + "m";
        } else {
          configBottom = (String)mainSettingsFireAlarmSettingsPreAlarmSettings[1] + "off";
        }
        configTop.replace("1","*");
        configTop.replace("0","$");
      }
    } else if (silencePressed == true and silenceStillPressed == false){
      configPage = 3;
      cursorPosition = 2;
      configTop = (String)mainSettingsFireAlarmSettings[2];
      configBottom = (String)mainSettingsFireAlarmSettings[3]+audibleSilence;
      configBottom.replace("1","*");
      configBottom.replace("0","$");
    } else if (drillPressed == true and drillStillPressed == false){
      if (cursorPosition == 0){
        if (preAlarm == false){
          EEPROM.write(74,1); //enable pre-alarm
          preAlarm = true;
        } else {
          EEPROM.write(74,0); //disable pre-alarm
          preAlarm = false;
        }
        EEPROM.commit();
        configTop = (String)mainSettingsFireAlarmSettingsPreAlarmSettings[0]+preAlarm;
        if (preAlarm == true){
          configBottom = (String)mainSettingsFireAlarmSettingsPreAlarmSettings[1] + (firstStageTime/60000) + "m";
        } else {
          configBottom = (String)mainSettingsFireAlarmSettingsPreAlarmSettings[1] + "off";
        }
        configTop.replace("1","*");
        configTop.replace("0","$");
      } else if (cursorPosition == 1 and preAlarm == true) {
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
        configTop = (String)mainSettingsFireAlarmSettingsPreAlarmSettings[1] + (firstStageTime/60000) + "m";
        configBottom = (String)mainSettingsFireAlarmSettingsPreAlarmSettings[0] + preAlarm;
        configBottom.replace("1","*");
        configBottom.replace("0","$");
      } 
    }
  }
//----------------------------------------------------------------------------- SETTINGS > FIRE ALARM > PRE-ALARM


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
  if (digitalRead(resetButtonPin) == HIGH){ //RESET BUTTON
    resetStillPressed = true;
  }
  if (digitalRead(silenceButtonPin) == HIGH){ //SILENCE BUTTON
    silenceStillPressed = true;
  }
  if (digitalRead(drillButtonPin) == HIGH){ //DRILL BUTTON
    drillStillPressed = true;
  }
// --------- UPDATE BUTTON VARIABLES
}

void lcdBacklight(){
  if (lcdTimeout!=0){
    if (lcdTimeout <= lcdTimeoutTimer and backlightOn == true){
      lcd.noBacklight();
      backlightOn = false;
    } else if (backlightOn == true) {
      lcdTimeoutTimer++;
    }
    if (drillPressed == true or silencePressed == true or resetPressed == true or fullAlarm == true or trouble == true or (keyInserted == true and keyRequired == true)){
      lcdTimeoutTimer = 0;
      if (backlightOn == false){
        lcd.backlight();
      }
      backlightOn = true;
    }
  }
}

void smokeDetector(){
  if (smokeDetectorTimer >= smokeDetectorVerificationTime){
    smokeDetectorOn(true);
    if (smokeDetectorPostRestartTimer >= 60000){
      smokeDetectorCurrentlyInVerification = false;
      currentScreen = -1;
      digitalWrite(alarmLed, LOW);
      smokeDetectorTimer = 0;
    } else {
      smokeDetectorPostRestartTimer++;
    } 
  } else {
    smokeDetectorTimer++;
  }
} 

void loop() {
  systemClock = millis(); //-------------------- SYSTEM CLOCK
  if (systemClock-lastPulse >= 1){

    lcdBacklight(); //------------------------------------------------------ CHECK LCD BACKLIGHT 

    checkDevices(); //------------------------------------------------------ CHECK ACTIVATING DEVICES
    
    troubleCheck(); //------------------------------------------------------ TROUBLE CHECK

    alarm(); //------------------------------------------------------------- ALARM CODEWHEEL

    if (smokeDetectorCurrentlyInVerification == true){
      smokeDetector();
    }

    if (keyCheckTimer >= 100){
      checkKey(); //---------------------------------------------------------- CHECK KEY 
      keyCheckTimer = 0;
    } else {
      keyCheckTimer++;
    }
    
    if (buttonCheckTimer >= 20){
      if (configMenu==false){
        if ((keyInserted == true and keyRequired == true) or (keyRequired==false)){
          checkButtons(); //check if certain buttons are pressed
        } else if (keylessSilence == true and fullAlarm == true and silenced == false){
          if (digitalRead(silenceButtonPin) == HIGH){
            digitalWrite(silenceLed, HIGH);
            digitalWrite(alarmLed, LOW);
            digitalWrite(readyLed, LOW);
            readyLedStatus = false;
            horn = false;
            if (audibleSilence == false){
              strobe = false;
            }
            silenced=true;
            noTone();
          }
        }
        lcdUpdate(); //------------------------------------------------------ UPDATE LCD DISPLAY

      } else if (configMenu==true) {
        if ((keyInserted == true and keyRequired == true) or (keyInserted==false and keyRequired==false)){
          config();
        }
      }
      buttonCheckTimer = 0;
    } else {
      buttonCheckTimer++;
    }

    lastPulse = millis(); //update last pulse
  }
}