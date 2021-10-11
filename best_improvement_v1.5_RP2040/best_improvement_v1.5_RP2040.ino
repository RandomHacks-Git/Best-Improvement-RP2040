//uses core https://github.com/earlephilhower/arduino-pico 1.9.5, Generic RP2040 - all other settings untouched default

#include <Wire.h> //for touch ic
#include <EEPROM.h>
#include "ht1621.h" //LCD controller https://github.com/altLab/HT1621
#include "max6675.h" //https://learn.adafruit.com/thermocouple/arduino-code
#include <PID_v1.h> //https://github.com/br3ttb/Arduino-PID-Library 
//#define TIMER_INTERRUPT_DEBUG         1
//#define _TIMERINTERRUPT_LOGLEVEL_     4
#include "RPi_Pico_TimerInterrupt.h" //https://github.com/khoih-prog/RPI_PICO_TimerInterrupt
#include <RunningMedian.h> //https://github.com/RobTillaart/RunningMedian

//LCD section addresses
#define MAIN 17
#define LEFT 25
#define RIGHT 11
#define SMALL 8

//Touch buttons mapping
#define UP 128
#define DOWN 64
#define SET 32
#define CF 16

//Pins sda 4, scl 5
HT1621 ht(14, 13, 12, 11); // LCD data,wr,rd,cs
#define BACKLIGHT 2
#define TOUCHINT 3 //touch interrupt
#define PIEZO 0
#define REEDINT 1 //reed switch pin
#define BLOWER 7 //blower pin
#define HEATER 20 //heater pin
#define BTN1 8 //physical button 1
#define BTN2 9 //physical button 2
#define BTN3 10 //physical button 3
#define AHEAT 26 //temperature pot analog pin 
#define ABLOW 27 //blower pot analog pin                                          
#define ESCFEED 6

//thermocouple pins
#define THERMO_CLK 18
#define THERMO_CS 17
#define THERMO_DO 16
MAX6675 thermocouple(THERMO_CLK, THERMO_CS, THERMO_DO);

//Miscellaneous
#define FIRMWARE_VERSION 101 //firmware version (1.01)
#define DEBOUNCETIME 10 //button debounce time
#define LCDBRIGHTNESS 60 //default brightness, closer to 0 -> brighter, closer to 100 -> dimmer
#define LCDBRIGHTNESSDIM 100 //standby brightness
#define MINTEMP 100
#define MAXTEMP 550
#define MINBLOW 35
#define MAXBLOW 100
#define SHUTDOWNTEMP 85 //below this temp the blower shuts off
#define SETTINGSEXITTIME 8000 //if you are inside settings (blinking screen), it will automatically exit after this time (ms)
#define WINDOWSIZE 205 //Heater PWM period size (ms)
#define SERIALTIME 1000 //Serial output time (ms)
#define TIMER_FREQ_HZ 1

//Calibration factors (so the air that comes OUT OF THE NOZZLE actually reaches the set temperature)
#define RANGE200 1.05
#define RANGE300 1.08
#define RANGE400 1.11

// touch IC address / registers
#define touchAddress 0x56 // Device address
#define keyValues 0x34 // Hexadecimal address for the key values register
#define sysCon 0x3A //hex value for config register
#define kdr0 0x23 //hex value for key disable register
#define mcon 0x21 //hex value for mode control register
#define gsr 0x20 //hex address for global sensitivity register

volatile unsigned long touchMillis;
volatile bool touched, touchReleased = 1;

unsigned long lastTempPrint;
unsigned long lastTempRead;
unsigned long lastTempIcon;
unsigned long lastSerialOutput;
volatile bool reedStatus;
volatile bool btn1, btn2, btn3;
volatile unsigned long btnMillis, reedMillis;
volatile bool buttonFlag;
volatile bool toneFlag, longToneFlag;
volatile bool reedFlag;
volatile byte touchedButton;
volatile bool readTouchFlag;
unsigned long lastReact;
bool standby;
bool timer = false;
bool timeUnit = 1;
volatile bool timerFlag;
volatile bool setPointReached;
volatile byte setPointChanged = 2; //0 = no change, 1 = decreased, 2 = increased
bool converted;
bool newPotValue;
unsigned long potMillis;
unsigned long windowStartTime;
unsigned long setPointReachedTime;
unsigned long lastAnalogCheck;
volatile bool heating;
bool blowerOn;
volatile bool coolingAfterTimer;
volatile unsigned short timerTemporary;

