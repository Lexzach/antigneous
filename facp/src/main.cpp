#include <Arduino.h>
#include <EEPROM.h>
#include <LiquidCrystal_I2C.h>
#include <string.h>

//RUNTIME VARIABLES
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
int characters[] = {32,45,46,47,48,49,50,51,52,53,54,55,56,57,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,80,81,82,83,84,85,86,87,88,89,90}; //characters allowed on name
int panelNameList[16];
int clearTimer = 0; //timer for keeping track of button holding for clearing character in the name editor
int verification = 0; //number to keep track of ms for verification
int drill = 0; //number to keep track of ms for drill
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

//CONFIG VARIABLES (Set these by default in case eeprom fails to load, and it cannot be reset. Allows the FACP to still run with a default configuration)
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
int resistorLenience = 0;
int panelHomescreen = 0;
int lcdTimeout = 0;
String panelName = "";
LiquidCrystal_I2C lcd(0x27,16,2);

//PINS
int zone1Pin = 15;
int zone2Pin = 15; //TESTING is set to 15 but is normally 39
int hornPin = 13;
int buzzerPin = 4;
int strobePin = 18;
int smokeDetectorPin = 14;
int readyLedPin = 27;

void resetEEPROM() {
  for (int i=0; i<=1024; i++){ //write all 255's from 0-1024 address
      EEPROM.write(i,255);
    }

    EEPROM.write(0,76);EEPROM.write(1,101);EEPROM.write(2,120);EEPROM.write(3,122);EEPROM.write(4,97);EEPROM.write(5,99);EEPROM.write(6,104); //write Lexzach to addresses 0-6
    EEPROM.write(7,0); //code-3 default
    EEPROM.write(8,0); //key not required by default
    EEPROM.write(9,1); //verification is turned on by default
    EEPROM.write(10,25); //default verification time of 2.5 seconds

    //system name "ANTIGNEOUS"
    EEPROM.write(11,65);EEPROM.write(12,78);EEPROM.write(13,84);EEPROM.write(14,73);EEPROM.write(15,71);EEPROM.write(16,78);EEPROM.write(17,69);EEPROM.write(18,79);EEPROM.write(19,85);EEPROM.write(20,83);
    for (int i=21; i<=71; i++){ //write all 0's from 23-71 address
      EEPROM.write(i,0);
    }
    EEPROM.write(72,125); //EOL lenience 500 by default (take the value stored and multiply by 4 to get actual value)
    EEPROM.write(73,1); //EOL resistor is enabled by default
    EEPROM.write(74,0); //pre-alarm disabled by default
    EEPROM.write(75,1); //pre-alarm first-stage is 1 minute by default
    EEPROM.write(76,0); //smoke detector pre-alarm is disable by default
    EEPROM.write(77,5); //smoke detector timeout is five minutes by default
    EEPROM.write(78,0); //homescreen is panel name by default
    EEPROM.write(79,1); //audible silence is enabled by default
    EEPROM.write(80,0); //lcd timeout is disabled by default


    EEPROM.commit();
}

void setup() {
  Serial.begin(115200); //begin serial
  Serial.println("Lexzach's Low-Cost FACP v1");
  Serial.println("Booting...");

  pinMode(hornPin, OUTPUT); //horn
  pinMode(strobePin, OUTPUT); //strobe
  pinMode(smokeDetectorPin, OUTPUT); //smoke relay
  pinMode(readyLedPin, OUTPUT); //ready LED D
  pinMode(26, OUTPUT); //silence LED D
  pinMode(25, OUTPUT); //alarm LED D
  pinMode(33, INPUT); //key switch D
  pinMode(32, INPUT); //reset switch D
  pinMode(35, INPUT); //silence switch D
  pinMode(34, INPUT); //drill switch D
  
  pinMode(zone1Pin, INPUT); //zone 1
  pinMode(zone2Pin, INPUT); //zone 2

  pinMode(buzzerPin, OUTPUT); //buzzer D
  digitalWrite(hornPin, HIGH); //horn
  digitalWrite(strobePin, HIGH); //strobe
  digitalWrite(smokeDetectorPin, HIGH); //smoke relay
  
  pinMode(22, OUTPUT); //scl
  pinMode(21, OUTPUT); //sda

  Serial.println("Initializing LCD...");
  lcd.init(); //initialize LCD
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(4,0);
  lcd.print("Booting...");

  Serial.println("Configured all pins");
  

  EEPROM.begin(1025); //allocate memory address 0-1024 for EEPROM
  Serial.println("Configured EEPROM for addresses 0-1024");

  // EEPROM.write(0,255); //TESTING PURPOSES
  // EEPROM.commit(); //TESTING PURPOSES

  Serial.println("Verifying EEPROM configuration integrity...");

  if (EEPROM.read(0) != 76 or EEPROM.read(6) != 104 or EEPROM.read(7) > 5 or EEPROM.read(8) > 1){ //EEPROM verification, check essential-to-run components, listing all conditions that cannot exist in a correct EEPROM
    Serial.println("EEPROM verification failed. Re-writing EEPROM");
    resetEEPROM();
  } else {
    Serial.println("EEPROM verification finished");
  }
  
  Serial.println("Current EEPROM:");
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
  verificationTime = EEPROM.read(10)*100;
  resistorLenience = EEPROM.read(72)*4;
  panelHomescreen = EEPROM.read(78);
  lcdTimeout = EEPROM.read(80)*15000;
  int x=0;
  for (int i=11; i<=71; i++){ //read panel name
    if (EEPROM.read(i) != 0){
      panelName = panelName + (char)EEPROM.read(i);
      panelNameList[x] = EEPROM.read(i);
      x++;
    }
  }
  Serial.println("Config loaded");
  digitalWrite(readyLedPin, HIGH); //power on ready LED on startup
  digitalWrite(26, LOW);
  digitalWrite(25, LOW);
  digitalWrite(smokeDetectorPin, LOW); //turn on smoke relay
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
  digitalWrite(25, HIGH);
  digitalWrite(26, LOW);
}

