float readTemp(bool unit) {
  if (millis() - lastTempRead >= 200 || currentTemp == 0) {
    float temperature = thermocouple.readCelsius();
    lastTempRead = millis();
    if (!isnan(temperature)) {//max6675 returned a good value
      currentTemp = temperature - otherSettings.calTemp;
      //      int difference = setPoint - calibrateTemp(0);
      //      if (abs(difference) <= 15) myPID.SetTunings(CLOSE_KP, CLOSE_KI, CLOSE_KD); //temperature is close to setPoint so set some less aggressive PID values THIS DIDNT WORK AS I INTENDED SO I USE DIFFERENT PIDs DEPENDING ON THE SETPOINT NOW, SEE HEAT() FUNCTION
      //      else myPID.SetTunings(KP, KI, KD);
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
    if (currentTemp >= 650) { //in case for some reason the temperature gets higher than this value the heater is disabled, blower put at max and error is shown on screen (keep in mind that currentTemp is not yet calibrated here so when the station is at 550ºC the currentTemp value is actually 550*RANGE400 or 550*1.11= 610.5ºC)
      digitalWrite(HEATER, LOW);
      Serial.println("Failsafe");
      analogWrite(BLOWER, 100);
      for (int i = 0 ; i < 32 ; i++) { //turn all LCD segments off
        ht.writeMem(i, 0);
      }
      char text[] = "Temperature failsafe   ";
      if (otherSettings.tempUnit) { //turn on ºC
        changeSegment(24, 1, 0); //turn off ºF
        changeSegment(24, 0, 1); //turn on ºC
      }
      else { //turn on ºF
        changeSegment(24, 0, 0); //turn off ºC
        changeSegment(24, 1, 1); //turn on ºF
      }
      while (1) {
        printNumber(LEFT, calibrateTemp(0));
        printText(MAIN, text, 0);
      }
    }
  }
  if (unit) return currentTemp; //celsius
  else return currentTemp * 1.8 + 32; //fahrenheit
}

short handleTempUnit (unsigned short temp, bool unit) {
  if (unit)return temp;
  else {
    short converted = (temp * 1.8 + 32); //convert from ºC to ºF
    if (converted > 999) return 999; //display only has 3 digits :(
    else return converted;
  }
}

short convertToC(unsigned short temp) {
  if (otherSettings.tempUnit)return temp; //already in ºC
  else return int(((temp - 32) / 1.8)); //convert from ºF to ºC
}

void heat() {
  calibrateTemp(1);
  if (setPoint >= 400) myPID.SetTunings(HIGH_KP, HIGH_KI, HIGH_KD);
  else myPID.SetTunings(KP, KI, KD);
  input = readTemp(1);
  myPID.Compute();
  if ((millis() - windowStartTime) > WINDOWSIZE) windowStartTime += WINDOWSIZE; //time to shift the Relay Window
  if (output < (millis() - windowStartTime)) digitalWrite(HEATER, LOW);
  else digitalWrite(HEATER, HIGH);
}

int calibrateTemp(bool type) {
  if (type) { //calibrate setPoint
    setPoint = convertToC(setTemp);
    if (setPoint >= 200) {
      if (setPoint < 300) setPoint *= RANGE200;
      else if (setPoint >= 300 && setPoint < 400) setPoint *= RANGE300;
      else if (setPoint < 500) setPoint *= RANGE400;
    }
    setPoint += otherSettings.calTemp;
    return handleTempUnit(setPoint, otherSettings.tempUnit);
  }
  else { //calibrate temperature reading to display on LCD
    if (convertToC(setTemp) < 200) {
      if (otherSettings.tempUnit) return int(readTemp(1) - otherSettings.calTemp); //ºC
      else return handleTempUnit((readTemp(1) - otherSettings.calTemp), 0); //ºF
    }
    else if (convertToC(setTemp) < 300) {
      if (otherSettings.tempUnit) return int((readTemp(1) - otherSettings.calTemp) / RANGE200); //ºC
      else return handleTempUnit(((readTemp(1) - otherSettings.calTemp) / RANGE200), 0); //ºF
    }
    else if (convertToC(setTemp) < 400) {
      if (otherSettings.tempUnit) return int((readTemp(1) - otherSettings.calTemp) / RANGE300); //ºC
      else return handleTempUnit(((readTemp(1) - otherSettings.calTemp) / RANGE300), 0); //ºF
    }
    else if (convertToC(setTemp) < 500) {
      if (otherSettings.tempUnit) return int((readTemp(1) - otherSettings.calTemp) / RANGE400); //ºC
      else return handleTempUnit(((readTemp(1) - otherSettings.calTemp) / RANGE400), 0); //ºF
    }
    else if (convertToC(setTemp) >= 500) {
      if (otherSettings.tempUnit) return int(readTemp(1) - otherSettings.calTemp); //ºC
      else return handleTempUnit((readTemp(1) - otherSettings.calTemp), 0); //ºF
    }
  }
  return 0;
}
