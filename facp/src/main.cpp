#include <Arduino.h>
#include <EEPROM.h>
#include <LiquidCrystal_I2C.h>
#include <string.h>

char *firmwareRev = "1.1"; //VERSION
int EEPROMVersion = 1; //version control to rewrite eeprom after update
int EEPROMBuild = 1;


//----------------------------------------------------------------------------- RUNTIME VARIABLES
bool fullAlarm = false; //bool to control if this is a full alarm that requres a panel reset to clear
bool silenced = false;
bool keyInserted = false; //if the control panel has a key inserted
bool horn = false; //bool to control if the horns are active
bool strobe = false; //bool to control if the strobes are active
bool trouble = false; //bool to control if the panel is in trouble
bool troubleAck = false; //bool to control if the trouble is acknowledged
bool configMenu = false; //determine if the control panel is in the configuration menu
bool resetPressed = false;
bool silencePressed = false; //make sure that presses don't count more than once
bool drillPressed = false;
bool resetStillPressed = false;
bool silenceStillPressed = false; //make sure that presses don't count more than once
bool drillStillPressed = false;
bool updateScreen = false; //updating the screen in the config menu
bool possibleAlarm = false; //panel receieved 0 from pull station ciruit and is now investigating
bool definiteAlarm = false; //panel has investigated and determined that it was not a fluke
bool walkTest = false; //is the system in walk test
bool silentWalkTest = false;
bool backlightOn = true;
bool keyRequiredVisual; //variable for panel security
bool debug = false;
int characters[] = {32,45,46,47,48,49,50,51,52,53,54,55,56,57,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,80,81,82,83,84,85,86,87,88,89,90}; //characters allowed on name
int panelNameList[16];
int clearTimer = 0; //timer for keeping track of button holding for clearing character in the name editor
int verification = 0; //number to keep track of ms for verification
int drill = 0; //number to keep track of ms for drill
//------ void loop delays to lessen load
int deviceCheckTimer = 0;
int keyCheckTimer = 0;
//------ void loop delays to lessen load
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
int zerosCounted = 0; //verification variable
int walkTestCount = 0; //keep track of walk test activations
int zoneAlarm = 0; //which zone is in alarm 0 - none | 1 - zone 1 | 2 - zone 2 | 3 - zone 1 & 2 
String configTop; //configuration menu strings for lcd
String configBottom;
String currentConfigTop; //configuration menu strings for current lcd display
String currentConfigBottom;

//Default EEPROM values in the case that the EEPROM fails to load
bool keyRequired = false; //determine if key switch is required to operate buttons
bool isVerification = true; //is verification turned on
bool eolResistor = true; //is the EOL resistor enabled
bool preAlarm = false; //use pre-alarm?
bool smokeDetectorPreAlarm = false; //should smoke detectors activate first stage
bool audibleSilence = true;
int smokeDetectorTimeout = 5; //how long should smoke detector pre-alarm wait before cancelling the pre-alarm
int firstStageTime = 1; //time in minutes that first stage should last
int codeWheel = 0; //which alarm pattern to use, code-3 default
int verificationTime = 2500;
//int resistorLenience = 0; DEPRECATED
int panelHomescreen = 0;
int lcdTimeout = 0;
String panelName = "";
LiquidCrystal_I2C lcd(0x27,16,2);

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











//get resistor lenience by having zone 1, zone 2, and smoke on a output pin instead of +3.3v so you can measure the difference

//have it so holding down the drill button causes a visual indicator on the display along with blinking the alarm led really fast

//Add feature to calibrate activating, trouble, and normal voltages using the IO output instead of 3.3v

