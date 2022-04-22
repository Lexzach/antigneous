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
int verification = 0; //number to keep track of ms for verification
int drill = 0; //number to keep track of ms for drill
int codeWheelTimer = 0; //code wheel timing variable
int troubleLedTimer = 0; //number to keep track of ms for trouble light
int alarmLedTimer = 0; //alarm led timer
int troubleType = 0; //trouble type 0 - general, 1 - eol resistor
int lcdUpdateTimer = 0; //update delay
int currentScreen = -1; //update display if previous screen is not the same
int configPage = 0; //config page for the config menu
String configTop; //configuration menu strings for lcd
String configBottom;
String currentConfigTop; //configuration menu strings for current lcd display
String currentConfigBottom;

//CONFIG VARIABLES
bool keyRequired = true; //determine if key switch is required to operate buttons
bool isVerification = true; //is verification turned on
bool eolResistor = true; //is the EOL resistor enabled
bool preAlarm = false; //use pre-alarm?
bool smokeDetectorPreAlarm = false; //should smoke detectors activate first stage
int smokeDetectorTimeout = 5; //how long should smoke detector pre-alarm wait before cancelling the pre-alarm
int firstStageTime = 1; //time in minutes that first stage should last
int codeWheel = 0; //which alarm pattern to use, code-3 default
int verificationTime = 2500;
int resistorLenience = 0;
int panelHomescreen = 0;
String panelName = "";
LiquidCrystal_I2C lcd(0x27,16,2);

