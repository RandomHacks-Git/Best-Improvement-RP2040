void clearDigit(byte section, byte digitNumber) {
  byte address;
  switch (section) {
    case MAIN:
      if (digitNumber == 1) address = 17;
      if (digitNumber == 2) address = 19;
      if (digitNumber == 3) address = 21;
      break;
    case LEFT:
      if (digitNumber == 1) address = 25;
      if (digitNumber == 2) address = 27;
      if (digitNumber == 3) address = 29;
      break;
    case RIGHT:
      if (digitNumber == 1) address = 11;
      if (digitNumber == 2) address = 13;
      if (digitNumber == 3) address = 15;
      break;
    case SMALL:
      if (digitNumber == 1) address = 8;
      break;
  }
  ht.writeMem(address, 0b0000);
  ht.writeMem(address + 1, 0b0000);
}

byte printNumber(byte section, short number) {
  if ((number > 999 && otherSettings.tempUnit) || (number < 0 && selectedSection != 4)) return 0;
  else if (number > 999 && (section == MAIN || section == LEFT)) number = 999;
  if (section == SMALL && number > 9) return 0;
  byte hundreds, tens, units;
  if (number < 0)number *= -1; //turn number positive, negative numbers are only used for temperature calibration

  if (section != SMALL && number > 99) {
    hundreds = ((number / 100) % 10);
    tens = ((number / 10) % 10);
    units = number % 10;
    if (section == MAIN) {
      digitPrint(17, units);
      digitPrint(19, tens);
      digitPrint(21, hundreds);
      if (standby) changeSegment(22, 3, 1); //turn the moon/star back on if in standby
    }
    else if (section == LEFT) {
      digitPrint(25, units);
      digitPrint(27, tens);
      digitPrint(29, hundreds);
    }
    else if (section == RIGHT) {
      digitPrint(11, units);
      digitPrint(13, tens);
      digitPrint(15, hundreds);
      if (!timer) changeSegment(16, 3, 1); //turn the "off" icon back on if timer is off
    }
  }
  else if (section != SMALL && number > 9) {
    tens = number / 10;
    units = number % 10;
    clearDigit(section, 3);
    if (section == MAIN) {
      digitPrint(17, units);
      digitPrint(19, tens);
      if (standby) changeSegment(22, 3, 1); //turn the moon/star back on if in standby
    }
    else if (section == LEFT) {
      digitPrint(25, units);
      digitPrint(27, tens);
      if (selectedSection == 4 && handleTempUnit(otherSettings.calTemp[calibrationArrayIndex()], otherSettings.tempUnit) < 0) changeSegment(30, 1, 1); //enable negative sign
    }
    else if (section == RIGHT) {
      digitPrint(11, units);
      digitPrint(13, tens);
      if (!timer) changeSegment(16, 3, 1); //turn the "off" icon back on if timer is off
    }
  }
  else {
    units = number;
    if (section != SMALL) {
      clearDigit(section, 3);
      clearDigit(section, 2);
    }
    if (section == MAIN) {
      digitPrint(17, units);
      if (standby) changeSegment(22, 3, 1); //turn the moon/star back on if in standby
    }
    else if (section == LEFT) {
      digitPrint(25, units);
      if (selectedSection == 4 && otherSettings.calTemp[calibrationArrayIndex()] < 0) changeSegment(28, 1, 1); //enable negative sign
    }
    else if (section == RIGHT) {
      digitPrint(11, units);
      if (!timer) changeSegment(16, 3, 1); //turn the "off" icon back on if timer is off
    }
    else if (section == SMALL) {
      digitPrint(8, units);
    }
  }
  return 1;
}

//print digit