//maybe instead of looking for 0s for activation, simply look for a dip in voltage





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

  EEPROM.write(50, EEPROMVersion); //write current version and build
  EEPROM.write(51, EEPROMBuild); 
  Serial.println("Wrote EEPROM version and build");
  //EEPROM.write(72,125); //EOL lenience 500 by default (take the value stored and multiply by 4 to get actual value)
  //Serial.println("Set EOL lenience to 500");
  EEPROM.write(73,1); //EOL resistor is enabled by default
  Serial.println("Enabled EOL resistor");
  EEPROM.write(74,0); //pre-alarm disabled by default
  Serial.println("Disabled pre-alarm");
  EEPROM.write(75,1); //pre-alarm first-stage is 1 minute by default
  Serial.println("Set pre-alarm first stage to 1 minute");
  EEPROM.write(76,0); //smoke detector pre-alarm is disable by default
  Serial.println("Disabled smoke detector pre-alarm");
  EEPROM.write(77,5); //smoke detector timeout is five minutes by default
  Serial.println("Set smoke detector timeout to 5 minutes");
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
  lcd.print("I/O");
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
    lcd.print("ERROR 1");
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
  lcd.print("I/O-SLFTST");
  delay(100);

  Serial.println("Current EEPROM dump:"); //dump EEPROM into serial monitor
  for (int i=0; i<=1024; i++){
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
    smokeDetectorPreAlarm = true;
  } else {
    smokeDetectorPreAlarm = false;
  }
  if (EEPROM.read(79) == 1){
    audibleSilence = true;
  } else {
    audibleSilence = false;
  }
  smokeDetectorTimeout = EEPROM.read(77)*60000;
  firstStageTime = EEPROM.read(75)*60000;
  verificationTime = EEPROM.read(10)*10;
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
  lcd.print("I/O-SLFTST-CONFIG");
  delay(100);
  Serial.println("Config loaded");
  digitalWrite(readyLed, HIGH); //power on ready LED on startup
  digitalWrite(silenceLed, LOW);
  digitalWrite(alarmLed, LOW);
  digitalWrite(smokeDetectorRelay, LOW); //turn on smoke relay
  Serial.println("-=- STARTUP COMPLETE -=-");
}



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

void activateNAC(){
  horn = true;
  strobe = true;
  fullAlarm = true;
  silenced = false;
  configMenu = false;
  tone();
  digitalWrite(alarmLed, HIGH);
  digitalWrite(silenceLed, LOW);
}

void checkKey(){
  if (digitalRead(keySwitchPin) == HIGH){
    keyInserted = true;
  } else {
    keyInserted = false;
  }
}

//----------------------------------------------------------------------------- CHECK ACTIVATION DEVICES
void checkDevices(){
  if (debug == true){
    Serial.println(analogRead(zone1Pin));
  }
  if (walkTest == 0){
    if ((analogRead(zone1Pin) == 0 or analogRead(zone2Pin) == 0) and horn != true and silenced==false){
      possibleAlarm = true;
    } 

    if (possibleAlarm == true and horn != true and strobe != true and silenced==false and isVerification == true){ //verification code
      if (analogRead(zone1Pin) == 0 or analogRead(zone2Pin) == 0){
        zerosCounted++;
      }
      if (verification >= verificationTime){
        if (zerosCounted > 0.1*verificationTime and analogRead(zone1Pin) == 0 and analogRead(zone2Pin) == 0){
          definiteAlarm = true;
          possibleAlarm = false;
          zerosCounted = 0;
          verification = 0;
        } else {
          zerosCounted = 0;
          possibleAlarm = false;
          verification = 0;
        }
      } else {
        verification++;
      }
    }
  } else if (walkTest == true){
    if (analogRead(zone1Pin) == 0){// or analogRead(zone2Pin) == 0){
      walkTestCount++;
      walkTestSmokeDetectorTimer = 0;
      while (analogRead(zone1Pin) == 0) {// or analogRead(zone2Pin) <= resistorLenience) {
        digitalWrite(strobeRelay, LOW);
        if (silentWalkTest == false){
          digitalWrite(hornRelay, LOW);
        }
        digitalWrite(alarmLed, HIGH);
        walkTestSmokeDetectorTimer++;
        if (walkTestSmokeDetectorTimer >= 500){
          digitalWrite(smokeDetectorRelay, HIGH);
        }
        delay(1);
      }
      digitalWrite(strobeRelay, HIGH);
      if (silentWalkTest == false){
        digitalWrite(hornRelay, HIGH);
      }
      digitalWrite(alarmLed, LOW);
      currentScreen = -1;
      delay(250);
      digitalWrite(smokeDetectorRelay, LOW);
    }
  }

  if (definiteAlarm == true or (isVerification == false and analogRead(zone1Pin) == 0 and horn != true and silenced==false)){ //activate the horns and strobes after verification
    activateNAC();
    definiteAlarm = false;
  } else if ((analogRead(zone1Pin) >= 3000 or analogRead(zone2Pin) >= 3000) and eolResistor == true and troubleTimer >= 200) {
    trouble = true;
    troubleType=1;
  } else if ((analogRead(zone1Pin) >= 3000 or analogRead(zone2Pin) >= 3000) and eolResistor == true and troubleTimer <= 200){
    troubleTimer++;
  } else {
    troubleTimer = 0;
  }
}
//----------------------------------------------------------------------------- CHECK ACTIVATION DEVICES


