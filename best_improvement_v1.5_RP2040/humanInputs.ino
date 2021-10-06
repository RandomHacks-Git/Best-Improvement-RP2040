byte readTouch() { //ask touch ic which button was touched
  Wire.beginTransmission(touchAddress);//(0x56); Begin transmission to the ic
  Wire.write(byte(keyValues)); //Ask the particular register for data
  Wire.endTransmission(); // Ends the transmission and transmits the data from the register
  Wire.requestFrom(touchAddress, 1);  // Request the transmitted byte from the register
  if (Wire.available()) return Wire.read(); // Reads and returns the data from the register
  else return 0; //adicionei
}

void reactTouch() { //take action according to touched key
  //Serial.println(touchedButton);
  byte stepAmount = 5;
  if ((touchedButton == UP || touchedButton == DOWN) && millis() - touchMillis > 1000) {
    if (!selectedSection) {
      otherSettings.serialOutput = !otherSettings.serialOutput;
      changeSegment(7, 1, otherSettings.serialOutput); //turn "RS232 On" segment on LCD on or off depending on serialOutput bool status
      EEPROM.put(20, otherSettings); //save to eeprom
      EEPROM.commit();
      if (otherSettings.buzzer) {
        tone(PIEZO, 4000, 70);
        delay(70);
        tone(PIEZO, 5000, 100);
        lastReact = millis();
        lastToneMillis = millis();
      }
      touched = false;
    }
    else { //increase stepAmount if any setting is selected and up or down buttons are long pressed
      if (otherSettings.tempUnit) stepAmount = 10; //fast stepAmount for ºC
      else stepAmount = 20; //fast stepAmount for ºF
    }
  }

  if (touchedButton == SET && millis() - touchMillis > 1000) {
    stopBlinking();
    otherSettings.buzzer = !otherSettings.buzzer;
    if (otherSettings.buzzer) {
      tone(PIEZO, 4000, 70);
      delay(70);
      tone(PIEZO, 5000, 100);
      lastToneMillis = millis();
    }
    else { //no sound, flash backlight instead?
      tone(PIEZO, 5000, 70);
      delay(70);
      tone(PIEZO, 4000, 100);
      lastToneMillis = millis();
    }
    EEPROM.put(20, otherSettings);
    EEPROM.commit();
    touched = false;
    touchedButton = 0;
    lastReact = millis();
    selectedSection = 0;
  }

  if (touchedButton == SET && touchReleased && selectedSection != 4 && millis() - lastReact >= 200) {
    if (selectedSection < 2 || (selectedSection < 3 && !reedStatus)) { // || (timer && selectedSection < 3 && !heating) initially I planned to manage seconds and minutes so there was another section to toggle between them // timer can only be changed if the handle is in the base
      selectedSection += 1;
      printNumber(LEFT, setBlow); //if in settings menu print the blower value imediately or else if currently heating "set temperature" might appear instead
      ht.writeMem(31, 0b0010); //enable fan icon disable set icon
      changeSegment(24, 0, 0); //disable ºC
      changeSegment(24, 1, 0); //disable ºF
      if (!heating) {
        printNumber(RIGHT, setTimer); //timer value
        changeSegment(10, 3, 1); //enable S icon
      }
    }
    else {
      selectedSection = 0;
      stopBlinking();
    }
    touched = false;
    touchedButton = 0;
    lastReact = millis();
  }

  else if (touchedButton == UP) { //UP BUTTON
    if (selectedSection == 1) { //temp
      if (setTemp + stepAmount < handleTempUnit(MAXTEMP, otherSettings.tempUnit)) {
        setTemp += stepAmount;
        if (otherSettings.selectedCh != 4 || converted || (!otherSettings.tempUnit && setTemp == 217)) { //if it is the first time I modify the temperature with the touch channel (otherSettings.selectedCh isn't 4 yet) or the temp unit changed or it is set to ºF and at the min temp (212ºF) I round up the value to the nearest multiple of 5
          setTemp = 5 * int(setTemp / 5);
          converted = false;
        }
      }
      else {
        setTemp = handleTempUnit(MAXTEMP, otherSettings.tempUnit);
        if (otherSettings.buzzer) {
          tone(PIEZO, 1000, 100);
          lastToneMillis = millis();
        }
      }
      printNumber(MAIN, setTemp);
      printChannel(4); //touch channel
      otherSettings.selectedCh = 4;
      lastReact = millis();
      setPointReached = false;
      if (heating) setPointChanged = 2;
    }
    else if (selectedSection == 2) { //blower
      if (setBlow + stepAmount / 5 < MAXBLOW) setBlow += stepAmount / 5;
      else {
        setBlow = MAXBLOW;
        if (otherSettings.buzzer) {
          tone(PIEZO, 1000, 100);
          lastToneMillis = millis();
        }
      }
      printNumber(LEFT, setBlow);
      printChannel(4); //touch channel
      otherSettings.selectedCh = 4;
      lastReact = millis();
    }
    else if (selectedSection == 3) { //timer
      if (setTimer + stepAmount / 5 <= 999) {
        setTimer += stepAmount / 5;
      }
      else {
        setTimer = 999;
        if (otherSettings.buzzer) {
          tone(PIEZO, 1000, 100);
          lastToneMillis = millis();
        }
      }
      timer = true;
      timerTemporary = setTimer;
      printNumber(RIGHT, setTimer);
      changeSegment(16, 3, 0); //disable off icon
      changeSegment(24, 3, 1);
      changeSegment(10, 3, 1); //enable S icon
      timeUnit = 0;
      lastReact = millis();
    }
    //    else if (selectedSection == 4) { //timer unit
    //      if (timeUnit) { //seconds
    //        changeSegment(10, 2, 0); ////disable M icon
    //        changeSegment(10, 3, 1); //enable S icon
    //        timeUnit = 0;
    //      }
    //      else { //minutes
    //        changeSegment(10, 3, 0); ////disable s icon
    //        changeSegment(10, 2, 1); //enable M icon
    //        timeUnit = 1;
    //      }
    //      touched = false;
    //      lastReact = millis();
    //    }
    else if (selectedSection == 4) { //temperature calibration
      if (otherSettings.calTemp < 50)otherSettings.calTemp += 1;
      else if (otherSettings.buzzer) {
        tone(PIEZO, 1000, 100);
        lastToneMillis = millis();
      }
      if (otherSettings.tempUnit) printNumber(LEFT, otherSettings.calTemp); //replace two lines with function? this code is repeated in lcdStuff tab - blinkSelection function, also below
      else printNumber(LEFT, otherSettings.calTemp * 1.8);
      lastReact = millis();
    }
  }
  else if (touchedButton == DOWN) { //DOWN BUTTON
    if (selectedSection == 1) {
      if (setTemp - stepAmount > handleTempUnit(MINTEMP, otherSettings.tempUnit)) {
        setTemp -= stepAmount;
        if (otherSettings.selectedCh != 4 || converted || (!otherSettings.tempUnit && setTemp == 994)) { //if it is the first time I modify the temperature with the touch channel (otherSettings.selectedCh isn't 4 yet) or the temp unit changed or it is set to ºF and at the max temp (999ºF) I round up the value to the nearest multiple of 5
          setTemp = 5 * round(setTemp / 5.0F + 0.4);
          converted = false;
        }
      }
      else {
        setTemp = handleTempUnit(MINTEMP, otherSettings.tempUnit);
        if (otherSettings.buzzer) {
          tone(PIEZO, 1000, 100);
          lastToneMillis = millis();
        }
      }
      printNumber(MAIN, setTemp);
      printChannel(4); //touch channel
      otherSettings.selectedCh = 4;
      lastReact = millis();
      setPointReached = false;
      if (heating) setPointChanged = 1;
    }
    else if (selectedSection == 2) {
      if (setBlow - stepAmount / 5 > MINBLOW) setBlow -= stepAmount / 5;
      else {
        setBlow = MINBLOW;
        if (otherSettings.buzzer) {
          tone(PIEZO, 1000, 100);
          lastToneMillis = millis();
        }
      }
      printNumber(LEFT, setBlow);
      printChannel(4); //touch channel
      otherSettings.selectedCh = 4;
      lastReact = millis();
    }
    else if (selectedSection == 3) { //timer
      if (setTimer - stepAmount / 5 > 0) {
        setTimer -= stepAmount / 5;
        printNumber(RIGHT, setTimer);
      }
      else {
        setTimer = 0;
        timerTemporary = 0;
        timer = false;
        printNumber(RIGHT, setTimer);
        changeSegment(16, 3, 1); //enable OFF icon
        if (otherSettings.buzzer) {
          tone(PIEZO, 1000, 100);
          lastToneMillis = millis();
        }
      }
      timerTemporary = setTimer;
      lastReact = millis();
    }
    //    else if (selectedSection == 4) { //timer unit
    //      if (timeUnit) { //seconds
    //        changeSegment(10, 2, 0); ////disable M icon
    //        changeSegment(10, 3, 1); //enable S icon
    //        timeUnit = 0;
    //      }
    //      else { //minutes
    //        changeSegment(10, 3, 0); ////disable S icon
    //        changeSegment(10, 2, 1); //enable M icon
    //        timeUnit = 1;
    //      }
    //      touched = false;
    //      lastReact = millis();
    //    }
    else if (selectedSection == 4) { //temperature calibration
      if (otherSettings.calTemp > -50) otherSettings.calTemp -= 1;
      else if (otherSettings.buzzer) {
        tone(PIEZO, 1000, 100);
        lastToneMillis = millis();
      }
      if (otherSettings.tempUnit) printNumber(LEFT, otherSettings.calTemp); //replace two lines with function? this code is repeated in lcdStuff tab - blinkSelection function
      else printNumber(LEFT, otherSettings.calTemp * 1.8);
      lastReact = millis();
    }
  }

  else if (touchedButton == CF && touchReleased) { // ºC/ºF BUTTON
    if (!otherSettings.tempUnit) { //change to ºC
      changeSegment(10, 1, 0);
      changeSegment(10, 0, 1);
      if (selectedSection == 4) {
        changeSegment(24, 1, 0); //turn off ºF
        changeSegment(24, 0, 1); //turn on ºC
      }
      setTemp = convertToC(setTemp); //convert to ºC
    }
    else { //change to ºF
      changeSegment(10, 0, 0);
      changeSegment(10, 1, 1);
      if (selectedSection == 4) {
        changeSegment(24, 0, 0); //turn off ºC
        changeSegment(24, 1, 1); //turn on ºF
      }
      setTemp = handleTempUnit(setTemp, 0); //convert to ºF
    }
    converted = true;
    if (!selectedSection) printNumber(MAIN, setTemp);
    otherSettings.tempUnit = !otherSettings.tempUnit;
    EEPROM.put(20, otherSettings);
    EEPROM.commit();
    lastReact = millis();
    touched = false;
    touchedButton = 0;
  }
  if (touchedButton == CF && !setTimer && (!selectedSection || selectedSection == 4) && millis() - touchMillis > 1000) {
    if (!selectedSection) {
      selectedSection = 4;
      changeSegment(31, 0, 1); //turn on Set icon
      changeSegment(31, 1, 0); //disable fan icon
      changeSegment(24, 2, 1); //turn on Cal. icon
      if (otherSettings.tempUnit) { //turn on ºC
        changeSegment(24, 1, 0); //turn off ºF
        changeSegment(24, 0, 1); //turn on ºC
      }
      else { //turn on ºF
        changeSegment(24, 0, 0); //turn off ºC
        changeSegment(24, 1, 1); //turn on ºF
      }
    }
    else {
      stopBlinking();
      changeSegment(31, 0, 0); //turn off Set icon
      changeSegment(31, 1, 1); //turn on fan icon
      changeSegment(24, 2, 0); //turn off Cal. icon
      changeSegment(24, 0, 0); //turn off ºC
      changeSegment(24, 1, 0); //turn off ºF
      EEPROM.put(20, otherSettings);
      EEPROM.commit();
    }
    if (otherSettings.buzzer) {
      tone(PIEZO, 1000, 100);
      lastToneMillis = millis();
    }
    touchedButton = 0;
    touched = false;
    lastReact = millis();
  }
}