void digitPrint(byte address, byte number) {
  switch (number) {
    case 0:
      ht.writeMem(address, 0b1111);
      ht.writeMem(address + 1, 0b0101);
      break;
    case 1:
      ht.writeMem(address, 0b0110);
      ht.writeMem(address + 1, 0b0000);
      break;
    case 2:
      ht.writeMem(address, 0b1011);
      ht.writeMem(address + 1, 0b0110);
      break;
    case 3:
      ht.writeMem(address, 0b1111);
      ht.writeMem(address + 1, 0b0010);
      break;
    case 4:
      ht.writeMem(address, 0b0110);
      ht.writeMem(address + 1, 0b0011);
      break;
    case 5:
      ht.writeMem(address, 0b1101);
      ht.writeMem(address + 1, 0b0011);
      break;
    case 6:
      ht.writeMem(address, 0b1101);
      ht.writeMem(address + 1, 0b0111);
      break;
    case 7:
      ht.writeMem(address, 0b0111);
      ht.writeMem(address + 1, 0b0000);
      break;
    case 8:
      ht.writeMem(address, 0b1111);
      ht.writeMem(address + 1, 0b0111);
      break;
    case 9:
      ht.writeMem(address, 0b1111);
      ht.writeMem(address + 1, 0b0011);
      break;
  }
}

void printText(byte address, char text[], bool loopText) { //this function is blocking!! modify and use timer interrupt or handle inside loop if needed, since I'm only using it to show errors there is no need
  digitalWrite(HEATER, LOW); //in case you did not read/understand the above comment :)
  text = strlwr(text);
  uint8_t textLength = strlen(text);
  //  char textBuffer[textLength + 4];
  //  char finalSpaces[4] = "   ";
  //  strcat(textBuffer, finalSpaces);

  do {
    for (int i = 0; i < textLength; i++) {
      printLetter(address, text[i]);
      if (i > 0) printLetter(address + 2, text[i - 1]); //shift letter to next 7 segments on the left like ■■a -> ■ab if text[i] = b
      if (i > 1) printLetter(address + 4, text[i - 2]); //shift letter to next 7 segments on the left like ■ab -> abc if text[i] = c
      //Serial.println(text[i]);
      delay(200);
      rp2040.wdt_reset(); //reset watchdog timer
    }
  } while (loopText);
}