void checkKey(){
  if (digitalRead(33) == HIGH){
    keyInserted = true;
  } else {
    keyInserted = false;
  }
}

void checkDevices(){
  
  if (walkTest == 0){
    if ((analogRead(zone1Pin) <= resistorLenience or analogRead(zone2Pin) <= resistorLenience) and horn != true and silenced==false){
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
      while (analogRead(zone1Pin) <= resistorLenience) {// or analogRead(zone2Pin) <= resistorLenience) {
        digitalWrite(strobePin, LOW);
        if (silentWalkTest == false){
          digitalWrite(hornPin, LOW);
        }
        digitalWrite(25, HIGH);
        walkTestSmokeDetectorTimer++;
        if (walkTestSmokeDetectorTimer >= 5000){
          digitalWrite(14, HIGH);
        }
        delay(1);
      }
      digitalWrite(strobePin, HIGH);
      if (silentWalkTest == false){
        digitalWrite(hornPin, HIGH);
      }
      digitalWrite(25, LOW);
      currentScreen = -1;
      delay(250);
      digitalWrite(smokeDetectorPin, LOW);
    }
  }

  if (definiteAlarm == true or (isVerification == false and analogRead(zone1Pin) <= resistorLenience and horn != true and silenced==false)){ //activate the horns and strobes after verification
    activateNAC();
    definiteAlarm = false;
  } else if (analogRead(zone1Pin) == 4095 and eolResistor == true and troubleTimer == 2000) {
    trouble = true;
    troubleType=1;
  } else if (analogRead(zone1Pin) == 4095 and eolResistor == true and troubleTimer <= 2000){
    troubleTimer++;
  } else {
    troubleTimer = 0;
  }
}

void troubleCheck(){
  if (trouble == true){
    if (troubleLedTimer == 0){
      digitalWrite(readyLedPin, LOW);
      if (troubleAck == false and fullAlarm == false){
        noTone();
      }
    } else if (troubleLedTimer == 750){
      digitalWrite(readyLedPin, HIGH);
      if (troubleAck == false and fullAlarm == false){ //sound the buzzer if the trouble is not acked
        tone();
      }
    } else if (troubleLedTimer == 1500){
      troubleLedTimer = -1;
    }

    troubleLedTimer++;
  } else {
    if (walkTest == false){
      digitalWrite(readyLedPin, HIGH);
    }
    if (troubleLedTimer != 0){
      noTone();
    }
    troubleLedTimer=0;
  }
}