void handleButton() {
  if (btn1) {
    if (digitalRead(BTN1)) { //short press
      if (selectedSection && selectedSection != 4) stopBlinking();
      if (otherSettings.selectedCh == 1) {
        defineBlower();
        defineTemp();
      }
      else {
        otherSettings.selectedCh = 1;
        EEPROM.put(20, otherSettings);
        EEPROM.commit();
        printChannel(1);
        printNumber(MAIN, handleTempUnit(ch1Settings.temp, otherSettings.tempUnit));
        printNumber(LEFT, ch1Settings.blow);
        if (convertToC(setTemp) > ch1Settings.temp && heating) setPointChanged = 1;
        else setPointChanged = 2;
        setTemp = handleTempUnit(ch1Settings.temp, otherSettings.tempUnit);
        setBlow = ch1Settings.blow;
      }
      setPointReached = false;
      buttonFlag = false;
      btn1 = 0;
    }
    else if (millis() - btnMillis >= 1000) { //long press
      ch1Settings.temp = convertToC(setTemp);
      ch1Settings.blow = setBlow;
      EEPROM.put(4, ch1Settings);
      EEPROM.commit();
      if (otherSettings.buzzer) {
        tone(PIEZO, 4000, 50);
        delay(50);
        tone(PIEZO, 5000, 100);
        lastToneMillis = millis();
      }
      buttonFlag = false;
      btn1 = 0;
      otherSettings.selectedCh = 1;
      EEPROM.put(20, otherSettings);
      EEPROM.commit();
      printChannel(otherSettings.selectedCh);
    }
  }
  else if (btn2) {
    if (digitalRead(BTN2)) { //short press
      if (selectedSection && selectedSection != 4) stopBlinking();
      if (otherSettings.selectedCh == 2) {
        defineBlower();
        defineTemp();
      }
      else {
        otherSettings.selectedCh = 2;
        EEPROM.put(20, otherSettings);
        EEPROM.commit();
        printChannel(2);
        printNumber(MAIN, handleTempUnit(ch2Settings.temp, otherSettings.tempUnit));
        printNumber(LEFT, ch2Settings.blow);
        if (convertToC(setTemp) > ch2Settings.temp && heating) setPointChanged = 1;
        else setPointChanged = 2;
        setTemp = handleTempUnit(ch2Settings.temp, otherSettings.tempUnit);
        setBlow = ch2Settings.blow;
      }
      setPointReached = false;
      buttonFlag = false;
      btn2 = 0;
    }
    else if (millis() - btnMillis >= 1000) { //long press
      ch2Settings.temp = convertToC(setTemp);
      ch2Settings.blow = setBlow;
      EEPROM.put(8, ch2Settings);
      EEPROM.commit();
      if (otherSettings.buzzer) {
        tone(PIEZO, 4000, 50);
        delay(50);
        tone(PIEZO, 5000, 100);
        lastToneMillis = millis();
      }
      buttonFlag = false;
      btn2 = 0;
      otherSettings.selectedCh = 2;
      EEPROM.put(20, otherSettings);
      EEPROM.commit();
      printChannel(otherSettings.selectedCh);
    }
  }
  else if (btn3) {
    if (digitalRead(BTN3)) { //short press
      if (selectedSection && selectedSection != 4) stopBlinking();
      if (otherSettings.selectedCh == 3) {
        defineBlower();
        defineTemp();
      }
      else {
        otherSettings.selectedCh = 3;
        EEPROM.put(20, otherSettings);
        EEPROM.commit();
        printChannel(3);
        printNumber(MAIN, handleTempUnit(ch3Settings.temp, otherSettings.tempUnit));
        printNumber(LEFT, ch3Settings.blow);
        if (convertToC(setTemp) > ch3Settings.temp && heating) setPointChanged = 1;
        else setPointChanged = 2;
        setTemp = handleTempUnit(ch3Settings.temp, otherSettings.tempUnit);
        setBlow = ch3Settings.blow;
      }
      setPointReached = false;
      buttonFlag = false;
      btn3 = 0;
    }
    else if (millis() - btnMillis >= 1000) { //long press
      ch3Settings.temp = convertToC(setTemp);
      ch3Settings.blow = setBlow;
      EEPROM.put(12, ch3Settings);
      EEPROM.commit();
      if (otherSettings.buzzer) {
        tone(PIEZO, 4000, 50);
        delay(50);
        tone(PIEZO, 5000, 100);
        lastToneMillis = millis();
      }
      buttonFlag = false;
      btn3 = 0;
      otherSettings.selectedCh = 3;
      EEPROM.put(20, otherSettings);
      EEPROM.commit();
      printChannel(otherSettings.selectedCh);
    }
  }
}