unsigned short heaterVal, blowerVal; //analog pot values for heater and blower
unsigned short setTemp, setBlow, lastSetBlow, setTimer; //current set values
float currentTemp;

byte selectedSection;
unsigned long lastBlink;
bool sectionOff;
bool switchDisplayed;
bool displayingVersion = true;
bool eepromFlag;

struct chSettings {
  unsigned short temp;
  unsigned short blow;
};

struct otherSettings {
  bool tempUnit; //1 for ºC, 0 for ºF
  bool buzzer;
  byte selectedCh;
  short calTemp; //temperature calibration value, can be negative or positive
  bool serialOutput;
};

chSettings ch1Settings, ch2Settings, ch3Settings, touchSettings;
otherSettings otherSettings;

unsigned int eepromCheck = 1234567899; //used in setup to check if settings where previously saved to flash (emulated eeprom)

//PID
//P - how fast it shoots towards set point, if set too high creates overshoot, I - remove oscillations and offset, can increase overshoot D - like a break, removes overshoot, if set too high leads to unresponsiveness
double setPoint = 0, input = 0, output = 0;

double KP = 3.2, KI = 0.17, KD = 1.5; //<400ºC
double HIGH_KP = 3.2, HIGH_KI = 0.08, HIGH_KD = 0.32; //>=400ºC

PID myPID(&input, &output, &setPoint, KP, KI, KD, P_ON_E, DIRECT);

// Init RPI_PICO_Timer
RPI_PICO_Timer ITimer(3);

RunningMedian samples = RunningMedian(8); //used for the temperature potentiometer analogRead
RunningMedian samples2 = RunningMedian(8); //used for the blower potentiometer analogRead

void loadDefaults() {
  ch1Settings.temp = 200;
  ch1Settings.blow = 100;
  ch2Settings.temp = 360;
  ch2Settings.blow = 50;
  ch3Settings.temp = 500;
  ch3Settings.blow = 70;
  //    ch1Settings = {200, 100};
  //    ch2Settings = {360, 50};
  //    ch3Settings = {500, 70};
  otherSettings.tempUnit = 1;
  otherSettings.buzzer = 1;
  otherSettings.selectedCh = 1;
  otherSettings.calTemp = 7;
  otherSettings.serialOutput = 0;

  EEPROM.put(4, ch1Settings);
  EEPROM.put(8, ch2Settings);
  EEPROM.put(12, ch3Settings);
  EEPROM.put(16, touchSettings);
  EEPROM.put(20, otherSettings);
  EEPROM.put(30, eepromCheck);
  EEPROM.commit();
  Serial.println("Default settings loaded and saved to EEPROM");
  tone(PIEZO, 7000, 300);
}

