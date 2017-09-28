
// ----------------------------------------
// Configure and initializes the SPI interface between Teensy 3.1 and Intan RHD2132
// 07 Nov 2016, JE
// ---------------------------------------


void SPI_init() {


  SERIALNAME->print("Begin and configuring SPI bus now...");

  SPI.begin();
  //SPI.usingInterrupt(SPIF); // JE try this ISR name instead 27 apr 2017. Shoudln't have any effect since no other interrupts are triggered by other/competing SPI devices
  //SPI.usingInterrupt(SamplingPeriodTimer); //JE: adding this 13 Feb 2017
  SPI.setSCK(SCKpin);
  SPI.setMOSI(MOSIpin);
  SPI.setMISO(MISOpin);

/* JE 01 may 2017.  Calling SPI.beginTransaction more locally to allow for different SPI bus transfer rates.
 *  Idea is to see if APWR register is set more robustly when writing more slowly
 */
  //SPI.beginTransaction(SPIsettingsIntan);
  //  SPI.beginTransaction(SPISettings(SPI_TRANSFER_RATE_DIV, MSBFIRST, SPI_MODE0));  //Based on INTAN requirements, but clock divided down from max to make sure SPI transfer occurs safely
  pinMode(CSpin, OUTPUT);
  digitalWrite(CSpin, HIGH);

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


  // read company designation from registers 40-44
  SPI.beginTransaction(SPIsettingsFast); // Using slower clock speed for config, hopefully more robust for apwr reg etc. JE 01 may 2017
  throwAwayRead =          readRegister(40); //need two extra throw away values due to 2 command lag of SPI data returning.
  throwAwayRead =          readRegister(41); //First 2 responses are non-sensical here, throw them away
  CompanyDesignation[0] =  readRegister(42);
  CompanyDesignation[1] =  readRegister(43);
  CompanyDesignation[2] =  readRegister(44);
  CompanyDesignation[3] =  readRegister(40); //Let's just read regNum 40 again.  readRegister(0) would also be ok, but let's not mess with it.
  CompanyDesignation[4] =  readRegister(40);
  SPI.endTransaction();


  // print otu contents of registers we just read
  SERIALNAME->println("Reading and printing contents of registers 40-44. Check link is verified.  Should spell out INTAN below:");

  for (int n = 0; n < (sizeof(CompanyDesignation) / sizeof(CompanyDesignation[0])); n++) {
    SERIALNAME->write(CompanyDesignation[n]); //prints out in ASCII
  }

  SERIALNAME->println(); //prints out in ASCII
  SERIALNAME->write(TERM_CHAR); //write termination character, used to sync with LabView
  
}




uint16_t SPI_getAmplInfo() { // returns number of amplifiers on chip

  uint16_t numAmps; // return value
  uint16_t AmplifierInfo[4]; //read amplifier information  and die revision in registers 60-63. See pg 23 of Intan RHD2000 datasheet


  SPI.beginTransaction(SPIsettingsFast); // Using slower clock speed for config, hopefully more robust for apwr reg etc. JE 01 may 2017
  //read amplifier info from registers 60-63
  throwAwayRead = readRegister(60); // reg60: die revision
  throwAwayRead = readRegister(61); // reg61: unipolar/bipolar (should be 1 for us since we use bipolar RHD2132)
  AmplifierInfo[0] =  readRegister(62); // reg62: number of amplifers. should be 32
  AmplifierInfo[1] =  readRegister(63); // reg 63: Intan chip ID, should be 1 for RHD2132
  AmplifierInfo[2] =  readRegister(63); // call 2 more readRegister commands  to get all the data returned
  AmplifierInfo[3] =  readRegister(63);
  SPI.endTransaction();

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

  SERIALNAME->write(TERM_CHAR); //write termination character, used to sync with LabView
  SERIALNAME->flush(); // wait for all data in Tx buffer to transmit
  // return number of amplifiers
  numAmps = AmplifierInfo[2];
  return numAmps;

}
