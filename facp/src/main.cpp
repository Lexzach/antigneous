#include <Arduino.h>
#include <EEPROM.h>
#include <LiquidCrystal_I2C.h>

//RUNTIME VARIABLES
bool fullAlarm = false; //bool to control if this is a full alarm that requres a panel reset to clear
bool keyInserted = false; //if the control panel has a key inserted
bool horn = false; //bool to control if the horns are active
bool strobe = false; //bool to control if the strobes are active
bool trouble = false; //bool to control if the panel is in trouble
bool troubleAck = false; //bool to control if the trouble is acknowledged
int verification = 0; //number to keep track of ms for verification
int drill = 0; //number to keep track of ms for drill
int codeWheelTimer = 0; //code wheel timing variable
int troubleLedTimer = 0; //number to keep track of ms for trouble light
int alarmLedTimer = 0; //alarm led timer
int troubleType = 0; //trouble type 0 - general, 1 - eol resistor
int lcdUpdateTimer = 0; //update delay
int currentScreen = 0; //update display if previous screen is not the same

//CONFIG VARIABLES
bool keyRequired = true; //determine if key switch is required to operate buttons
bool isVerification = true; //is verification turned on
bool eolResistor = true; //is the EOL resistor enabled
int codeWheel = 0; //which alarm pattern to use, code-3 default
int verificationTime = 2500;
int resistorLenience = 0;
LiquidCrystal_I2C lcd(0x27,16,2);

void setup() {
  Serial.begin(115200); //begin serial
  Serial.println("Lexzach's Low-Cost FACP v1");
  Serial.println("Booting...");

  pinMode(13, OUTPUT); //horn
  pinMode(12, OUTPUT); //strobe
  pinMode(14, OUTPUT); //smoke relay
  pinMode(27, OUTPUT); //ready LED
  pinMode(26, OUTPUT); //silence LED
  pinMode(25, OUTPUT); //alarm LED
  pinMode(33, INPUT); //key switch
  pinMode(32, INPUT); //reset switch
  pinMode(35, INPUT); //silence switch
  pinMode(34, INPUT); //drill switch
  pinMode(15, INPUT); //pull station
  pinMode(4, OUTPUT); //buzzer
  
  pinMode(22, OUTPUT); //scl
  pinMode(21, OUTPUT); //sda

  Serial.println("Initializing LCD...");
  lcd.init(); //initialize LCD
  lcd.clear();
  lcd.backlight();
  lcd.setCursor(3,0);
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
    for (int i=25; i==71; i++){ //write all 0's from 25-71 address
      EEPROM.write(i,0);
    }
    EEPROM.write(72,0); //EOL lenience 0 by default
    EEPROM.write(73,1); //EOL resistor is enabled by default


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
  verificationTime = EEPROM.read(10)*100;
  resistorLenience = EEPROM.read(72);
  Serial.println("Config loaded");
  digitalWrite(27, HIGH); //power on ready LED on startup
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

void checkKey(){
  if (digitalRead(33) == HIGH){
    keyInserted = true;
  } else {
    keyInserted = false;
  }
}

void checkDevices(){

  if (analogRead(15) <= resistorLenience and horn != true){
    verification++;
  } else {
    verification = 0;
  }
  if (verification == verificationTime or isVerification == false){ //activate the horns and strobes after verification
    horn = true;
    strobe = true;
    fullAlarm = true;
    tone();
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
    tone();
    digitalWrite(27, HIGH); //ready LED
    digitalWrite(26, HIGH); //silence LED
    digitalWrite(25, HIGH); //alarm LED  
    delay(1500);
    noTone();
    ESP.restart();
  }

  if (digitalRead(35) == HIGH){ //SILENCE BUTTON
    if (horn == true){ //if horns are not silenced, silence the horns
      digitalWrite(26, HIGH);
      digitalWrite(25, LOW);
      horn = false;
    } else if (horn == false and strobe == true){ //if the horns are silenced and the button is pressed again, silence the buzzer
      noTone();
    } else if (horn == false and strobe == false and trouble == true){
      troubleAck = true;
      noTone();
    }
  }

  if (digitalRead(34) == HIGH and horn != true){ //DRILL BUTTON
    if (drill == 2000){
      horn = true;
      strobe = true;
      fullAlarm = true;
      tone();
    } else {
      drill++;
    }
  } else {
    drill = 0;
  }
}

void alarm(){
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
      } else if (codeWheelTimer == 5000){
        codeWheelTimer = -1;
      }

    }

    codeWheelTimer++;
    alarmLedTimer++;
    if (alarmLedTimer >= 750){
      if (digitalRead(25) == true){
        digitalWrite(25, LOW);
        alarmLedTimer = 0;
      } else {
        digitalWrite(25, HIGH);
        alarmLedTimer = 0;
      }
    }
  } else {
    digitalWrite(13, HIGH);
    codeWheelTimer = 0;
  }
}

void lcdUpdate(){
  if (trouble==false and fullAlarm==false and horn==false and strobe==false){
      lcd.clear();
      lcd.setCursor(2,0);
      lcd.print("System Normal");
      currentScreen = 0;
  } else if (trouble==true){
    if (troubleType == 0 and currentScreen != 1){
      lcd.clear();
      lcd.setCursor(1,0);
      lcd.print("*** Trouble ***");
      lcd.setCursor(2,1);
      lcd.print("Unknown Trouble");
      currentScreen = 1;
    } else if (troubleType == 1 and currentScreen != 2){
      lcd.clear();
      lcd.setCursor(1,0);
      lcd.print("*** Trouble ***");
      lcd.setCursor(2,0);
      lcd.print("Device removed or EOL resistor not installed");
      currentScreen = 2;
    } else if (fullAlarm == true and currentScreen != 3){
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("*** FIRE ALARM ***");
      lcd.setCursor(2,1);
      lcd.print("Zone 1");
      currentScreen = 3;
    }
  }
}
void loop() {
  delay(1);
  checkKey(); //check to see if the key is inserted
  checkDevices(); //check pull stations and smoke detectors
  if (keyInserted == true or keyRequired == false){
    checkButtons(); //check if certain buttons are pressed
  }
  troubleCheck(); //trouble check
  alarm(); //alarm codewheel
  if (lcdUpdateTimer == 1000){
    lcdUpdate();
    lcdUpdateTimer=0;
  } else {
    lcdUpdateTimer++;
  }
  Serial.println(analogRead(15));
}



