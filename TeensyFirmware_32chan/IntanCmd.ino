
// ------------------------------------
//   SPI READ REGISTER COMMAND
// ------------------------------------
/* JE: This used to return an unsigned int with readAddress | Register contents.
  Result is returned in lower 8 bytes. First 8 bytes (MSBs) are all 0.
  unsigned int readRegister(byte thisRegister, int bytesToRead ) {

   JE: change to digitalWriteFast() for faster toggling of CS...for teh future
*/

uint16_t readRegister(byte thisRegister) {

  // byte inByte = 0;           // incoming byte from the SPI
  //byte defaultresult = -1;           // result to return, default is -1, will be set to other meaningful value when SPI transfer completes
  //unsigned int result;   // result to return

  byte xtrans = 0b0;
  uint16_t result;
  byte response1;
  byte response2;
  

  //Intan expects the adress after 11
  byte readAddress = READ_REG_CMD | thisRegister;

  //SERIALNAME->println(readAddress, BIN);
  // take the chip select low to select the device:
  digitalWrite(CSpin, LOW);
  
  //send the first byte *(adress)
  response1 = SPI.transfer(readAddress);
  //read the second byte (data). The intan chip does this with all 0's
  response2 = SPI.transfer(xtrans);
  
  // take the chip select high to de-select:
  digitalWrite(CSpin, HIGH);
  
  result = (response1 << 8) | response2;
  return (result);
 
}




// ---------------------------------------
//   CONVERT CHANNEL SPI COMMAND
// ----------------------------------------
/* JE: Intan RHD2132 specifies that 2 bytes will be returned for each convert command, no matter what.
  So let's update function prototype to account for this, no variable bytes to read
   unsigned int convertChannel(byte thisChannel, int bytesToRead ) {
*/

uint16_t convertChannel(byte thisChannel) {

  uint16_t result;
  byte response1;
  byte response2;

  //Intan expects the adress after 00
  byte convertAddress = CONVERT_CHAN_CMD | thisChannel;
  
  // take the chip select low to select the device:
  digitalWrite(CSpin, LOW);
  
  //send the first byte *(adress)
  response1 = SPI.transfer(convertAddress);
    //transfer second byte. Last bit could be a 1 or 0 depending on desired DSP settings.
  response2 = SPI.transfer(0); //JE 03 Nov 2016: Note that a 1 (high) can be sent as LSB for fast recovery from large transient.

  // take the chip select high to de-select:
  digitalWrite(CSpin, HIGH);


  // JE: Return result with first SPI byte shifted 8 bits, then OR'd with response2 byte
  result = (response1 << 8) | response2;

  return (result);
}





// --------------------------------------
//      WRITE REGISTER COMMAND
// --------------------------------------
uint16_t writeRegister(byte thisRegister, byte thisValue) {

uint16_t resultWrite;

  byte writeAddress = WRITE_REG_CMD | thisRegister;


  // take the chip select low to select the device:
  digitalWrite(CSpin, LOW);

  byte check1 = SPI.transfer(writeAddress); //Send register location
  byte check2 = SPI.transfer(thisValue);  //Send value to record into register

  // take the chip select high to de-select:
  digitalWrite(CSpin, HIGH);

  resultWrite = (check1 << 8) | check2;
  
}





// --------------------------------------
//      CALIBRATE ADC COMMAND
// --------------------------------------

/* From Intan datasheet: ADC self-calibration should be performed after chip power-up and register configuration....
    Self-calibration takes many clock cycles to execute....
    Nine dummy commands must be sent after a CALIBRATE command to generate the necessary clock cycles
    Calibrate command should be sent only ONCE to initiate a calibration sequence...nine commands following CALIBRATE command are not executed,
    ; they are ignored until CALIBRATE is complete

*/
void calibrateADC() {

 //  give the intan at least 100 micros to configure before ADC calibration--see page 19 of RHD2000 datasheet.
  // 1 ms is overkill, but barely any wait at all.
  delay(1);
    
  // take the chip select low to select the device:
  digitalWrite(CSpin, LOW);

  SPI.transfer(CALIBRATE_REG_CMD_MSBs); //Send first byte (MSBs)
  SPI.transfer(CALIBRATE_REG_CMD_LSBs);  //Send second byte (LSBs)

  // take the chip select high to de-select:
  digitalWrite(CSpin, HIGH);

  // Now send the 9 dummy commands, reading always safer than writing.
  int NUM_DUMMY_CMD  = 9;
  uint16_t dummyRead;

  SERIALNAME->print("Sending dummy commands to complete ADC calibration....");
  for (int n = 0; n <= NUM_DUMMY_CMD; n++) {
    dummyRead = readRegister(60); // reg60: die revision

    /* for debug
      SERIALNAME->println(dummyRead, BIN);
      SERIALNAME->print("Calibration cycle:  ");
      SERIALNAME->println(n + 1);
    */
  }
  SERIALNAME->println("FINISHED!");
  SERIALNAME->write(TERM_CHAR); // print termination character, to keep synced with LabView VISA R/W operations.
}