void checkButtons(){
  if (digitalRead(32) == HIGH){ //RESET BUTTON
    lcd.clear();
    lcd.setCursor(2,0);
    lcd.print("Resetting...");
    tone();
    digitalWrite(readyLedPin, HIGH); //ready LED
    digitalWrite(26, HIGH); //silence LED
    digitalWrite(25, HIGH); //alarm LED  
    digitalWrite(hornPin, HIGH); //horn
    digitalWrite(strobePin, HIGH); //strobe
    digitalWrite(14, HIGH); //smoke relay
    delay(2500);
    noTone();
    digitalWrite(readyLedPin, LOW); //ready LED
    digitalWrite(26, LOW); //silence LED
    digitalWrite(25, LOW); //alarm LED  
    ESP.restart();
  }

  if (digitalRead(35) == HIGH){ //SILENCE BUTTON
    if (horn == true){ //if horns are not silenced, silence the horns
      digitalWrite(26, HIGH);
      digitalWrite(25, LOW);
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

  if (digitalRead(34) == HIGH and horn != true){ //DRILL BUTTON
    if (drill == 2000){
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

void alarm(){
  if (strobe == true){
    digitalWrite(strobePin, LOW);
  }else{
    digitalWrite(strobePin,HIGH);
  }
  if (horn == true){
    if (codeWheel == 0){

      if (codeWheelTimer == 0){ //temporal code 3 
        digitalWrite(hornPin, LOW);
      } else if (codeWheelTimer == 500) {
        digitalWrite(hornPin, HIGH);
      } else if (codeWheelTimer == 1000) {
        digitalWrite(hornPin, LOW);
      } else if (codeWheelTimer == 1500) {
        digitalWrite(hornPin, HIGH);
      } else if (codeWheelTimer == 2000) {
        digitalWrite(hornPin, LOW);
      } else if (codeWheelTimer == 2500) {
        digitalWrite(hornPin, HIGH);
      } else if (codeWheelTimer == 4000) {
        codeWheelTimer = -1;
      }

      
    } else if (codeWheel == 1) {

      if (codeWheelTimer == 0){ //marchtime
        digitalWrite(hornPin, LOW);
      } else if (codeWheelTimer == 250){
        digitalWrite(hornPin, HIGH);
      } else if (codeWheelTimer == 500){
        codeWheelTimer = -1;
      }

    } else if (codeWheel == 2) { //4-4
      
      if (codeWheelTimer == 0) {
        digitalWrite(hornPin, LOW);
      } else if (codeWheelTimer == 300) {
        digitalWrite(hornPin, HIGH);
      } else if (codeWheelTimer == 600) {
        digitalWrite(hornPin, LOW);
      } else if (codeWheelTimer == 900) {
        digitalWrite(hornPin, HIGH);
      } else if (codeWheelTimer == 1200) {
        digitalWrite(hornPin, LOW);
      } else if (codeWheelTimer == 1500) {
        digitalWrite(hornPin, HIGH);
      } else if (codeWheelTimer == 1800) {
        digitalWrite(hornPin, LOW);
      } else if (codeWheelTimer == 2100) {
        digitalWrite(hornPin, HIGH);

      } else if (codeWheelTimer == 2850) {
        digitalWrite(hornPin, LOW);
      } else if (codeWheelTimer == 3150) {
        digitalWrite(hornPin, HIGH);
      } else if (codeWheelTimer == 3450) {
        digitalWrite(hornPin, LOW);
      } else if (codeWheelTimer == 3750) {
        digitalWrite(hornPin, HIGH);
      } else if (codeWheelTimer == 4050) {
        digitalWrite(hornPin, LOW);
      } else if (codeWheelTimer == 4350) {
        digitalWrite(hornPin, HIGH);
      } else if (codeWheelTimer == 4650) {
        digitalWrite(hornPin, LOW);
      } else if (codeWheelTimer == 4950) {
        digitalWrite(hornPin, HIGH);
      } else if (codeWheelTimer == 14950) {
        codeWheelTimer = -1;
      }
    
    } else if (codeWheel == 3) { //continuous
      digitalWrite(hornPin, LOW);
    } else if (codeWheel == 5) {
      if (codeWheelTimer == 0){ //marchtime slower
        digitalWrite(hornPin, LOW);
      } else if (codeWheelTimer == 500){
        digitalWrite(hornPin, HIGH);
      } else if (codeWheelTimer == 1000){
        codeWheelTimer = -1;
      }

    } else if (codeWheel == 4) {
      if (codeWheelTimer == 0){ //california code
        digitalWrite(hornPin, LOW);
      } else if (codeWheelTimer == 10000){
        digitalWrite(hornPin, HIGH);
      } else if (codeWheelTimer == 15000){
        codeWheelTimer = -1;
      }

    }

    codeWheelTimer++;
    alarmLedTimer++;
    if (alarmLedTimer >= 750){
      if (digitalRead(25) == false){
        digitalWrite(25, HIGH);
        alarmLedTimer = 0;
      } else {
        digitalWrite(25, LOW);
        alarmLedTimer = 0;
      }
    }
  } else {
    digitalWrite(hornPin, HIGH);
    codeWheelTimer = 0;
  }
}

void lcdUpdate(){
  if (trouble==false and fullAlarm==false and horn==false and strobe==false and walkTest == false and currentScreen != 0){
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
      lcd.setCursor(2,1);
      lcd.print("Unknown");
      currentScreen = 1;
    } else if (troubleType == 1 and currentScreen != 2){
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("* Trouble *");
      lcd.setCursor(0,0);
      lcd.print("Ground Fault");
      lcd.autoscroll();
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
    digitalWrite(readyLedPin, LOW); //ready led off for walk test
  }
}
void config(){
  char *main[] = {"Testing","Settings"}; //menu 0
  char *mainTesting[] = {"Walk Test","Silent Wlk Test","Strobe Test"}; //menu 1
  char *mainSettings[] = {"Fire Alarm","Panel"}; //menu 2
  char *mainSettingsFireAlarmSettings[] = {"Coding","Verification","Pre-Alarm","Audible Sil.: "}; //menu 3
  char *mainSettingsVerificationSettings[] = {"Verification:","Verif. Time"}; //menu 4
  char *mainSettingsFireAlarmSettingsCoding[] = {"Temporal Three","Marchtime","4-4","Continuous","California","Slow Marchtime"}; //menu 5
  char *mainSettingsFireAlarmSettingsPreAlarmSettings[] = {"Pre-Alarm: ","stage 1: ","Detector PreAlrm"}; //menu 6
  char *mainSettingsFireAlarmSettingsPreAlarmSettingsSmokeDetectorPreAlarmSettings[] = {"Det. PreAlrm: ","Det. 1st stge: ","Det. Tmeout: "}; //menu 7
  char *mainPanelSettings[] = {"Panel Name","Panel Security","LCD Dim:","Factory Reset","Firmware Ver."}; //menu 8
  char *mainPanelSettingsPanelSecurity[] = {"None","Keyswitch","Passcode"}; //menu 9
  char *mainPanelSettingsPanelName[] = {"Enter Name:"}; //menu 10
  char *mainPanelSettingsFactoryReset[] = {"Are you sure?"}; //menu 11
  char *mainPanelSettingsAbout[] = {""}; //menu 12
  
  // char *mainPanelSettingsHomescreen[] = {"Panel Name", "Stats for Nerds"}; //menu 10
  // char *mainPanelSettingsHomescreenStatsForNerds[] = {"Zone Input Voltages"}; //menu 11
  if (digitalRead(32) == HIGH){ //RESET BUTTON
    resetPressed = true;
  } else {
    resetPressed = false;
    resetStillPressed = false;
  }
  if (digitalRead(35) == HIGH){ //SILENCE BUTTON
    silencePressed = true;
  } else {
    silencePressed = false;
    silenceStillPressed = false;
  }
  if (digitalRead(34) == HIGH){ //DRILL BUTTON
    drillPressed = true;
  } else {
    drillPressed = false;
    drillStillPressed = false;
  }
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
        configTop = (String)mainSettingsVerificationSettings[0];
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
        //panel security
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
        //factory reset
      } else if (cursorPosition == 4){
        //firmware version
      }
    }
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
      for (int i=11; i<=71; i++){ //read panel name
        if (EEPROM.read(i) != 0){
          panelName = panelName + (char)EEPROM.read(i);
          panelNameList[x] = EEPROM.read(i);
          x++;
        }
      }
    } else if (drillPressed == true and drillStillPressed == false){
      currentConfigTop = "bruh";
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
      EEPROM.write(7,cursorPosition);
      EEPROM.commit();
      configTop = (String)mainSettingsFireAlarmSettingsCoding[cursorPosition]+"*";
      if (cursorPosition == 5){
        configBottom = (String)mainSettingsFireAlarmSettingsCoding[0];
      } else {
        configBottom = (String)mainSettingsFireAlarmSettingsCoding[cursorPosition+1];
      }
      codeWheel = EEPROM.read(7); //codeWheel setting address
    }
  }
  
  // if (resetPressed == true and resetStillPressed == false){

  // } else if (silencePressed == true and silenceStillPressed == false){

  // } else if (drillPressed == true and drillStillPressed == false){
  
  // }

  if (configTop != currentConfigTop or configBottom != currentConfigBottom){
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("]" + configTop);
    lcd.setCursor(0,1);
    lcd.print(configBottom);
    currentConfigTop = configTop;
    currentConfigBottom = configBottom;
    if (configPage == 10){
      lcd.setCursor(cursorPosition,1);
    }
    
  }
  


  if (digitalRead(32) == HIGH){ //RESET BUTTON
    resetStillPressed = true;
  }
  if (digitalRead(35) == HIGH){ //SILENCE BUTTON
    silenceStillPressed = true;
  }
  if (digitalRead(34) == HIGH){ //DRILL BUTTON
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
    if ((drillPressed == true or silencePressed == true or resetPressed == true or fullAlarm == true or trouble == true) and backlightOn == false){
      lcdTimeoutTimer = 0;
      lcd.backlight();
      backlightOn = true;
    }
  }
  delay(1);
  checkKey(); //check to see if the key is inserted
  checkDevices(); //check pull stations and smoke detectors
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
}



