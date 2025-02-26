
/* ---------------------------------------------------------------------
  Configure intan chip registers 0-7 and 14-17 based on user input.
  Note: Reg 8-13 used for filter settings. Those are configured in separate
  function FilterSettingDefaults.ino


  Last modified: JE 04 June 2019
                - adding SPI1 for amps C and D (128ch system)
                - fast settle function implemented upon power up, see pg 24 of Intan RHD2000 datasheet
                ------------------------------------------------------------------- 
 */



// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
//                                                     SPI PORT 0 (amps A and B)
// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
void ConfigureRegisters_0_17() {


  SPI.beginTransaction(SPIsettingsFast);

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
      SERIALNAME->println("amp A configuring registers 0-7 now.");
    }
    else {
      thisCSpin = CSpinB;  // for ampB
      SERIALNAME->println("amp B configuring registers 0-7 now.");
    }




    // Take care of REG 0-7
    const int NUM_MISC_REG = 8; //  number of writeable registers. (There are more read-only ones, in addition)
    byte dataRegister[NUM_MISC_REG]; //JE: changed to 8 registers 08 Nov 2016, breaking up functionality for easier UI programming

    // Take care of REG 14-17
    const int NUM_APWR = 4; // Individual power ampliifer settings
    byte apwrRegister[NUM_APWR]; //JE: changed to 17 registers 08 Nov 2016



    // JE: updated 29 Nov 2016 to add potential for more user options in future, and to make more explicit what bits are set
    // dataRegister[0] = 0b11011110; //ADC configuration and amplfier fast settle.
    //dataRegister[1] = ADC_BUFFER_BIAS | (VDD_SENSE_EN << 6); //Supply sensor and ADC Buffer Bias Current

    dataRegister[0] = (ADC_REF_BW << 6) | (AMP_fast_settle << 5) | (AMP_Vref_En << 4) | (ADC_comparator_bias << 2) | (ADC_comparator_select << 0); // was << 3 and << 1 till 7pm on 17 may 2017.  Bits not set properly.
    dataRegister[1] = (VDD_SENSE_EN << 6) |  ADC_BUFFER_BIAS; //Supply sensor and ADC Buffer Bias Current; Note bit D[7]  = X is set to 0 for now
    dataRegister[2] =  MUX_BIAS; // MUX Bias current 0bxx(muxbias[5:0]); bits 6 and 7 marked X = set to 0 for now.
    dataRegister[3] = (MUX_LOAD << 5) | (TEMP_S2 << 4) | (TEMP_S1 << 3) | (TEMP_EN << 2) | (DIGOUT_HiZ << 1) | DIGOUT; // all other bits must be set to 0 to save power. // JE: was << 5 (no effect, since = 0),
    dataRegister[4] = (WEAK_MISO << 7) | (TWOSCOMP << 6) | (ABSMODE << 5) | (DSPen << 4) | DSP_cutoff;
    dataRegister[5] = (Z_DAC_PWR << 6) | (Z_LOAD << 5) | (Z_SCALE << 3) | (Z_CONN_ALL << 2) | (Z_SEL_POL << 1) | Z_EN ; // Impedance check control
    dataRegister[6] = Z_DAC; // Impedance Check DAC
    dataRegister[7] = Zcheck_SEL ; //Impedance check amplifier select

    //Print desired settings for reg 0-7:
    SERIALNAME->println("Desired configuration of registers 0-7:");
    // print contents of Reg 0-7
    for (int i = 0 ; i < NUM_MISC_REG; i++) {
      SERIALNAME->print(i);
      SERIALNAME->print(": \t");
      SERIALNAME->println(dataRegister[i], BIN);
    }



    // configure Misc registers 0-7
    for (int registerNumber = 0 ; registerNumber < NUM_MISC_REG; registerNumber++) {
      writeRegister(byte(registerNumber), dataRegister[registerNumber], thisCSpin);
    }

    // configure APWR registesr
    //  Set  all bits = 1 for Reg 14-17, normal operation to power up each individual bioamplifier
    // byte apwrRegisterAllOn = 0b11111111; // defined in header file
    for (int i = 0; i < NUM_APWR; i++) {
      writeRegister(byte(i + 14), apwrRegisterAllOn, thisCSpin); // Note +14 offset to write to proper registerapwrRegisterAllOn
    }


    // ------------------------------------------
    // Implement fast settle function "It is recommended (though not required) to hold amp fast settle high momentarily
    // after powering up the chip if low values of fL (< 1 Hz) are used." (Intan datasheet page 24)
    // -----------------------------------------

    // byte registerNumberZero = 0;


    // set bit to high to reset amplifier
    AMP_fast_settle = 1;
    dataRegister[0] = (ADC_REF_BW << 6) | (AMP_fast_settle << 5) | (AMP_Vref_En << 4) | (ADC_comparator_bias << 2) | (ADC_comparator_select << 0); // was << 3 and << 1 till 7pm on 17 may 2017.  Bits not set properly.
    writeRegister(byte(0), dataRegister[0], thisCSpin);

    //wait 'momentarily'
    delay(50); // 50 ms long enough?

    // set bit to low to resume normal amplifier function
    AMP_fast_settle = 0;
    dataRegister[0] = (ADC_REF_BW << 6) | (AMP_fast_settle << 5) | (AMP_Vref_En << 4) | (ADC_comparator_bias << 2) | (ADC_comparator_select << 0); // was << 3 and << 1 till 7pm on 17 may 2017.  Bits not set properly.
    writeRegister(byte(0), dataRegister[0], thisCSpin);


    //call 2 dummy commands to make sure all SPI commands have had time to transfer properly with 2 cmd delay
    readRegister(43, thisCSpin);// 43 returns  'A' from INTAN.
    readRegister(44, thisCSpin);// 44 returns  last 'N' from INTAN




    /*Now let's read out the contents of the registers we just wrote.*/
    uint16_t miscRegSettings [NUM_MISC_REG];   // allocate memory space for auxiliary commands included in each sampling period
    uint16_t apwrRegSettings [NUM_APWR];   // allocate memory space for auxiliary commands included in each sampling period

    //SPI pipe delay of 2 commands, hence 2 offset
    readRegister(0, thisCSpin);  //read MISC registers
    readRegister(1, thisCSpin);
    miscRegSettings [0] = readRegister(2, thisCSpin);
    miscRegSettings [1] = readRegister(3, thisCSpin);
    miscRegSettings [2] = readRegister(4, thisCSpin);
    miscRegSettings [3] = readRegister(5, thisCSpin);
    miscRegSettings [4] = readRegister(6, thisCSpin);
    miscRegSettings [5] = readRegister(7, thisCSpin);
    miscRegSettings [6] = readRegister(14, thisCSpin); //read APWR registers
    miscRegSettings [7] = readRegister(15, thisCSpin);

    apwrRegSettings [0] = readRegister(16, thisCSpin);
    apwrRegSettings [1] = readRegister(17, thisCSpin);
    apwrRegSettings [2] = readRegister(43, thisCSpin); // 2 more dummy commands to finish getting all apwrRegSettings
    apwrRegSettings [3] = readRegister(44, thisCSpin);


    SERIALNAME->println("Finished register configuration ");

    SERIALNAME->println("Configuration of registers 0-7:");
    // print contents of Reg 0-7
    for (int i = 0 ; i < NUM_MISC_REG; i++) {
      SERIALNAME->print(i);
      SERIALNAME->print(": \t");
      SERIALNAME->println(miscRegSettings[i], BIN);
    }


    SERIALNAME->println("Configured apwr registers 14-17:");

    // print contents of Reg 14-17
    for (int i = 0 ; i < NUM_APWR; i++) {
      SERIALNAME->print(i + 14);
      SERIALNAME->print(": \t");
      SERIALNAME->println(apwrRegSettings[i], BIN);
    }


  } //end for loop over amps A and B

  SERIALNAME->write(TERM_CHAR); // print termination character, to keep synced with LabView VISA R/W operations
  SERIALNAME->flush(); // wait for all data in Tx buffer to transmit

  // end this SPI transaction with slower speed
  SPI.endTransaction();

}



// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
//                                                     SPI PORT 1 (amps C and D)
// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
void ConfigureRegisters_0_17_SPI1() {


  SPI1.beginTransaction(SPIsettingsFast);

  // -------------------------------
  //    configure amp C, then  D
  //    same config settings for both amps
  //    chip select lines set in SPIconfig.h
  // ------------------------------

  int thisCSpin;
  for (int k = 0; k < 2 ; k++) { //loop twice through, once for A, once for B, set chip select accordingly

    // set chip select pin

    if (k == 0) {
      thisCSpin = CSpinC;  //for amp C
      SERIALNAME->println("amp C configuring registers 0-7 now.");
    }
    else {
      thisCSpin = CSpinD;  // for amp D
      SERIALNAME->println("amp D configuring registers 0-7 now.");
    }




    // Take care of REG 0-7
    const int NUM_MISC_REG = 8; //  number of writeable registers. (There are more read-only ones, in addition)
    byte dataRegister[NUM_MISC_REG]; //JE: changed to 8 registers 08 Nov 2016, breaking up functionality for easier UI programming

    // Take care of REG 14-17
    const int NUM_APWR = 4; // Individual power ampliifer settings
    byte apwrRegister[NUM_APWR]; //JE: changed to 17 registers 08 Nov 2016



    // JE: updated 29 Nov 2016 to add potential for more user options in future, and to make more explicit what bits are set
    // dataRegister[0] = 0b11011110; //ADC configuration and amplfier fast settle.
    //dataRegister[1] = ADC_BUFFER_BIAS | (VDD_SENSE_EN << 6); //Supply sensor and ADC Buffer Bias Current

    dataRegister[0] = (ADC_REF_BW << 6) | (AMP_fast_settle << 5) | (AMP_Vref_En << 4) | (ADC_comparator_bias << 2) | (ADC_comparator_select << 0); // was << 3 and << 1 till 7pm on 17 may 2017.  Bits not set properly.
    dataRegister[1] = (VDD_SENSE_EN << 6) |  ADC_BUFFER_BIAS; //Supply sensor and ADC Buffer Bias Current; Note bit D[7]  = X is set to 0 for now
    dataRegister[2] =  MUX_BIAS; // MUX Bias current 0bxx(muxbias[5:0]); bits 6 and 7 marked X = set to 0 for now.
    dataRegister[3] = (MUX_LOAD << 5) | (TEMP_S2 << 4) | (TEMP_S1 << 3) | (TEMP_EN << 2) | (DIGOUT_HiZ << 1) | DIGOUT; // all other bits must be set to 0 to save power. // JE: was << 5 (no effect, since = 0),
    dataRegister[4] = (WEAK_MISO << 7) | (TWOSCOMP << 6) | (ABSMODE << 5) | (DSPen << 4) | DSP_cutoff;
    dataRegister[5] = (Z_DAC_PWR << 6) | (Z_LOAD << 5) | (Z_SCALE << 3) | (Z_CONN_ALL << 2) | (Z_SEL_POL << 1) | Z_EN ; // Impedance check control
    dataRegister[6] = Z_DAC; // Impedance Check DAC
    dataRegister[7] = Zcheck_SEL ; //Impedance check amplifier select

    //Print desired settings for reg 0-7:
    SERIALNAME->println("Desired configuration of registers 0-7:");
    // print contents of Reg 0-7
    for (int i = 0 ; i < NUM_MISC_REG; i++) {
      SERIALNAME->print(i);
      SERIALNAME->print(": \t");
      SERIALNAME->println(dataRegister[i], BIN);
    }



    // configure Misc registers 0-7
    for (int registerNumber = 0 ; registerNumber < NUM_MISC_REG; registerNumber++) {
      writeRegister_SPI1(byte(registerNumber), dataRegister[registerNumber], thisCSpin);
    }

    // configure APWR registesr
    //  Set  all bits = 1 for Reg 14-17, normal operation to power up each individual bioamplifier
    // byte apwrRegisterAllOn = 0b11111111; // defined in header file
    for (int i = 0; i < NUM_APWR; i++) {
      writeRegister_SPI1(byte(i + 14), apwrRegisterAllOn, thisCSpin); // Note +14 offset to write to proper registerapwrRegisterSetOn
    }


    // ------------------------------------------
    // Implement fast settle function "It is recommended (though not required) to hold amp fast settle high momentarily
    // after powering up the chip if low values of fL (< 1 Hz) are used." (Intan datasheet page 24)
    // -----------------------------------------

   


    // set bit to high to reset amplifier
    AMP_fast_settle = 1;
    dataRegister[0] = (ADC_REF_BW << 6) | (AMP_fast_settle << 5) | (AMP_Vref_En << 4) | (ADC_comparator_bias << 2) | (ADC_comparator_select << 0); // was << 3 and << 1 till 7pm on 17 may 2017.  Bits not set properly.
    writeRegister_SPI1(byte(0), dataRegister[0], thisCSpin);

    //wait 'momentarily'
    delay(50); // 50 ms long enough?

    // set bit to low to resume normal amplifier function
    AMP_fast_settle = 0;
    dataRegister[0] = (ADC_REF_BW << 6) | (AMP_fast_settle << 5) | (AMP_Vref_En << 4) | (ADC_comparator_bias << 2) | (ADC_comparator_select << 0); // was << 3 and << 1 till 7pm on 17 may 2017.  Bits not set properly.
    writeRegister_SPI1(byte(0), dataRegister[0], thisCSpin);


    //call 2 dummy commands to make sure all SPI bits transferred properly with 2 cmd delay in SPI pipe
    readRegister_SPI1(43, thisCSpin);// 43 returns  'A' from INTAN.
    readRegister_SPI1(44, thisCSpin);// 44 returns  last 'N' from INTAN




    /*Now let's read out the contents of the registers we just wrote.*/
    uint16_t miscRegSettings [NUM_MISC_REG];   // allocate memory space for auxiliary commands included in each sampling period
    uint16_t apwrRegSettings [NUM_APWR];   // allocate memory space for auxiliary commands included in each sampling period

    //SPI pipe delay of 2 commands, hence 2 offset
    readRegister_SPI1(0, thisCSpin);  //read MISC registers
    readRegister_SPI1(1, thisCSpin);
    miscRegSettings [0] = readRegister_SPI1(2, thisCSpin);
    miscRegSettings [1] = readRegister_SPI1(3, thisCSpin);
    miscRegSettings [2] = readRegister_SPI1(4, thisCSpin);
    miscRegSettings [3] = readRegister_SPI1(5, thisCSpin);
    miscRegSettings [4] = readRegister_SPI1(6, thisCSpin);
    miscRegSettings [5] = readRegister_SPI1(7, thisCSpin);
    miscRegSettings [6] = readRegister_SPI1(14, thisCSpin); //read APWR registers
    miscRegSettings [7] = readRegister_SPI1(15, thisCSpin);

    apwrRegSettings [0] = readRegister_SPI1(16, thisCSpin);
    apwrRegSettings [1] = readRegister_SPI1(17, thisCSpin);
    apwrRegSettings [2] = readRegister_SPI1(43, thisCSpin); // 2 more dummy commands to finish getting all apwrRegSettings
    apwrRegSettings [3] = readRegister_SPI1(44, thisCSpin);


    SERIALNAME->println("Finished register configuration ");

    SERIALNAME->println("Configuration of registers 0-7:");
    // print contents of Reg 0-7
    for (int i = 0 ; i < NUM_MISC_REG; i++) {
      SERIALNAME->print(i);
      SERIALNAME->print(": \t");
      SERIALNAME->println(miscRegSettings[i], BIN);
    }


    SERIALNAME->println("Configured apwr registers 14-17:");

    // print contents of Reg 14-17
    for (int i = 0 ; i < NUM_APWR; i++) {
      SERIALNAME->print(i + 14);
      SERIALNAME->print(": \t");
      SERIALNAME->println(apwrRegSettings[i], BIN);
    }


  } //end for loop over amps C and D

  SERIALNAME->write(TERM_CHAR); // print termination character, to keep synced with LabView VISA R/W operations
  SERIALNAME->flush(); // wait for all data in Tx buffer to transmit

  // end this SPI transaction with slower speed
  SPI1.endTransaction();

}
