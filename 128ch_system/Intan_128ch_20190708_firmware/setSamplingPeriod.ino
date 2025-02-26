// ----------------------------------------
// Set user defined sampling period
// This sets timerInterval object period on Teensy
// Does not configure anything on Intan chips
// Created:  01 Dec 2016, JE
// Last mofified: 
//  -JE 04 June 2019, cleaner coercion of sampling and streaming ratios depending on BT or USB connected
// ---------------------------------------


const int computeSamplingPeriod() {

  long SAMPLE_PERIOD_MICROS;


  SERIALNAME->println("Input desired sampling frequency (Hz). Return string is actual Fs (Hz) ");
  SERIALNAME->write(TERM_CHAR); // print termination character, to keep synced with LabView VISA R/W operations.

  // wait for user to input desired sampling rate
  while (!SERIALNAME->available()) {
    // wait for user input
  }


  int fs = SERIALNAME->parseInt();
  serialFlush();

  SAMPLE_PERIOD_MICROS = round(1000000 / fs);
  float FS = 1000000 / SAMPLE_PERIOD_MICROS; //sampling rate in Hz


  SERIALNAME->println(FS);
  SERIALNAME->write(TERM_CHAR); // print termination character, to keep synced with LabView VISA R/W operations.



  return SAMPLE_PERIOD_MICROS; //return sample period. used to set the Teensy Timer Interval

}


// -------------------------------------------------------
// Compute ratio of UART stream period to sampling period
// -------------------------------------------------------
const int computeSerialStreamRatio(int DT_SAMPLE_MICROS) {

  long DT_Stream;
  int SampleRateSerialRatio;

  SERIALNAME->println("Input desired UART data stream frequency (Hz). Return string is ratio of serial stream period to sampling period (coerced to nearest integer): ");
  SERIALNAME->write(TERM_CHAR); // print termination character, to keep synced with LabView VISA R/W operations.

  // wait for user to input desired sampling rate
  while (!SERIALNAME->available()) {
    // wait for user input
  }


  int fstream = SERIALNAME->parseInt();
  serialFlush();

  DT_Stream = round(1000000 / fstream);
  //float FS = 1000000/Dt_Stream; //sampling rate in Hz

  //coerce bluetooth serial stream period to be at least the minimum period (limit Serial data streaming rate on LV display)
  if (serialID == 1) { // serialID=1 identifies bluetooth connected (whereas serialID = 0 means USB connected) -JE 04 June 2019
    if (DT_Stream < MIN_DT_STREAM_MICROS) {
      DT_Stream = MIN_DT_STREAM_MICROS;
    }
  }

  SampleRateSerialRatio = round(DT_Stream / DT_SAMPLE_MICROS);

  
  // coerce this ratio to always be >=1 otherwise we are asking for streaming that happens faster than we acquire data, -JE 04 June 2019
  if (SampleRateSerialRatio < 1) {
    SampleRateSerialRatio = 1;
  }

  SERIALNAME->println(SampleRateSerialRatio);
  SERIALNAME->write(TERM_CHAR); // print termination character, to keep synced with LabView VISA R/W operations.


  return SampleRateSerialRatio; //return sample period. used to set the Teensy Timer Interval
}
