
// -----------------------------------------------------------------------------
// Define sample rate  and compute sampling period that controls ADC convert and serial write
// Uses Teensyduino interrupt Timer object
// 
// JE 24 Oct 2018: Also compute the ratio between sampling frequency and UART data stream rate.  
// For bluetooth 115200 baud x 64 chans, this maxes around 50 Hz (dependent on host computer)
// -----------------------------------------------------------------------------

const int DEFAULT_TS = 10000;  // define default sampling rate in microsec.  10000 us = 10 ms = 100 Hz. This is per channel rate.
const int MIN_DT_STREAM_MICROS = 20000;  // define default sampling rate in microsec.  20000 us = 20 ms = 50 Hz. This is max data stream rate for 64 chan system UART Tx via Bluetooth

// Define function prototype to get user settings 
extern const int computeSamplingPeriod();
extern const int computeSerialStreamRatio(int DT_SAMP);


