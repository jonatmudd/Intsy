
// ----------------------------------------
// Configure and initializes the SPI interface between Teensy 3.1 and Intan RHD2132
// Created: 07 Nov 2016, JE
// 
// Last modified:  19 June 2017, JE: adding compatibilty for 2x Intan RHD2132 (64 chan setup)
// ---------------------------------------


void SPI_init() {


  SERIALNAME->print("Begin and configuring SPI bus now...");

  SPI.begin();
  SPI.setSCK(SCKpin);
  SPI.setMOSI(MOSIpin);
  SPI.setMISO(MISOpin);

  //SPI.beginTransaction(SPIsettingsIntan);
  SPI.beginTransaction(SPISettings(SPI_TRANSFER_RATE_DIV, MSBFIRST, SPI_MODE0));  //Based on INTAN requirements, but clock divided down from max to make sure SPI transfer occurs safely

  // Set pin chip select pins high to avoid conficts with the Intan amp we want to address.
  pinMode(CSpinA, OUTPUT);
  digitalWrite(CSpinA, HIGH);

  pinMode(CSpinB, OUTPUT);
  digitalWrite(CSpinB, HIGH);

  
  /* set weak_MISO = 0 on both chips. This needs to be written in Reg4:D[7]
    We must set this bit prior to any other commands on RHD2000 chip.
    (otherwise, we end up with undesired MISO line conflict until it is finally configured)
    dataRegister[4] = (WEAK_MISO << 7) | (TWOSCOMP << 6) | (ABSMODE << 5) | (DSPen << 4) | DSP_cutoff;
    JE 26 June 2017   */
  byte WEAK_MISO_REG_NUM = 4;
  byte WEAK_MISO_DATA_BYTE = (WEAK_MISO << 7);

  writeRegister(WEAK_MISO_REG_NUM, WEAK_MISO_DATA_BYTE, CSpinA);
  writeRegister(WEAK_MISO_REG_NUM, WEAK_MISO_DATA_BYTE, CSpinB);

//  digitalWrite(CSpinA, HIGH);
//  digitalWrite(CSpinB, HIGH);
  SPI.endTransaction();
  //SERIALNAME->println("SPI transaction begun.");
  SERIALNAME->println("Finished!");
  SERIALNAME->write(TERM_CHAR); //write termination character, used to sync with LabView

}



void SPI_verify() {
  // --------------------------------------------------------
  // --------- Verify SPI interface to INTAN RHD 2132 -------
  // --------------------------------------------------------

  // Verify fidelity of SPI interface by reading info stored in amplifier read only registers
  // JE: better way of reading and printing info from registers 40-44, 60-63


  // change from byte to uint16_t, as prototype for readRegister changed to output (uint16_t) instead of (byte). JE 07 Nov 2016
  uint16_t CompanyDesignation[5]; //read INTAN company designation in registeres 40-44 (5 bytes in ASCII). CompanyDesignation[0] should hold 'I' etc.


  SPI.beginTransaction(SPIsettingsFast); //

  // -------------------------------
  //    configure amp A, then  B
  //    same config settings for both amps
  //    chip select lines set in SPIconfig.h
  // ------------------------------

  int thisCSpin;
  for (int k = 0; k < 2 ; k++) { //loop twice through, once for A, once for B, set chip select accordingly

    // set chip select pin to 10 or 9

    if (k == 0) {
      thisCSpin = CSpinA;  //for amp A
      SERIALNAME->println("amp A verifying SPI link now.");
    }
    else {
      thisCSpin = CSpinB;  // for ampB
      SERIALNAME->println("amp B verifying SPI link now.");
    }

    // read company designation from registers 40-44
    throwAwayRead =          readRegister(40, thisCSpin); //need two extra throw away values due to 2 command lag of SPI data returning.
    throwAwayRead =          readRegister(41, thisCSpin); //First 2 responses are non-sensical here, throw them away
    CompanyDesignation[0] =  readRegister(42, thisCSpin);
    CompanyDesignation[1] =  readRegister(43, thisCSpin);
    CompanyDesignation[2] =  readRegister(44, thisCSpin);
    CompanyDesignation[3] =  readRegister(40, thisCSpin); //Let's just read regNum 40 again.  readRegister(0) would also be ok, but let's not mess with it.
    CompanyDesignation[4] =  readRegister(40, thisCSpin);



    // print otu contents of registers we just read
    SERIALNAME->println("Reading and printing contents of registers 40-44. Check link is verified.  Should spell out INTAN below:");

    for (int n = 0; n < (sizeof(CompanyDesignation) / sizeof(CompanyDesignation[0])); n++) {
      SERIALNAME->write(CompanyDesignation[n]); //prints out in ASCII
    }

    SERIALNAME->println(); //prints out in ASCII

    SPI.endTransaction();
  }
  SERIALNAME->write(TERM_CHAR); //write termination character, used to sync with LabView

}