void setup() {
  Serial.begin(115200); //begin serial
  Serial.println("Lexzach's Low-Cost FACP v1");
  Serial.println("Booting...");

  pinMode(13, OUTPUT); //horn
  pinMode(18, OUTPUT); //strobe
  pinMode(14, OUTPUT); //smoke relay
  pinMode(27, OUTPUT); //ready LED D
  pinMode(26, OUTPUT); //silence LED D
  pinMode(25, OUTPUT); //alarm LED D
  pinMode(33, INPUT); //key switch D
  pinMode(32, INPUT); //reset switch D
  pinMode(35, INPUT); //silence switch D
  pinMode(34, INPUT); //drill switch D
  pinMode(15, INPUT); //pull station D
  pinMode(4, OUTPUT); //buzzer D
  digitalWrite(13, HIGH); //horn
  digitalWrite(18, HIGH); //strobe
  digitalWrite(14, HIGH); //smoke relay
  
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

  EEPROM.write(0,255); //TESTING PURPOSES
  EEPROM.commit(); //TESTING PURPOSES

  Serial.println("Verifying EEPROM configuration integrity...");

  if (EEPROM.read(0) != 76 or EEPROM.read(6) != 104 or EEPROM.read(7) > 3 or EEPROM.read(8) > 1){ //EEPROM verification, check essential-to-run components, listing all conditions that cannot exist in a correct EEPROM
    Serial.println("EEPROM verification failed. Re-writing EEPROM");
    for (int i=0; i<=1024; i++){ //write all 255's from 0-1024 address
      EEPROM.write(i,255);
    }

    EEPROM.write(0,76);EEPROM.write(1,101);EEPROM.write(2,120);EEPROM.write(3,122);EEPROM.write(4,97);EEPROM.write(5,99);EEPROM.write(6,104); //write Lexzach to addresses 0-6
    EEPROM.write(7,0); //code-3 default
    EEPROM.write(8,0); //key not required by default
    EEPROM.write(9,1); //verification is turned on by default
    EEPROM.write(10,25); //default verification time of 2.5 seconds

    //system name "[DEFAULT NAME]"
    EEPROM.write(11,91);EEPROM.write(12,68);EEPROM.write(13,69);EEPROM.write(14,70);EEPROM.write(15,65);EEPROM.write(16,85);EEPROM.write(17,76);EEPROM.write(18,84);EEPROM.write(19,32);EEPROM.write(20,78);EEPROM.write(21,65);EEPROM.write(22,77);EEPROM.write(23,69);EEPROM.write(24,93);
    for (int i=25; i<=71; i++){ //write all 0's from 25-71 address
      EEPROM.write(i,0);
    }
    EEPROM.write(72,0); //EOL lenience 0 by default
    EEPROM.write(73,1); //EOL resistor is enabled by default
    EEPROM.write(74,0); //pre-alarm disabled by default
    EEPROM.write(75,1); //pre-alarm first-stage is 1 minute by default
    EEPROM.write(76,0); //smoke detector pre-alarm is disable by default
    EEPROM.write(77,5); //smoke detector timeout is five minutes by default
    EEPROM.write(78,0); //homescreen is panel name by default



    EEPROM.commit();
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
  smokeDetectorTimeout = EEPROM.read(77)*60000;
  firstStageTime = EEPROM.read(75)*60000;
  verificationTime = EEPROM.read(10)*100;
  resistorLenience = EEPROM.read(72);
  panelHomescreen = EEPROM.read(78);
  for (int i=11; i<=71; i++){ //read panel name
      if (EEPROM.read(i) != 0){
        panelName = panelName + (char)EEPROM.read(i);
      }
    }

  Serial.println("Config loaded");
  digitalWrite(27, HIGH); //power on ready LED on startup
  digitalWrite(26, LOW);
  digitalWrite(25, LOW);
  digitalWrite(14, LOW); //turn on smoke relay
}

void tone() {
  ledcSetup(0, 5000, 8); // setup beeper
  ledcAttachPin(4, 0); // attach beeper
  ledcWriteTone(0, 1500); // play tone
}
void noTone() {
  ledcSetup(0, 5000, 8); // setup beeper
  ledcAttachPin(4, 0); // attach beeper
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

  if (analogRead(15) <= resistorLenience and horn != true and silenced==false){
    verification++;
  } else {
    verification = 0;
  }
  if (verification == verificationTime or isVerification == false){ //activate the horns and strobes after verification
    activateNAC();
  } else if (analogRead(15) == 4095 and eolResistor == true) {
    trouble = true;
    troubleType=1;
  }

}

void troubleCheck(){
  if (trouble == true){
    if (troubleLedTimer == 0){
      digitalWrite(27, LOW);
      if (troubleAck == false and fullAlarm == false){
        noTone();
      }
    } else if (troubleLedTimer == 750){
      digitalWrite(27, HIGH);
      if (troubleAck == false and fullAlarm == false){ //sound the buzzer if the trouble is not acked
        tone();
      }
    } else if (troubleLedTimer == 1500){
      troubleLedTimer = -1;
    }

    troubleLedTimer++;
  } else {
    digitalWrite(27, HIGH);
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
    digitalWrite(27, HIGH); //ready LED
    digitalWrite(26, HIGH); //silence LED
    digitalWrite(25, HIGH); //alarm LED  
    digitalWrite(13, HIGH); //horn
    digitalWrite(18, HIGH); //strobe
    digitalWrite(14, HIGH); //smoke relay
    delay(2500);
    noTone();
    digitalWrite(27, LOW); //ready LED
    digitalWrite(26, LOW); //silence LED
    digitalWrite(25, LOW); //alarm LED  
    ESP.restart();
  }

  if (digitalRead(35) == HIGH){ //SILENCE BUTTON
    if (horn == true){ //if horns are not silenced, silence the horns
      digitalWrite(26, HIGH);
      digitalWrite(25, LOW);
      horn = false;
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
    digitalWrite(18, LOW);
  }else{
    digitalWrite(18,HIGH);
  }
  if (horn == true){
    if (codeWheel == 0){

      if (codeWheelTimer == 0){ //temporal code 3 
        digitalWrite(13, LOW);
      } else if (codeWheelTimer == 500) {
        digitalWrite(13, HIGH);
      } else if (codeWheelTimer == 1000) {
        digitalWrite(13, LOW);
      } else if (codeWheelTimer == 1500) {
        digitalWrite(13, HIGH);
      } else if (codeWheelTimer == 2000) {
        digitalWrite(13, LOW);
      } else if (codeWheelTimer == 2500) {
        digitalWrite(13, HIGH);
      } else if (codeWheelTimer == 4000) {
        codeWheelTimer = -1;
      }

      
    } else if (codeWheel == 1) {

      if (codeWheelTimer == 0){ //marchtime
        digitalWrite(13, LOW);
      } else if (codeWheelTimer == 250){
        digitalWrite(13, HIGH);
      } else if (codeWheelTimer == 500){
        codeWheelTimer = -1;
      }

    } else if (codeWheel == 2) { //4-4
      
      if (codeWheelTimer == 0) {
        digitalWrite(13, LOW);
      } else if (codeWheelTimer == 300) {
        digitalWrite(13, HIGH);
      } else if (codeWheelTimer == 600) {
        digitalWrite(13, LOW);
      } else if (codeWheelTimer == 900) {
        digitalWrite(13, HIGH);
      } else if (codeWheelTimer == 1200) {
        digitalWrite(13, LOW);
      } else if (codeWheelTimer == 1500) {
        digitalWrite(13, HIGH);
      } else if (codeWheelTimer == 1800) {
        digitalWrite(13, LOW);
      } else if (codeWheelTimer == 2100) {
        digitalWrite(13, HIGH);

      } else if (codeWheelTimer == 2850) {
        digitalWrite(13, LOW);
      } else if (codeWheelTimer == 3150) {
        digitalWrite(13, HIGH);
      } else if (codeWheelTimer == 3450) {
        digitalWrite(13, LOW);
      } else if (codeWheelTimer == 3750) {
        digitalWrite(13, HIGH);
      } else if (codeWheelTimer == 4050) {
        digitalWrite(13, LOW);
      } else if (codeWheelTimer == 4350) {
        digitalWrite(13, HIGH);
      } else if (codeWheelTimer == 4650) {
        digitalWrite(13, LOW);
      } else if (codeWheelTimer == 4950) {
        digitalWrite(13, HIGH);
      } else if (codeWheelTimer == 14950) {
        codeWheelTimer = -1;
      }
    
    } else if (codeWheel == 3) { //continuous
      digitalWrite(13, LOW);
    } else if (codeWheel == 5) {
      if (codeWheelTimer == 0){ //marchtime slower
        digitalWrite(13, LOW);
      } else if (codeWheelTimer == 500){
        digitalWrite(13, HIGH);
      } else if (codeWheelTimer == 1000){
        codeWheelTimer = -1;
      }

    } else if (codeWheel == 4) {
      if (codeWheelTimer == 0){ //california code
        digitalWrite(13, LOW);
      } else if (codeWheelTimer == 10000){
        digitalWrite(13, HIGH);
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
    digitalWrite(13, HIGH);
    codeWheelTimer = 0;
  }
}

void lcdUpdate(){
  if (trouble==false and fullAlarm==false and horn==false and strobe==false and currentScreen != 0){
      lcd.noAutoscroll();
      lcd.clear();
      lcd.setCursor(2,0);
      lcd.print("System Normal");
      lcd.setCursor(0,1);
      if (panelHomescreen == 0){
        lcd.print(panelName);
      } else if (panelHomescreen == 1){
        lcd.print(analogRead(15));
      }
      currentScreen = 0;
  } else if (trouble==true){
    if (troubleType == 0 and currentScreen != 1){
      lcd.clear();
      lcd.setCursor(1,0);
      lcd.print("* Trouble *");
      lcd.setCursor(2,1);
      lcd.print("Unknown Trouble");
      currentScreen = 1;
    } else if (troubleType == 1 and currentScreen != 2){
      lcd.clear();
      lcd.setCursor(1,0);
      lcd.print("* Trouble *");
      lcd.setCursor(2,0);
      lcd.print("Device removed or EOL resistor not installed");
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
  }
}
void config(){
  char *main[] = {"Testing","Settings"}; //menu 0
  char *mainTesting[] = {"Walk Test","Silent Walk Test","Strobe Test","Automatic System Test"}; //menu 1
  char *mainSettings[] = {"Fire Alarm Settings","Panel Settings"}; //menu 2
  char *mainSettingsFireAlarmSettings[] = {"Coding: ","Verification Settings","Pre-Alarm Settings","Audible Silence: "}; //menu 3
  char *mainSettingsVerificationSettings[] = {"Verification: ","Verification Time: "}; //menu 4
  char *mainSettingsFireAlarmSettingsCoding[] = {"Temporal Three","Marchtime","4-4","Continuous","California","Slower Marchtime"}; //menu 5
  char *mainSettingsFireAlarmSettingsPreAlarmSettings[] = {"Pre-Alarm: ","First Stage Time: ","Smoke Detector Pre-Alarm Settings"}; //menu 6
  char *mainSettingsFireAlarmSettingsPreAlarmSettingsSmokeDetectorPreAlarmSettings[] = {"Smoke Detector Pre-Alarm: ","Smoke Detector Starts First Stage: ","Smoke Detector Timeout: "}; //menu 7
  char *mainPanelSettings[] = {"Panel Name","Panel Security: ","Homescreen","LCD Timeout: "}; //menu 8
  char *mainPanelSettingsPanelSecurity[] = {"None","Keyswitch","Passcode"}; //menu 9
  char *mainPanelSettingsHomescreen[] = {"Panel Name", "Stats for Nerds"}; //menu 10
  char *mainPanelSettingsHomescreenStatsForNerds[] = {"Zone Input Voltages"}; //menu 11
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
    configTop = (String)main[0];
    configBottom = (String)main[1];
    if (silencePressed == true and silenceStillPressed == false){
      silencePressed = true;
      configMenu = false;
      currentScreen=-1;
    }
  }
  

  // char buffer1[configTop.length()];
  // char buffer2[configBottom.length()];
  // configTop.toCharArray(buffer1,configTop.length());
  // configBottom.toCharArray(buffer2,configBottom.length());


  if (configTop != currentConfigTop or configBottom != currentConfigBottom){
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("> " + configTop);
    lcd.setCursor(0,1);
    lcd.print(configBottom);
    currentConfigTop = configTop;
    currentConfigBottom = configBottom;
  
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
    // lcdUpdateTimer=0;
  } else if (configMenu==true) {
    if (keyInserted == true or keyRequired == false){
      config();
    }
  } 
  // else {
  //   lcdUpdateTimer++;
  // }
  
}