void troubleCheck(){
  if (trouble == true){
    if (troubleLedTimer == 0){
      digitalWrite(readyLed, LOW);
      if (troubleAck == false and fullAlarm == false){
        noTone();
      }
    } else if (troubleLedTimer == 750){
      digitalWrite(readyLed, HIGH);
      if (troubleAck == false and fullAlarm == false){ //sound the buzzer if the trouble is not acked
        tone();
      }
    } else if (troubleLedTimer == 1500){
      troubleLedTimer = -1;
    }

    troubleLedTimer++;
  } else {
    if (walkTest == false){
      digitalWrite(readyLed, HIGH);
    }
    if (troubleLedTimer != 0){
      noTone();
    }
    troubleLedTimer=0;
  }
}

void reboot(){
  lcd.clear();
    lcd.setCursor(2,0);
    lcd.print("Resetting...");
    tone();
    digitalWrite(readyLed, HIGH); //ready LED
    digitalWrite(silenceLed, HIGH); //silence LED
    digitalWrite(alarmLed, HIGH); //alarm LED
    digitalWrite(hornRelay, HIGH); //horn
    digitalWrite(strobeRelay, HIGH); //strobe
    digitalWrite(smokeDetectorRelay, HIGH); //smoke relay
    lcd.backlight();
    delay(2500);
    noTone();
    digitalWrite(readyLed, LOW); //ready LED
    digitalWrite(silenceLed, LOW); //silence LED
    digitalWrite(alarmLed, LOW); //alarm LED  
    ESP.restart();
}