void setup() {
  Serial.begin(115200);
  EEPROM.begin(128);
  analogWriteFreq(20000); //20kHz PWM freq
  analogWriteRange(100); //because I'm using percentages, this makes it easier

  //initialize pins
  pinMode(BACKLIGHT, OUTPUT);   //initialize backlight pin
  analogWrite(BACKLIGHT, 100); //backlight off until initializing LCD
  pinMode(PIEZO, OUTPUT);
  pinMode(BLOWER, OUTPUT);
  analogWrite(BLOWER, 60); //turn on blower to get a more accurate reading of the temperature (in case there was a short power failure or you turned off the station while it was still hot the blower will turn on to avoid damage to the heater element/handle) 
  pinMode(HEATER, OUTPUT);
  pinMode(AHEAT, INPUT);
  pinMode(ABLOW, INPUT);
  pinMode(BTN1, INPUT_PULLUP);
  pinMode(BTN2, INPUT_PULLUP);
  pinMode(BTN3, INPUT_PULLUP);
  pinMode(REEDINT, INPUT_PULLUP);
  pinMode(TOUCHINT, INPUT);

  bool defaultsLoaded = 0;
  delay(30); //let pins stabilize
  if (!digitalRead(BTN1) && !digitalRead(BTN3)) { //loads default settings if buttons 1 and 3 are pressed simultaneously while powering on the station
    loadDefaults();
    defaultsLoaded = true;
  }

  unsigned int tempEepromCheck;

  if (!defaultsLoaded) {
    EEPROM.get(30, tempEepromCheck);
    if (tempEepromCheck == eepromCheck) { //checks if the variable eepromCheck is already stored in flash, if it is,reads the settings else stores them.
      EEPROM.get(4, ch1Settings);
      EEPROM.get(8, ch2Settings);
      EEPROM.get(12, ch3Settings);
      EEPROM.get(16, touchSettings);
      EEPROM.get(20, otherSettings);
      Serial.println("Settings loaded from EEPROM");
    }
    else loadDefaults();
  }

  //initialize LCD
  ht.begin();
  ht.sendCommand(HT1621::BIAS_THIRD_4_COM);
  ht.sendCommand(HT1621::SYS_EN);

  ht.sendCommand(HT1621::LCD_ON); //turn lcd on

  for (int i = 0 ; i < 32 ; i++) { //turn all LCD segments off
    ht.writeMem(i, 0);
  }

  blowerVal = analogRead(ABLOW);
  heaterVal = analogRead(AHEAT);

  switch (otherSettings.selectedCh) {
    case 0: //pot
      defineBlower();
      defineTemp();
      break;
    case 1: //ch1
      setTemp = handleTempUnit(ch1Settings.temp, otherSettings.tempUnit);
      setBlow = ch1Settings.blow;
      printNumber(MAIN, setTemp);
      printNumber(LEFT, setBlow);
      break;
    case 2: //ch2
      setTemp = handleTempUnit(ch2Settings.temp, otherSettings.tempUnit);
      setBlow = ch2Settings.blow;
      printNumber(MAIN, setTemp);
      printNumber(LEFT, setBlow);
      break;
    case 3: //ch3
      setTemp = handleTempUnit(ch3Settings.temp, otherSettings.tempUnit);
      setBlow = ch3Settings.blow;
      printNumber(MAIN, setTemp);
      printNumber(LEFT, setBlow);
      break;
    case 4: //touch
      setTemp = handleTempUnit(touchSettings.temp, otherSettings.tempUnit);
      setBlow = touchSettings.blow;
      printNumber(MAIN, setTemp);
      printNumber(LEFT, setBlow);
  }

  //turn on touch keys segments
  ht.writeMem(6, 0b1111);

  //turn on fan icon
  ht.writeMem(31, 0b0010);

  //turn on clock icon
  changeSegment(24, 3, 1);

  //turn on "off" icon
  changeSegment(16, 3, 1);

  //turn on ºC or ºF on main display
  if (otherSettings.tempUnit)ht.writeMem(10, 0b0001); //ºC
  else ht.writeMem(10, 0b0010); //ºF

  printChannel(otherSettings.selectedCh); //Print channel to screen

  reedStatus = digitalRead(REEDINT); //check if handle is in base
  if (reedStatus) changeSegment(31, 2, 1); //handle out of base turn on "handle" icon
  else changeSegment(31, 2, 0); //handle in base turn off "handle" icon

  changeSegment(7, 1, otherSettings.serialOutput); //turn "RS232 On" segment on or off according to last setting

  printNumber(RIGHT, FIRMWARE_VERSION); //print firmware version
  changeSegment(14, 3, 1); //decimal point

  analogWrite(BACKLIGHT, LCDBRIGHTNESS); //turn on backlight at default brightness

  Wire.begin(); // Initiate the Wire library

  // Config APT8L08 (touch IC)
  Wire.beginTransmission(touchAddress);//(0x56);
  Wire.write(byte(sysCon));//(sysCon);
  Wire.write(byte(0x5A));//(90);// Set to config mode (0x5A)
  Wire.endTransmission();

  Wire.beginTransmission(touchAddress);
  Wire.write(byte(kdr0));//(kdr0);
  Wire.write(byte(15));// Disable first 4 inputs (0000 1111)
  Wire.endTransmission();

  Wire.beginTransmission(touchAddress);
  Wire.write(byte(mcon));//(mcon);
  Wire.write(byte(81));// interrupt changes when releasing key 81 (0101 0001) multitouch 85 (0101 0101), pulse interrupt + multitouch 93
  Wire.endTransmission();

  Wire.beginTransmission(touchAddress);
  Wire.write(byte(gsr));//(gsr);
  Wire.write(byte(0x01));// Lower sensitivity (0x01), chip default 0x02, max 0x0F
  Wire.endTransmission();

  Wire.beginTransmission(touchAddress);
  Wire.write(byte(sysCon));//(sysCon);
  Wire.write(byte(0x00));// Set to work mode (0x00)
  Wire.endTransmission();

  delay(550); //so temperature reaches the thermocouple for a more accurate reading
  currentTemp = readTemp(1);
  if (currentTemp > SHUTDOWNTEMP + 20) {
    analogWrite(BLOWER, 100); //turn on blower at max
    blowerOn = true;
  }
  else analogWrite(BLOWER, 0);

  //Setup interrupts
  attachInterrupt(digitalPinToInterrupt(TOUCHINT), touchAction, CHANGE);
  attachInterrupt(digitalPinToInterrupt(REEDINT), reedAction, CHANGE);
  attachInterrupt(digitalPinToInterrupt(BTN1), btnAction, CHANGE);
  attachInterrupt(digitalPinToInterrupt(BTN2), btnAction, CHANGE);
  attachInterrupt(digitalPinToInterrupt(BTN3), btnAction, CHANGE);

  myPID.SetOutputLimits(0, WINDOWSIZE); //PWM on time varies between 0ms and 205ms
  myPID.SetSampleTime(150); //PID refresh rate

  blowerVal = analogRead(ABLOW);
  heaterVal = analogRead(AHEAT);

}

