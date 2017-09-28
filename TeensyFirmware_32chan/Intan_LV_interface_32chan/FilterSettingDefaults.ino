// ----------------------------------------
//  Get user defined filter settings
//  07 Nov 2016, JE
//  User asked 1) use default values; if not 2) to choose selection for lower and upper cutoffs
//  NB: In future, need to expand offerings to use off-chip resistor to set 0.01 Hz lower cutoff
// ---------------------------------------


void configFilterSettings() {


  SPI.beginTransaction(SPIsettingsConfig); // Using slower clock speed for config, hopefully more robust for apwr reg etc. JE 01 may 2017


  // Now that user has selected BPF settings, configure bytes and write registers 8-13 with filter settings
  // registers 8-13 are on chip amplifier bandwidth select.  Note that X = 0 is set in bits D[6] and D[5] where appropriate (see Intan RHD2000 datasheet, pg 22)
  const int NUM_FILT_REG = 6; //  number of writeable registers. (There are more read-only ones, in addition)
  byte onchipBPF[NUM_FILT_REG]; //JE: changed to 8 registers 08 Nov 2016, breaking up functionality for easier UI programming


  // Define bits to write to registers
  byte OFFCHIP_RH1 = 0; // **   set to 0 for on board resistors, we plan to use on-board resistors always for high cutoff
  byte OFFCHIP_RH2 = 0; // **
  int OFFCHIP_RL;  // ** user selects below whether to use on or off-chip. Default is to use on-chip R_L.

  // JE 03 Nov 2016: For R_L off-chip will have to change ADC_AUX3_EN ?
  byte ADC_AUX1_EN = 1 ;  // all set to 1 for enabling auxiliary as ADC inputs
  byte ADC_AUX2_EN = 1 ;

  int ADC_AUX3_EN;  // this bit should be set to complement of OFFCHIP_RL (=0 when OFFCHIP_RL is active).  If set to 1, doesn't matter?



  byte RH1_DAC1;
  byte RH1_DAC2;

  byte RH2_DAC1;
  byte RH2_DAC2;

  byte RL_DAC1;
  byte RL_DAC2;
  byte RL_DAC3;

  int upperBandIndex;
  int lowerBandIndex;

  char inChar;

  /* JE 12 Dec 2016. No defaults chosen here. They will be chosen in LV  instead.
      The values from LV will be passed in to set bits in Intan register.


    SERIALNAME->println("Filter settings: Enter 0 for default filter settings (0.1 Hz - 5 kHz) or  1 for custom");

    // JE: 12 Dec 2016.  First set R_L bit
    while (!SERIALNAME->available()) {
      // wait for user input
    }


    //  int defaultSettings = SERIALNAME->parseInt();
    //  serialFlush();
    //
    //  SERIALNAME->print("Default Setting Selection: ");
    //  SERIALNAME->println(defaultSettings);
    //  SERIALNAME->println();
    //
    //  //JE default settings are for 0.1 Hz - 5 Hz
    //  if (defaultSettings == 0) {
    //
    //    // Use on-chip resistors, this allows aux measurements to be made too
    //    OFFCHIP_RL  = 0;  // use on-chip R_L
    //
    //    // JE 03 Nov 2016: For R_L off-chip will have to change ADC_AUX3_EN ?
    //    ADC_AUX3_EN = 1;  // this bit should be set to complement of OFFCHIP_RL (=0 when OFFCHIP_RL is active).  If set to 1, doesn't matter?
    //
    //
    //    // These correspond to 0.1 Hz - 5 kHz. See Intan RDH2000 series manual
    //    RH1_DAC1 = 33;
    //    RH1_DAC2 = 0;
    //
    //    RH2_DAC1 = 37;
    //    RH2_DAC2 = 0;
    //
    //    RL_DAC1 = 16;
    //    RL_DAC2 = 60;
    //    RL_DAC3 = 1;
    //  }


    //  else {//if (defaultSettings !=0){
  */


  // JE Not printing messages to serial. LV interface will make obvious what we are setting and must do it in prescribed order to match block diagram flow.
  // choose whether to use off-chip resistor

  //SERIALNAME->println( "Use off-chip resistor R_L?     1 = yes; 0 = no (= use on-chip R_L)");

  //choose whether to use on or off-chip resistor for R_L. For GI applications, we eventually intend to use off-chip to set 0.01 Hz lower cutoff.
  // Haven't made explicit option here to set offchip R_H1 and RH_2 as there is no future application for them, at present.
  // LV will send three numbers, read them all before flushing. This avoid races between LV sending serial to Teensy vs. Teensy serialFlush.

  // print short string to acknowledge the event was received and we've entered the subfunction
  // LV will look for this to be written to VISA resource.  This is needed to help time LV and prevent race conditions

  //SERIALNAME->println("f_ack");
  SERIALNAME->println("Enter filter settings for on/off chip RL, upper and lower band index (comma separated): ");
  SERIALNAME->write(TERM_CHAR); // print termination character, to keep synced with LabView VISA R/W operations.


  // wait for user input, this will only be sent after Labview receives the previous termination character as a sort of ACK.
  // get input for off-chip R_L
  while (!SERIALNAME->available()) {
  }

  inChar = SERIALNAME->read();
  OFFCHIP_RL = inChar - '0'; // was .parseInt(). Changed 13 Dec 2016 JE


  // get input for upper band index. wait here until the byte becomes available
  while (!SERIALNAME->available()) {
  }

  //char inChar2 = SERIALNAME->read();
  //upperBandIndex = inChar2 - '0';
  upperBandIndex = SERIALNAME->parseInt();

  // get input for lower band index
  while (!SERIALNAME->available()) {
  }

  //  char inChar3 = SERIALNAME->read();
  // lowerBandIndex = inChar3 - '0';

  lowerBandIndex = SERIALNAME->parseInt();

  serialFlush(); //flush the read buffer



  //must be sure to turn off ADC_AUX3_EN, in case OFF_CHIP_RL is turned on
  if (OFFCHIP_RL == 1) {
    ADC_AUX3_EN = 0;
  }
  else {
    ADC_AUX3_EN = 1;
  }

  SERIALNAME->print("Set OFFCHIP_RL = ");
  SERIALNAME->println(OFFCHIP_RL);
  SERIALNAME->print("Setting ADC_AUX3_EN = ");
  SERIALNAME->println(ADC_AUX3_EN);



  SERIALNAME->print("Set upper band index to: ");
  SERIALNAME->println(upperBandIndex);
  SERIALNAME->print("Set lower band index: ");
  SERIALNAME->println(lowerBandIndex);
  SERIALNAME->println();



  RH1_DAC1 = upperBand[upperBandIndex][0];
  RH1_DAC2 = upperBand[upperBandIndex][1];

  RH2_DAC1 = upperBand[upperBandIndex][2];
  RH2_DAC2 = upperBand[upperBandIndex][3];

  RL_DAC1 = lowerBand[lowerBandIndex][0];
  RL_DAC2 = lowerBand[lowerBandIndex][1];
  RL_DAC3 = lowerBand[lowerBandIndex][2];



  onchipBPF[0] =  (OFFCHIP_RH1 << 7) | RH1_DAC1;
  onchipBPF[1] =  (ADC_AUX1_EN << 7) | RH1_DAC2;
  onchipBPF[2] =  (OFFCHIP_RH2 << 7) | RH2_DAC1;
  onchipBPF[3] =  (ADC_AUX2_EN << 7) | RH2_DAC2;
  onchipBPF[4] =  (OFFCHIP_RL  << 7) | RL_DAC1;
  onchipBPF[5] =  (ADC_AUX3_EN << 7) | (RL_DAC3 << 6) | RL_DAC2;


  for (int i = 0; i < NUM_FILT_REG; i++) {
    writeRegister(byte(i + 8), onchipBPF[i]); // Note +8 offset to write to proper register address, starting with 8
  }

  //call 2 dummy commands to make sure apwr is set properly
  readRegister(43);// 43 returns  'A' from INTAN.
  readRegister(44);// 44 returns  last 'N' from INTAN


  // read back filter settings to confirm they were configured properly
  uint16_t filtRegSettings [NUM_FILT_REG];   // allocate memory space for auxiliary commands included in each sampling period

  //SPI pipe delay of 2 commands, hence 2 offset
                        readRegister(8);  //read MISC registers
                        readRegister(9);
  filtRegSettings [0] = readRegister(10);
  filtRegSettings [1] = readRegister(11);
  filtRegSettings [2] = readRegister(12);
  filtRegSettings [3] = readRegister(13);
  filtRegSettings [4] = readRegister(43); // 2 more dummy commands to finish getting all apwrRegSettings
  filtRegSettings [5] = readRegister(44);



  // print contents of Reg 8-13
  SERIALNAME->println("Filter register configuration (leading 0's not shown):");

  for (int i = 0 ; i < NUM_FILT_REG; i++) {
    SERIALNAME->print(i + 8);
    SERIALNAME->print(": \t");
    SERIALNAME->println(filtRegSettings[i], BIN);
  }

  SERIALNAME->write(TERM_CHAR); // print termination character, to keep synced with LabView VISA R/W operations.
  SERIALNAME->flush(); // wait for all bytes to Tx

  SPI.endTransaction();
}