// -----------------------------------------------------
//  Get amplifier info, die revision, unipolar, etc.
// -----------------------------------------------------
uint16_t SPI_getAmplInfo() { // returns number of amplifiers on chip

  uint16_t numAmps = 0; // return value
  uint16_t AmplifierInfo[4]; //read amplifier information  and die revision in registers 60-63. See pg 23 of Intan RHD2000 datasheet


  SPI.beginTransaction(SPIsettingsFast); // Using slower clock speed for config, hopefully more robust for apwr reg etc. JE 01 may 2017


  int thisCSpin;
  for (int k = 0; k < 2 ; k++) { //loop twice through, once for A, once for B, set chip select accordingly

    // set chip select pin to 10 or 9

    if (k == 0) {
      thisCSpin = CSpinA;  //for amp A
      SERIALNAME->println("amp A get info now.");
    }
    else {
      thisCSpin = CSpinB;  // for ampB
      SERIALNAME->println("amp B get info now.");
    }

    //read amplifier info from registers 60-63
    throwAwayRead = readRegister(60, thisCSpin); // reg60: die revision
    throwAwayRead = readRegister(61, thisCSpin); // reg61: unipolar/bipolar (should be 1 for us since we use bipolar RHD2132)
    AmplifierInfo[0] =  readRegister(62, thisCSpin); // reg62: number of amplifers. should be 32
    AmplifierInfo[1] =  readRegister(63, thisCSpin); // reg 63: Intan chip ID, should be 1 for RHD2132
    AmplifierInfo[2] =  readRegister(63, thisCSpin); // call 2 more readRegister commands  to get all the data returned
    AmplifierInfo[3] =  readRegister(63, thisCSpin);


    numAmps = numAmps + AmplifierInfo[2]; //this should be either 32 or 64, depending on if 1 or 2 intan chips are present.
    // could use this in future to determine the actual user config.
    // for now, just assume 64 chans, both amps plugged in.  makes programming easier.
    
    //print amplifier info
    if (AmplifierInfo[3] == 1) {
      SERIALNAME->print("Chip ID: RHD2132");//value of 1 encodes for RHD2132 chip
      SERIALNAME->println("");//print new line
    }
    else { //could enter print statements for other chips here, but we won't bother now
      SERIALNAME->print("Amplifier Reg 63: ");//value of 1 encodes for RHD2132 chip
      SERIALNAME->println(AmplifierInfo[3]);//value of 1 encodes for RHD2132 chip
    }

    SERIALNAME->print("Number of amplifiers: ");
    SERIALNAME->println(AmplifierInfo[2]);
    if (AmplifierInfo[1] == 1) { //unipolar = common reference design
      SERIALNAME->println("Unipolar configuration = common reference");//value of 1 encodes for unipolar config characteristic of RHD2132
    }

    SERIALNAME->print("Die revision: ");
    SERIALNAME->println(AmplifierInfo[0]);
  }

  SPI.endTransaction();
  SERIALNAME->write(TERM_CHAR); //write termination character, used to sync with LabView
  SERIALNAME->flush(); // wait for all data in Tx buffer to transmit


  // return number of amplifiers
  return numAmps;

}