void checkButtons(){
  if (digitalRead(resetButtonPin) == HIGH){ //RESET BUTTON
    reboot();
  }

  if (digitalRead(silenceButtonPin) == HIGH){ //SILENCE BUTTON
    if (horn == true){ //if horns are not silenced, silence the horns
      digitalWrite(silenceLed, HIGH);
      digitalWrite(alarmLed, LOW);
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

  if (digitalRead(drillButtonPin) == HIGH and horn != true){ //DRILL BUTTON
    if (drill >= 5000){
      activateNAC();
    } else {
      drill++;
    }
    drillPressed = true;
  } else {
    drill = 0;
    drillPressed = false;
  }
}

//----------------------------------------------------------------------------- NAC ACTIVATION
void alarm(){
  if (strobe == true){
    digitalWrite(strobeRelay, LOW);
  }else{
    digitalWrite(strobeRelay,HIGH);
  }
  if (horn == true){
    if (codeWheel == 0){

      if (codeWheelTimer == 0){ //temporal code 3 
        digitalWrite(hornRelay, LOW);
      } else if (codeWheelTimer == 500) {
        digitalWrite(hornRelay, HIGH);
      } else if (codeWheelTimer == 1000) {
        digitalWrite(hornRelay, LOW);
      } else if (codeWheelTimer == 1500) {
        digitalWrite(hornRelay, HIGH);
      } else if (codeWheelTimer == 2000) {
        digitalWrite(hornRelay, LOW);
      } else if (codeWheelTimer == 2500) {
        digitalWrite(hornRelay, HIGH);
      } else if (codeWheelTimer == 4000) {
        codeWheelTimer = -1;
      }

      
    } else if (codeWheel == 1) {

      if (codeWheelTimer == 0){ //marchtime
        digitalWrite(hornRelay, LOW);
      } else if (codeWheelTimer == 250){
        digitalWrite(hornRelay, HIGH);
      } else if (codeWheelTimer == 500){
        codeWheelTimer = -1;
      }

    } else if (codeWheel == 2) { //4-4
      
      if (codeWheelTimer == 0) {
        digitalWrite(hornRelay, LOW);
      } else if (codeWheelTimer == 300) {
        digitalWrite(hornRelay, HIGH);
      } else if (codeWheelTimer == 600) {
        digitalWrite(hornRelay, LOW);
      } else if (codeWheelTimer == 900) {
        digitalWrite(hornRelay, HIGH);
      } else if (codeWheelTimer == 1200) {
        digitalWrite(hornRelay, LOW);
      } else if (codeWheelTimer == 1500) {
        digitalWrite(hornRelay, HIGH);
      } else if (codeWheelTimer == 1800) {
        digitalWrite(hornRelay, LOW);
      } else if (codeWheelTimer == 2100) {
        digitalWrite(hornRelay, HIGH);

      } else if (codeWheelTimer == 2850) {
        digitalWrite(hornRelay, LOW);
      } else if (codeWheelTimer == 3150) {
        digitalWrite(hornRelay, HIGH);
      } else if (codeWheelTimer == 3450) {
        digitalWrite(hornRelay, LOW);
      } else if (codeWheelTimer == 3750) {
        digitalWrite(hornRelay, HIGH);
      } else if (codeWheelTimer == 4050) {
        digitalWrite(hornRelay, LOW);
      } else if (codeWheelTimer == 4350) {
        digitalWrite(hornRelay, HIGH);
      } else if (codeWheelTimer == 4650) {
        digitalWrite(hornRelay, LOW);
      } else if (codeWheelTimer == 4950) {
        digitalWrite(hornRelay, HIGH);
      } else if (codeWheelTimer == 14950) {
        codeWheelTimer = -1;
      }
    
    } else if (codeWheel == 3) { //continuous
      digitalWrite(hornRelay, LOW);
    } else if (codeWheel == 5) {
      if (codeWheelTimer == 0){ //marchtime slower
        digitalWrite(hornRelay, LOW);
      } else if (codeWheelTimer == 500){
        digitalWrite(hornRelay, HIGH);
      } else if (codeWheelTimer == 1000){
        codeWheelTimer = -1;
      }

    } else if (codeWheel == 4) {
      if (codeWheelTimer == 0){ //california code
        digitalWrite(hornRelay, LOW);
      } else if (codeWheelTimer == 10000){
        digitalWrite(hornRelay, HIGH);
      } else if (codeWheelTimer == 15000){
        codeWheelTimer = -1;
      }

    }

    codeWheelTimer++;
    alarmLedTimer++;
    if (alarmLedTimer >= 750){
      if (digitalRead(25) == false){
        digitalWrite(alarmLed, HIGH);
        alarmLedTimer = 0;
      } else {
        digitalWrite(alarmLed, LOW);
        alarmLedTimer = 0;
      }
    }
  } else {
    digitalWrite(hornRelay, HIGH);
    codeWheelTimer = 0;
  }
}
//----------------------------------------------------------------------------- NAC ACTIVATION

void lcdUpdate(){
  if (trouble==false and fullAlarm==false and horn==false and strobe==false and walkTest == false and currentScreen != 0 and drill == 0){
      lcd.noAutoscroll();
      lcd.clear();
      lcd.setCursor(2,0);
      lcd.print("System Normal");
      lcd.setCursor(0,1);
      if (panelHomescreen == 0){
        lcd.print(panelName);
      } else if (panelHomescreen == 1){
        lcd.print(analogRead(zone1Pin));
      }
      currentScreen = 0;
  } else if (trouble==true){
    if (troubleType == 0 and currentScreen != 1){
      lcd.clear();
      lcd.setCursor(1,0);
      lcd.print("* Trouble *");
      lcd.setCursor(0,1);
      lcd.print("Unknown");
      currentScreen = 1;
    } else if (troubleType == 1 and currentScreen != 2){
      lcd.clear();
      lcd.setCursor(1,0);
      lcd.print("* Trouble *");
      lcd.setCursor(0,1);
      lcd.print("Ground Fault");
      currentScreen = 2;
    }
  } else if (fullAlarm == true and silenced == false and currentScreen != 3){
    lcd.clear();
    lcd.setCursor(1,0);
    lcd.print("* FIRE ALARM *");
    // lcd.setCursor(2,1);
    // lcd.print("Zone 1");
    currentScreen = 3;
  } else if (silenced == true and currentScreen != 4){
    lcd.clear();
    lcd.setCursor(1,0);
    lcd.print("* FIRE ALARM *");
    lcd.setCursor(1,1);
    lcd.print("-- SILENCED --");
    // lcd.setCursor(2,1);
    // lcd.print("Zone 1");
    currentScreen = 4;
  } else if (walkTest == true and currentScreen != 5) {
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("* Supervisory *");
    lcd.setCursor(0,1);
    if (silentWalkTest == false){
      lcd.print("Walk Test - "+(String)walkTestCount);
    } else {
      lcd.print("S. Wlk Test - "+(String)walkTestCount);
    }
    currentScreen = 5;
    digitalWrite(readyLed, LOW); //ready led off for walk test
  } else if (drillPressed == true and fullAlarm == false and horn == false and strobe == false and walkTest == false and currentScreen != 6) {
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("CONTINUE HOLDING");
    lcd.setCursor(1,1);
    lcd.print("TO START DRILL");
    currentScreen = 6;
  }
}
void config(){
  char *main[] = {"Testing","Settings"}; //menu 0
  char *mainTesting[] = {"Walk Test","Silent Wlk Test","Strobe Test"}; //menu 1
  char *mainSettings[] = {"Fire Alarm","Panel"}; //menu 2
  char *mainSettingsFireAlarmSettings[] = {"Coding","Verification","Pre-Alarm","Audible Sil.: "}; //menu 3
  char *mainSettingsVerificationSettings[] = {"Verification: ","Verif. Time"}; //menu 4
  char *mainSettingsFireAlarmSettingsCoding[] = {"Temporal Three","Marchtime","4-4","Continuous","California","Slow Marchtime"}; //menu 5
  char *mainSettingsFireAlarmSettingsPreAlarmSettings[] = {"Pre-Alarm: ","stage 1: ","Detector PreAlrm"}; //menu 6
  char *mainSettingsFireAlarmSettingsPreAlarmSettingsSmokeDetectorPreAlarmSettings[] = {"Det. PreAlrm: ","Det. 1st stge: ","Det. Tmeout: "}; //menu 7
  char *mainPanelSettings[] = {"Panel Name","Panel Security","LCD Dim:","Factory Reset","About","Calibration"}; //menu 8
  char *mainPanelSettingsPanelSecurity[] = {"None","Keyswitch","Passcode"}; //menu 9
  char *mainPanelSettingsPanelName[] = {"Enter Name:"}; //menu 10
  char *mainPanelSettingsAbout[] = {"ANTIGNEOUS OS","Firmware: ",firmwareRev,"by Lexzach"}; //menu 12
  char *mainPanelSettingsCalibrate[] = {"Activated Zone 1", "Normal Zone 1", "Trouble Zone 1", "Activated Zone 2", "Normal Zone 2", "Trouble Zone 2"}; //menu 13
  
  
  // char *mainPanelSettingsHomescreen[] = {"Panel Name", "Stats for Nerds"}; //menu 10
  // char *mainPanelSettingsHomescreenStatsForNerds[] = {"Zone Input Voltages"}; //menu 11
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
    } else if (drillPressed == true and drillStillPressed == false){
        if (cursorPosition == 0){
          walkTest = true;
          silentWalkTest = false;
          silencePressed = true;
          configMenu = false;
          currentScreen=-1;
          walkTestCount = 0;
        } else if (cursorPosition == 1) {
          walkTest = true;
          silentWalkTest = true;
          silencePressed = true;
          configMenu = false;
          currentScreen=-1;
          walkTestCount = 0;
        } else if (cursorPosition == 2) {
          if (strobe == false){
            strobe = true;
            configTop = (String)mainTesting[2]+" *";
          } else {
            strobe = false;
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
      cursorPosition = 0;
      configTop = (String)main[0];
      configBottom = (String)main[1];
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
      } else if (cursorPosition == 2) {
        cursorPosition = 3;
        configTop = (String)mainSettingsFireAlarmSettings[3]+audibleSilence;
        configBottom = (String)mainSettingsFireAlarmSettings[0];
      } else if (cursorPosition == 3) {
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
        configTop = (String)mainSettingsVerificationSettings[0]+isVerification;
        configBottom = (String)mainSettingsVerificationSettings[1];
      } else if (cursorPosition == 2) {
        configPage = 6;
        cursorPosition = 0;
        configTop = (String)mainSettingsFireAlarmSettingsPreAlarmSettings[0];
        configBottom = (String)mainSettingsFireAlarmSettingsPreAlarmSettings[1];
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
        configBottom = (String)mainSettingsFireAlarmSettings[0];
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
        configBottom = (String)mainPanelSettings[5];
      } else if (cursorPosition == 4) {
        cursorPosition = 5;
        configTop = (String)mainPanelSettings[5];
        configBottom = (String)mainPanelSettings[0];
      } else if (cursorPosition == 5) {
        cursorPosition = 0;
        configTop = (String)mainPanelSettings[0];
        configBottom = (String)mainPanelSettings[1];
      }
    } else if (silencePressed == true and silenceStillPressed == false){
      configPage = 2;
      cursorPosition = 0;
      configTop = (String)mainSettings[0];
      configBottom = (String)mainSettings[1];
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
        if (keyRequired == true){
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
        //firmware version
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
      if (clearTimer >= 1000) {
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
      cursorPosition = 0;
      configTop = (String)mainPanelSettings[0];
      configBottom = (String)mainPanelSettings[1];
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
        configTop = (String)mainSettingsVerificationSettings[1];
        configBottom = (String)mainSettingsVerificationSettings[0]+isVerification;
      } else if (cursorPosition == 1) {
        cursorPosition = 0;
        configTop = (String)mainSettingsVerificationSettings[0]+isVerification;
        configBottom = (String)mainSettingsVerificationSettings[1];
      }
    } else if (silencePressed == true and silenceStillPressed == false){
      configPage = 3;
      cursorPosition = 0;
      configTop = (String)mainSettingsFireAlarmSettings[0];
      configBottom = (String)mainSettingsFireAlarmSettings[1];
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
        configBottom = (String)mainSettingsVerificationSettings[1];
      } else if (cursorPosition == 1) {
        //computer do shit
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
      cursorPosition = 0;
      configTop = (String)mainPanelSettings[0];
      configBottom = (String)mainPanelSettings[1];
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
}

  
  // if (resetPressed == true and resetStillPressed == false){

  // } else if (silencePressed == true and silenceStillPressed == false){

  // } else if (drillPressed == true and drillStillPressed == false){
  
  // }

  if (configTop != currentConfigTop or configBottom != currentConfigBottom){
    lcd.clear();
    lcd.setCursor(0,0);
    if (configPage != -1){
      lcd.print("]" + configTop);
    } else {
      lcd.print(configTop);
    }
    lcd.setCursor(0,1);
    lcd.print(configBottom);
    currentConfigTop = configTop;
    currentConfigBottom = configBottom;
    if (configPage == 10){
      lcd.setCursor(cursorPosition,1);
    }
    
  }
  


  if (digitalRead(resetButtonPin) == HIGH){ //RESET BUTTON
    resetStillPressed = true;
  }
  if (digitalRead(silenceButtonPin) == HIGH){ //SILENCE BUTTON
    silenceStillPressed = true;
  }
  if (digitalRead(drillButtonPin) == HIGH){ //DRILL BUTTON
    drillStillPressed = true;
  }
}
void loop() {
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
  delay(1);

//------------------------------------------------------ CHECK KEY 
  if (keyCheckTimer >= 10){
    checkKey(); 
    keyCheckTimer = 0;
  } else {
    keyCheckTimer++;
  }
//------------------------------------------------------ CHECK KEY 

//------------------------------------------------------ CHECK ACTIVATING DEVICES
  if (deviceCheckTimer >= 10){
    checkDevices(); //check pull stations and smoke detectors
    deviceCheckTimer = 0;
  } else {
    deviceCheckTimer++;
  }
//------------------------------------------------------ CHECK ACTIVATING DEVICES

//------------------------------------------------------ CHECK BUTTONS
  if ((keyInserted == true or keyRequired == false) and configMenu == false){
    checkButtons(); //check if certain buttons are pressed
  }
  troubleCheck(); //trouble check
  alarm(); //alarm codewheel
  if (configMenu==false){
    lcdUpdate();
  } else if (configMenu==true) {
    if (keyInserted == true or keyRequired == false){
      config();
    }
  }
//------------------------------------------------------ CHECK BUTTONS
}