void printLetter(byte address, char letter) {
  switch (letter) {
    case '#':
      ht.writeMem(address, 0b0000);
      ht.writeMem(address + 1, 0b0000);
      break;
    case '0':
      ht.writeMem(address, 0b1111);
      ht.writeMem(address + 1, 0b0101);
      break;
    case '1':
      ht.writeMem(address, 0b0110);
      ht.writeMem(address + 1, 0b0000);
      break;
    case '2':
      ht.writeMem(address, 0b1011);
      ht.writeMem(address + 1, 0b0110);
      break;
    case '3':
      ht.writeMem(address, 0b1111);
      ht.writeMem(address + 1, 0b0010);
      break;
    case '4':
      ht.writeMem(address, 0b0110);
      ht.writeMem(address + 1, 0b0011);
      break;
    case '5':
      ht.writeMem(address, 0b1101);
      ht.writeMem(address + 1, 0b0011);
      break;
    case '6':
      ht.writeMem(address, 0b1101);
      ht.writeMem(address + 1, 0b0111);
      break;
    case '7':
      ht.writeMem(address, 0b0111);
      ht.writeMem(address + 1, 0b0000);
      break;
    case '8':
      ht.writeMem(address, 0b1111);
      ht.writeMem(address + 1, 0b0111);
      break;
    case '9':
      ht.writeMem(address, 0b1111);
      ht.writeMem(address + 1, 0b0011);
      break;
    case ' ':
      ht.writeMem(address, 0b0000);
      ht.writeMem(address + 1, 0b0000);
      break;
    case 'a':
      ht.writeMem(address, 0b0111);
      ht.writeMem(address + 1, 0b0111);
      break;
    case 'b':
      ht.writeMem(address, 0b1100);
      ht.writeMem(address + 1, 0b0111);
      break;
    case 'c':
      ht.writeMem(address, 0b1001);
      ht.writeMem(address + 1, 0b0101);
      break;
    case 'd':
      ht.writeMem(address, 0b1110);
      ht.writeMem(address + 1, 0b0110);
      break;
    case 'e':
      ht.writeMem(address, 0b1001);
      ht.writeMem(address + 1, 0b0111);
      break;
    case 'f':
      ht.writeMem(address, 0b0001);
      ht.writeMem(address + 1, 0b0111);
      break;
    case 'g':
      ht.writeMem(address, 0b1111);
      ht.writeMem(address + 1, 0b0011);
      break;
    case 'h':
      ht.writeMem(address, 0b0100);
      ht.writeMem(address + 1, 0b0111);
      break;
    case 'i':
      ht.writeMem(address, 0b0110);
      ht.writeMem(address + 1, 0b0000);
      break;
    case 'j':
      ht.writeMem(address, 0b1110);
      ht.writeMem(address + 1, 0b0000);
      break;
    case 'k':
      ht.writeMem(address, 0b0110);
      ht.writeMem(address + 1, 0b0111);
      break;
    case 'l':
      ht.writeMem(address, 0b1000);
      ht.writeMem(address + 1, 0b0101);
      break;
    case 'm':
      ht.writeMem(address, 0b0111);
      ht.writeMem(address + 1, 0b0101);
      break;
    case 'n':
      ht.writeMem(address, 0b0100);
      ht.writeMem(address + 1, 0b0110);
      break;
    case 'o':
      ht.writeMem(address, 0b1100);
      ht.writeMem(address + 1, 0b0110);
      break;
    case 'p':
      ht.writeMem(address, 0b0011);
      ht.writeMem(address + 1, 0b0111);
      break;
    case 'q':
      ht.writeMem(address, 0b0111);
      ht.writeMem(address + 1, 0b0011);
      break;
    case 'r':
      ht.writeMem(address, 0b0000);
      ht.writeMem(address + 1, 0b0110);
      break;
    case 's':
      ht.writeMem(address, 0b1101);
      ht.writeMem(address + 1, 0b0011);
      break;
    case 't':
      ht.writeMem(address, 0b1000);
      ht.writeMem(address + 1, 0b0111);
      break;
    case 'u':
      ht.writeMem(address, 0b1100);
      ht.writeMem(address + 1, 0b0100);
      break;
    case 'v':
      ht.writeMem(address, 0b1110);
      ht.writeMem(address + 1, 0b0101);
      break;
    case 'w':
      ht.writeMem(address, 0b1110);
      ht.writeMem(address + 1, 0b0111);
      break;
    case 'x':
      ht.writeMem(address, 0b0110);
      ht.writeMem(address + 1, 0b0111);
      break;
    case 'y':
      ht.writeMem(address, 0b1110);
      ht.writeMem(address + 1, 0b0011);
      break;
    case 'z':
      ht.writeMem(address, 0b1011);
      ht.writeMem(address + 1, 0b0110);
      break;
  }
}

void printChannel (byte channel) {
  switch (channel) {
    case 0: //Print P (for Potentiometer)
      ht.writeMem(8, 0b0011);
      ht.writeMem(9, 0b1111);
      break;
    case 1: //Print 1
      ht.writeMem(8, 0b0110);
      ht.writeMem(9, 0b1000);
      break;
    case 2: //Print 2
      ht.writeMem(8, 0b1011);
      ht.writeMem(9, 0b1110);
      break;
    case 3: //Print 3
      ht.writeMem(8, 0b1111);
      ht.writeMem(9, 0b1010);
      break;
    case 4:
      ht.writeMem(8, 0b1000);
      ht.writeMem(9, 0b1111);
  }
}

