float readTemp(bool unit) {
  if (millis() - lastTempRead >= 200 || currentTemp == 0) {
    float temperature = thermocouple.readCelsius();
    lastTempRead = millis();
    if (!isnan(temperature)) { //max6675 returned a good value
      currentTemp = temperature - otherSettings.calTemp[calibrationArrayIndex()];
      if (temperature == lastTemp) {
        sameReading++;
        if (sameReading >= 25)tempRunaway(); //if the last 25 consecutive readings (5 seconds) are the same something must be wrong with the thermocouple
      }
      else sameReading = 0;
      lastTemp = temperature;
    }
    else { //thermocouple not returning a correct value, therefore disabling heater and showing error on screen
      Serial.println("Thermocouple error!");
      digitalWrite(HEATER, LOW);
      analogWrite(BLOWER, 100);
      for (int i = 0 ; i < 32 ; i++) { //turn all LCD segments off
        ht.writeMem(i, 0);
      }
      char text[] = "Thermocouple error   ";
      printText(MAIN, text, 1);
    }
    if (heating && selectedSection != 4 && (setPoint > lastSetPoint && lastSetPoint != 0 && setPointChanged != 1 && (currentTemp >= convertToC(setTemp) + 100)) || (setPointReached && currentTemp <= (convertToC(setTemp) - 100))) tempRunaway(); //in case for some reason the temperature gets 100ºC higher than the set value or drops 50ºC or more after reaching the setPoint, the heater is disabled, blower put at max and error is displayed on screen
  }
  if (unit) return currentTemp; //celsius
  else return currentTemp * 1.8 + 32; //fahrenheit
}

short handleTempUnit (unsigned short temp, bool unit) {
  if (unit || temp == 0)return temp;
  else {
    short converted = (temp * 1.8 + 32); //convert from ºC to ºF
    if (converted > 999) return 999; //display only has 3 digits :(
    else return converted;
  }
}

short convertToC(unsigned short temp) {
  if (otherSettings.tempUnit || temp == 0)return temp; //already in ºC or 0ºC
  else return int(((temp - 32) / 1.8)); //convert from ºF to ºC
}

void heat() {
  if (tempLowered && timerTemporary > 2) tempLowered = false; //when the temperature decreases waits for 2 seconds after reaching the setpoint before turning the heater back on
  calibrateTemp(1);
  //if (setPoint >= 400) myPID.SetTunings(HIGH_KP, HIGH_KI, HIGH_KD);
  //else
  myPID.SetTunings(KP, KI, KD);
  input = readTemp(1);
  if (!tempLowered) {
    myPID.Compute();
    if ((millis() - windowStartTime) > WINDOWSIZE) windowStartTime += WINDOWSIZE; //time to shift the Relay Window
    if (output < (millis() - windowStartTime)) {
      digitalWrite(HEATER, LOW);
      isHigh = false;
    }
    else {
      if (!isHigh) {
        lastHigh = millis();
        isHigh = true;
      }
      if (input < MINTEMP - 10) analogWrite(HEATER, 50); //When the heating element is cold the resistance is lower and therefore the current will be higher, to increase the longevity of the heating element I am reducing the PWM duty cycle instead of turning the heater fully on
      else digitalWrite(HEATER, HIGH);
    }
  }
  else {
    digitalWrite(HEATER, LOW);
    isHigh = false;
  }
}

int calibrateTemp(bool type) {
  if (type) { //calibrate setPoint
    setPoint = convertToC(setTemp);
    setPoint += otherSettings.calTemp[calibrationArrayIndex()]; //apply calibration to the currently set temperature to get the adjusted setPoint
    //lastSetPoint = setPoint;
    return handleTempUnit(setPoint, otherSettings.tempUnit);
  }
  else { //calibrate temperature reading to display on LCD
    if (otherSettings.cool) {
      if (otherSettings.tempUnit) return int(readTemp(1)); //ºC
      else return handleTempUnit(readTemp(1), 0); //ºF
    }
    else {
      if (otherSettings.tempUnit) return int((readTemp(1) - otherSettings.calTemp[calibrationArrayIndex()])); //ºC
      else return handleTempUnit(((readTemp(1) - otherSettings.calTemp[calibrationArrayIndex()])), 0); //ºF
    }
  }
  return 0;
}

void startHeating() {
  myPID.SetMode(AUTOMATIC); //turn on PID
  windowStartTime = millis();
  ht.writeMem(23, 0b1000); //thermometer icon on
  heating = true;
  heaterTempChangeTime = millis();
}

void stopHeating() {
  myPID.SetMode(MANUAL); //turn off PID
  output = 0;
  setPoint = lastSetPoint = 0;
  digitalWrite(HEATER, LOW);
  isHigh = false;
  heating = false;
  setPointReached = false;
  setPointChanged = 2;
  heaterTempChangeTime = millis();
  coolingAfterTimer = false;
  if (otherSettings.cool && selectedSection == 4) {
    stopBlinking();
  }
}

byte calibrationArrayIndex() {
  byte calIndex; //calibration array index
  if (convertToC(setTemp) < 100) calIndex = 0; //only happens if MINTEMP is set to anything under 100ºC
  else calIndex = convertToC(setTemp) / 100 - 1; //derive otherSettings.CalTemp[] index from currently set temperature
  return calIndex;
}

void blowerBoost(bool boost) {
  if (otherSettings.boostBlower && heating) {
    if (boost) {
      byte blowBoost = setBlow + BLOWERBOOST;
      if (blowBoost > MAXBLOW) blowBoost = MAXBLOW;
      analogWrite(BLOWER, map(blowBoost, MINBLOW, MAXBLOW, MINDUTYCYCLE, MAXBLOW)); //boost blower speed while cooling
      boostingBlower = true;
    }
    else {
      analogWrite(BLOWER, map(setBlow, MINBLOW, MAXBLOW, MINDUTYCYCLE, MAXBLOW)); //lower blower speed back to original set speed
      boostingBlower = false;
    }
  }
}

void tempRunaway() {
  digitalWrite(HEATER, LOW);
  Serial.println("Failsafe");
  analogWrite(BLOWER, 100);
  for (int i = 0 ; i < 32 ; i++) { //turn all LCD segments off
    ht.writeMem(i, 0);
  }
  char text[] = "Temp failsafe   ";
  if (otherSettings.tempUnit) { //turn on ºC
    changeSegment(24, 1, 0); //turn off ºF
    changeSegment(24, 0, 1); //turn on ºC
  }
  else { //turn on ºF
    changeSegment(24, 0, 0); //turn off ºC
    changeSegment(24, 1, 1); //turn on ºF
  }
  for (static byte instance = 0; instance <= 2; instance++) { //scroll error across screen 3 times
    tone(PIEZO, 3500, 500);
    printNumber(LEFT, calibrateTemp(0));
    printText(MAIN, text, 0);
  }
  rp2040.reboot(); //reboot station after the error was displayed 3 times
}
