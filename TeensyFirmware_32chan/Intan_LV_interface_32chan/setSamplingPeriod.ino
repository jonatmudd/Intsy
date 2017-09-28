// ----------------------------------------
// Set user defined sampling period
//  01 Dec 2016, JE
// ---------------------------------------


const int computeSamplingPeriod() {

  long SAMPLE_PERIOD_MICROS;

// JE 12 dec 2016, not printing this in LV
// SERIALNAME->println("Enter sampling period (per channel) in Hz");
   
  // print short string to acknowledge the event was received and we've entered the subfunction
  // this is needed to help time LV and prevent race conditions
   
   //SERIALNAME->println("m_ack");
   SERIALNAME->println("Input desired sampling frequency (Hz). Return string is actual Fs (Hz) ");
   SERIALNAME->write(TERM_CHAR); // print termination character, to keep synced with LabView VISA R/W operations.
                                 


  // wait for user to input desired sampling rate
  while (!SERIALNAME->available()) {
    // wait for user input
  }


  int fs = SERIALNAME->parseInt();
  serialFlush();

  SAMPLE_PERIOD_MICROS = round(1000000 / fs);
  float FS = 1000000/SAMPLE_PERIOD_MICROS; //sampling rate in Hz
  
  //SERIALNAME->print("Sampling period (us) set to = ");
   //SERIALNAME->println(SAMPLE_PERIOD_MICROS);
   //SERIALNAME->print("Actual Sampling Frequency (Hz) = ");
   SERIALNAME->println(FS);
   SERIALNAME->write(TERM_CHAR); // print termination character, to keep synced with LabView VISA R/W operations.
  
  
  // SERIALNAME->print("Actual FS (Hz) = ");
  // SERIALNAME->println(FS);
  
  
  return SAMPLE_PERIOD_MICROS; //return sample period. used to set the Teensy Timer Interval

}