void defineBlower() {
  blowerVal = analogRead(ABLOW);
  //Serial.println(blowerVal);
  byte tempMap = map(blowerVal, 7, 4083, MAXBLOW, MINBLOW);
  int change = setBlow - tempMap;
  if (millis() - potMillis <= 2000 || abs(change) >= 2) { //to avoid analogRead noise, remains responsive while turning knob but only accepts any change equal or greater than 2 if last pot change was more than 2 seconds ago
    if (otherSettings.selectedCh != 0) {
      otherSettings.selectedCh = 0;
      EEPROM.put(20, otherSettings);
      EEPROM.commit();
      printChannel(0);
    }
    if (tempMap < MINBLOW) setBlow = MINBLOW;
    else if (tempMap > MAXBLOW) setBlow = MAXBLOW;
    else setBlow = tempMap;
    printNumber(LEFT, setBlow);
    potMillis = millis();
  }
}

void defineTemp() {
  heaterVal = analogRead(AHEAT);
  //Serial.println(heaterVal);
  unsigned short tempMap = map(heaterVal, 25, 4085, handleTempUnit(MAXTEMP, otherSettings.tempUnit), handleTempUnit(MINTEMP, otherSettings.tempUnit));
  int change = setTemp - tempMap;
  if (millis() - potMillis <= 2000 || abs(change) >= 2) { //to avoid analogRead noise, remains responsive while turning knob but only accepts any change equal or greater than 10 if last pot change was more than 2 seconds ago
    if (otherSettings.selectedCh != 0) {
      otherSettings.selectedCh = 0;
      EEPROM.put(20, otherSettings);
      EEPROM.commit();
      printChannel(0);
    }
    if (heating && setTemp > tempMap) setPointChanged = 1;
    else setPointChanged = 2;
    if (tempMap < handleTempUnit(MINTEMP, otherSettings.tempUnit)) setTemp = handleTempUnit(MINTEMP, otherSettings.tempUnit);
    else if (tempMap > handleTempUnit(MAXTEMP, otherSettings.tempUnit)) setTemp = handleTempUnit(MAXTEMP, otherSettings.tempUnit);
    else setTemp = tempMap;
    printNumber(MAIN, setTemp);
    if (heating) newPotValue = true;
    potMillis = millis();
  }
}
