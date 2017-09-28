
// -----------------------------------------------------------------------------
// DEFINE sample rate that controls ADC convert and serial write
// Uses Teensyduino interrupt Timer object
// -----------------------------------------------------------------------------


// Define function prototype to get user settings 
extern const int computeSamplingPeriod();
#define DEFAULT_TS  10000;  // define default sampling rate in microsec.  10000 us = 10 ms = 100 Hz. This is per channel rate.