void loop() {
  if (displayingVersion && millis() >= 1000) {
    if (timer) printNumber(RIGHT, setTimer);
    else {
      for ( byte i = 11; i < 16; i++) {
        ht.writeMem(i, 0);
      }
      changeSegment(16, 3, 1); //off icon
    }
    displayingVersion = false;
  }
  if (timerFlag) {
    printNumber(RIGHT, timerTemporary);
    timerFlag = false;
  }

  if (toneFlag) {
    tone(PIEZO, 7000, 100);
    toneFlag = false;
   }

  if (longToneFlag) {
    tone(PIEZO, 6000, 800);
    longToneFlag = false;
      }

  if (readTouchFlag) { //read which button was touched
    touchedButton = readTouch();
    readTouchFlag = false;
  }

  if (eepromFlag && millis() - potMillis > 100) { //this is used for defineBlower() and defineTemp() the reason I don't immediately write the channel setting to eeprom is because ocasionally it might happen that when turning the station off a change on the analog pins is detected and because there isn't enough time to write to eeprom before RP2040 completely loses power the flash ends up corrupted, having this delay solves the issue
    if (otherSettings.selectedCh == 0) { //check if the channel hasn't changed in the meantime
      EEPROM.put(20, otherSettings);
      EEPROM.commit();
    }
    eepromFlag = false;
  }

  if (reedFlag) {
    if (reedStatus) { //out of base
      timerTemporary = setTimer;
      changeSegment(31, 2, 1); //turn on iron icon
      analogWrite(BLOWER, setBlow); //turn blower on at current setting, map percentage to duty cycle
      myPID.SetMode(AUTOMATIC); //turn on PID
      windowStartTime = millis();
      heating = true;
      blowerOn = true;
      ht.writeMem(23, 0b1000); //thermometer icon on
    }
    else { //in base, reset LCD values and set blower to max
      ITimer.detachInterrupt();
      changeSegment(31, 2, 0); //turn off iron icon
      myPID.SetMode(MANUAL); //turn off PID
      output = 0;
      digitalWrite(HEATER, LOW);
      heating = false;
      setPointReached = false;
      setPointChanged = 2;
      coolingAfterTimer = false;
      if (readTemp(1) > SHUTDOWNTEMP && blowerOn) analogWrite(BLOWER, 100); //turns blower to full if the temperature is above the shutdown temperature and the blower isn't already off after using the timer
      ht.writeMem(23, 0); //turn of temp icon
      changeSegment(24, 0, 0); //turn off ºC
      changeSegment(24, 1, 0); //turn off ºF
      changeSegment(31, 0, 0); //turn off Set
      changeSegment(31, 1, 1); //turn on fan icon
      printNumber(LEFT, setBlow); //display set blower percentage
      setPointChanged = 2; // change back to setpoint decreased
      if (setTimer != 0) { //set the timer back to whatever it was before removing handle from base
        printNumber(RIGHT, setTimer);
        changeSegment(10, 3, 1); //enable S icon
      }
      else {
        for (byte i = 11; i < 17; i++) { //disable timer number segments
          ht.writeMem(i, 0b0000);
        }
        changeSegment(16, 3, 1); //enable OFF icon
        changeSegment(10, 3, 0); //disable S icon
      }
    }
    reedFlag = false;
  }

  if (blowerOn && reedStatus && (!timer || timerTemporary > 0) && !coolingAfterTimer) { //Turn the heater on/off to regulate temp
    //noInterrupts();
    heat();
    //interrupts();
  }
  else digitalWrite(HEATER, LOW);

  unsigned long millisecs = millis(); //to avoid wasting time getting millis a milion times
  if (!standby && !reedStatus && !selectedSection && millisecs - touchMillis > 6000 && millisecs - btnMillis > 6000 &&  millisecs - potMillis > 6000) { //Standby
    analogWrite(BACKLIGHT, LCDBRIGHTNESSDIM); //decrease backlight brightness to the default standby value
    standby = true;
    changeSegment(22, 3, 1); //turn moon/star icon on
  }
  else if (standby && (reedStatus || millisecs - touchMillis < 6000 || millisecs - btnMillis < 6000 || millisecs - potMillis < 6000)) { //Active
    analogWrite(BACKLIGHT, LCDBRIGHTNESS); //increase backlight brightness to the default value
    changeSegment(22, 3, 0); //turn moon/star icon off
    standby = false;
  }

  if (touched) {
    if (millisecs - lastReact >= 100) reactTouch();
    if (touchReleased) touched = false;
  }

  if (selectedSection != 0 && millisecs - lastBlink >= 300) { //Blink selection (settings)
    blinkSelection();
    if (selectedSection != 4 && touchReleased && millisecs - touchMillis >= SETTINGSEXITTIME) { //stop blinking if last time touched > SETTINGSEXITTIME (I should have used underscores for the defines, this one officially changed to setting sexy time in my head)
      stopBlinking();
    }
  }

  millisecs = millis();
  if ((!setPointReached || selectedSection == 4) && blowerOn && (reedStatus || readTemp(1) >= SHUTDOWNTEMP + otherSettings.calTemp) && (selectedSection == 0 || selectedSection == 4) && millisecs - lastTempPrint >= 200 && millisecs - potMillis >= 1000) { //Print real temp to LCD
    printNumber(MAIN, calibrateTemp(0));
    lastTempPrint = millis();
  }

  if (buttonFlag) handleButton(); //check which button was pressed (channel buttons)

  if (millis() - lastAnalogCheck >= 20) { //read pots median ignores noisy readings
    unsigned short analogHeat = analogRead(AHEAT);
    samples.add(analogHeat);
    analogHeat = samples.getMedian();
    if (abs(analogHeat - heaterVal) > 5) defineTemp();
    unsigned short analogBlow = analogRead(ABLOW);
    samples2.add(analogBlow);
    analogBlow = samples2.getMedian();
    if (abs(analogBlow - blowerVal) > 25) defineBlower();
    lastAnalogCheck = millis();
  }

  if (heating && reedStatus && (setBlow != lastSetBlow)) { //commit new pwm for blower
    analogWrite(BLOWER, setBlow);
    lastSetBlow = setBlow;
  }

  if (blowerOn && !heating && readTemp(1) < SHUTDOWNTEMP) { //turn blower off
    analogWrite(BLOWER, 0);//turn off blower
    digitalWrite(HEATER, LOW); //turn off heater, just to be triple safe
    printNumber(MAIN, setTemp); //display set temp on main section
    blowerOn = false;
    if (reedStatus) {
      ht.writeMem(23, 0); //turn of temp icon
      changeSegment(24, 0, 0); //turn off ºC
      changeSegment(24, 1, 0); //turn off ºF
      changeSegment(31, 0, 0); //turn off Set
      changeSegment(31, 1, 1); //turn on fan icon
      printNumber(LEFT, setBlow); //display set blower percentage
      setPointChanged = 2;
      if (setTimer != 0) {
        printNumber(RIGHT, setTimer);
        changeSegment(10, 3, 1); //enable S icon
        coolingAfterTimer = false;
      }
      else {
        for (byte i = 11; i < 17; i++) { //disable timer number segments
          ht.writeMem(i, 0b0000);
        }
        changeSegment(16, 3, 1); //enable OFF icon
        changeSegment(10, 3, 0); //disable S icon
      }
    }
  }

  if (heating && millisecs - lastTempIcon >= 1000) { //increase thermometer icon segments and display set blower/temperature on left screen section
    int current = ht.readMem(23); //read current segment state
    if (current != 0b1111) { //if they aren't all on
      current *= -1; //turn number negative to be able to shift 1s to the right instead of 0s (change bit sign)
      current >>= 1; // Shift to the right (for example 1000 turns 1100)
      current *= -1; // turn number positive again
      bitSet(current, 3); // turn on the 3rd bit (thermometer icon and first segment)
      ht.writeMem(23, current); //write
    }
    else ht.writeMem(23, 0b1000); //turn all segments off
    if (!selectedSection && !setPointReached) { //toggle between set blower value and set temperature while the station is trying to reach setPoint and no settings section is selected
      if (switchDisplayed) {
        changeSegment(24, 0, 0); //turn off ºC
        changeSegment(24, 1, 0); //turn off ºF
        changeSegment(31, 0, 1); //turn on Set
        changeSegment(31, 1, 1); //turn on fan icon
        printNumber(LEFT, setBlow); //print set blower
      }
      else {
        changeSegment(31, 0, 1); //turn on Set
        changeSegment(31, 1, 0); //turn off fan icon
        if (otherSettings.tempUnit) { //turn on ºC
          changeSegment(24, 1, 0); //turn off ºF
          changeSegment(24, 0, 1); //turn on ºC
        }
        else { //turn on ºF
          changeSegment(24, 0, 0); //turn off ºC
          changeSegment(24, 1, 1); //turn on ºF
        }
        printNumber(LEFT, setTemp); //print temperature
      }
      switchDisplayed = !switchDisplayed;
    }
    lastTempIcon = millisecs;
  }

  else if (!heating && currentTemp >= MINTEMP && millisecs - lastTempIcon >= 1000) { //decrease thermometer icon segments
    int current = ht.readMem(23);
    if (current != 0b1000) {
      current <<= 1;
      ht.writeMem(23, current);
    }
    else ht.writeMem(23, 0b1111);
    lastTempIcon = millisecs;
  }

  if (!selectedSection && !setPointReached && heating && millis() - potMillis >= 1000 && ((setPointChanged == 1 && readTemp(otherSettings.tempUnit) <= calibrateTemp(1)) || (setPointChanged == 2 && readTemp(otherSettings.tempUnit) >= calibrateTemp(1))))  {//beep and start counting when setPoint is reached
    setPointReached = true;
    printNumber(MAIN, setTemp); //display set temp on main section
    changeSegment(24, 0, 0); //turn off ºC
    changeSegment(24, 1, 0); //turn off ºF
    changeSegment(31, 0, 1); //turn on Set
    changeSegment(31, 1, 1); //turn on fan icon
    printNumber(LEFT, setBlow); //print blower value on left section
    if (otherSettings.buzzer) { //if sound is active buzz
      tone(PIEZO, 1000, 100);
    }
    setPointReachedTime = millis();
    timerTemporary = setTimer;
    ITimer.attachInterrupt(TIMER_FREQ_HZ, timerHandler);
    changeSegment(10, 3, 1); //enable S icon
    timerFlag = true;
    setPointChanged = false;
    newPotValue = false;
  }

  if (otherSettings.serialOutput && millis() - lastSerialOutput >= SERIALTIME) { //this can be used to overlay settings/temperature to OBS for example, just make the Serial.println you want inside this if statement make sure you use a real serial terminal for this like putty, arduino serial monitor won't work with the commands
    Serial.write(27);       // ESC command
    Serial.print("[1;31m"); // red foreground (text)
    Serial.write(27);       // ESC command
    Serial.print("[2J");    // clear screen
    Serial.write(27);       // ESC command
    Serial.print("[H");     // cursor to home
    Serial.print("Temp: ");
    if (heating || blowerOn) {
      if (!setPointReached) Serial.print(calibrateTemp(0));
      else Serial.print(setTemp);
    }
    else Serial.print("OFF");
    Serial.print("/");
    if (heating) {
      Serial.print(setTemp);
      if (otherSettings.tempUnit) Serial.println("ºC");
      else Serial.println("ºF");
    }
    else Serial.println("OFF");
    Serial.print("Blower: ");
    Serial.print(setBlow);
    Serial.println("%");


    //print whatever other variable you want like setTimer, timerTemporary, standby, otherSettings.selectedCh, and so on.
    //make sure you use a real serial terminal for this like putty, arduino serial monitor won't work with the commands

    lastSerialOutput = millis();
  }
}