void blinkSelection() {
  switch (selectedSection) {
    case 1:
      if (!sectionOff) { //turn off section
        for (byte i = 17; i < 23; i++) {
          ht.writeMem(i, 0b0000);
        }
        sectionOff = true;
      }
      else { //turn on section
        if (!otherSettings.cool) printNumber(MAIN, setTemp);
        else printOff(0);
        sectionOff = false;
      }
      lastBlink = millis();
      break;

    case 2:
      if (!otherSettings.cool) printNumber(MAIN, setTemp);
      else printOff(0);
      if (!sectionOff) {
        for (byte i = 25; i < 31; i++) {
          ht.writeMem(i, 0b0000);
        }
        sectionOff = true;
      }
      else {
        printNumber(LEFT, setBlow);
        sectionOff = false;
      }
      lastBlink = millis();
      break;

    case 3:
      printNumber(LEFT, setBlow);
      if (!sectionOff) {
        for (byte i = 11; i < 17; i++) {
          ht.writeMem(i, 0b0000);
        }
        if (!timer) changeSegment(16, 3, 1); //turn on "off" icon
        sectionOff = true;
      }
      else {
        printNumber(RIGHT, setTimer);
        if (!timer) changeSegment(16, 3, 1); //turn on "off" icon
        sectionOff = false;
      }
      lastBlink = millis();
      break;

    case 4:
      if (!sectionOff) {
        for (byte i = 25; i < 31; i++) {
          ht.writeMem(i, 0b0000);
        }
        sectionOff = true;
      }
      else {
        if (otherSettings.tempUnit) printNumber(LEFT, otherSettings.calTemp[calibrationArrayIndex()]);
        else printNumber(LEFT, otherSettings.calTemp[calibrationArrayIndex()] * 1.8);
        //printNumber(LEFT, handleTempUnit(otherSettings.calTemp, otherSettings.tempUnit));
        //handle CF icon function thingymabob
        sectionOff = false;
      }
      lastBlink = millis();
      break;
  }
}


void stopBlinking() {
  if (!otherSettings.cool) printNumber(MAIN, setTemp);
  else printOff(0);
  printNumber(LEFT, setBlow);
  if (!reedStatus || coolingAfterTimer || coolAirFlag) {
    if (timer) {
      printNumber(RIGHT, setTimer);
      if (timeUnit) changeSegment(10, 3, 1); //enable S icon
      //else changeSegment(10, 2, 1); //enable M icon
    }
    else {
      ht.writeMem(11, 0); //disable timer section
      ht.writeMem(12, 0);
      changeSegment(10, 3, 0); //disable S icon
    }
  }
  if (selectedSection == 4) {
    changeSegment(24, 0, 0); //turn off ºC
    changeSegment(24, 1, 0); //turn off ºF
    eepromFlag = true;
  }
  selectedSection = 0;
  if (otherSettings.selectedCh == 4) {
    touchSettings.temp = convertToC(setTemp);
    touchSettings.blow = setBlow;
    eepromFlag = true;;
  }
}

void changeSegment(byte address, byte bit, bool value) { //changes a single segment
  byte nowSet = ht.readMem(address); //check what is currently being displayed on address so we only change what we want
  nowSet = bitWrite(nowSet, bit, value); //change only the segment we want (bit)
  ht.writeMem(address, nowSet);
}

void printUnit() {
  if (otherSettings.tempUnit)changeSegment(10, 0, 1); //ºC
  else changeSegment(10, 1, 1); //ºF
}

void printOff(bool lcdArea) { //prints OFF on main display if lcdArea is 0 or on left display if lcdArea is 1 and disables temp unit icon, used for cool air only function
  if (!lcdArea) { //main
    digitPrint(21, 0);
    printLetter(19, 'f');
    printLetter(17, 'f');
    //  changeSegment(10, 0, 0); //disable ºC
    //  changeSegment(10, 1, 0); //disable ºF
  }
  else { //small left
    digitPrint(29, 0);
    printLetter(27, 'f');
    printLetter(25, 'f');
    //  changeSegment(24, 0, 0); //disable ºC
    //  changeSegment(24, 1, 0); //disable ºF
  }
}

void toggleLeftDisplay() { //toggle between set temp and fan speed on the left display
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
    if (!otherSettings.cool) printNumber(LEFT, setTemp); //print temperature
    else printOff(1);
  }
  switchDisplayed = !switchDisplayed;
}
