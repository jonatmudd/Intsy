
// -----------------------------------------------------------------------------
// DEFINING BANDPASS FILTER SETTINGS for RHD2132
// Register bits for filters defined below.
// For setting off-chip RL, we would set ADC_AUX3_EN = 0;
// -----------------------------------------------------------------------------


// Define function prototype to get user settings 
extern void configFilterSettings();



// JE: limited options to sensible ones for measuring GI.  Could also think about measuring spikes with higher 
const byte upperBand[13][4] = {
  {33, 0, 37, 0},
  {3, 1, 13, 1},
  {13, 1, 25, 1},
  {27, 1, 44, 1},
  {1, 2, 23, 2},
  {46, 2, 30, 3},
  {41, 3, 36, 4},
  {30, 5, 43, 6},
  {6, 9, 2, 11},
  {42, 10, 5, 13},
  {24, 13, 7, 16},
  {44, 17, 8, 21},
  {38, 26, 5, 31}
} ;// for 5, 3, 2.5, 2.0, 1.5, 1.0 kHz, 750, 500, 300, 250, 200, 150, 100 Hz respectively.


// for now, limited to choices that make sense for GI purposes

const byte lowerBand[13][3] = {
  {5, 1, 0},
  {18, 1, 0},
  {40, 1, 0},
  {20, 2, 0},
  {42, 2, 0},
  {8, 3, 0},
  {9, 4, 0},
  {44, 6, 0},
  {49, 9, 0},
  {35, 17, 0},
  {1, 40, 0},
  {56, 54, 0},
  {16, 60, 1}
}; // corresponds to:  10, 7.5, 5.0, 3.0, 2.5, 2, 1.5, 1, 0.75, 0.50, 0.30, 0.25, 0.1 Hz);




