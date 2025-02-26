
// -------------------------------------
// DEFINE SPI HARDWARE PINS for TEENSY 3.6 to INTAN RHD2132 interface
// Created: 10 July 2018, JE
//  SPI_init(), SPI_verify(), and SPI_getAmplInfo() all try to configure 2x Intan RHD2132
//  
// Last modified:
// JE 04 June 2019, adding second SPI port for  amps C and D 
// ------------------------------------


// SPI0 for amps A and B
const int CSpinA = 9; // chipSelect amp A
const int CSpinB = 10;  // chipSelect amp B
const int MOSIpin = 11;
const int MISOpin = 12;
const int SCKpin = 14; 


// SPI1 for amps C and D
const int CSpinC = 19; // chipSelect amp C
const int CSpinD = 21;  // chipSelect amp D
const int MOSIpin_SPI1 = 0;
const int MISOpin_SPI1 = 1;
const int SCKpin_SPI1 = 20; 



const double INTAN_MAX_SPI_TRANSFER_RATE = 24000000; // 24 MHz is max SPI CLK rate for  RHD2000 see page 15 of datasheet.
const int CLK_DIV = 2;  // divisor for max transfer rate.  Setting SPI CLK = Max transfer rate, sometimes leads to convert errors. JE 20 Dec 2016
const int SPI_TRANSFER_RATE_DIV = INTAN_MAX_SPI_TRANSFER_RATE / CLK_DIV;
//const int SPI_TRANSFER_RATE_DIV = 8000000;

SPISettings SPIsettingsFast(SPI_TRANSFER_RATE_DIV, MSBFIRST, SPI_MODE0); // normal faster SPI bus settings for converting adc data, etc.





// function for config and begin of SPI interface
// use of header and extern taken from here: http://electronics.stackexchange.com/questions/21025/call-serial-print-in-a-separate-tab-header-file

//SPI0 port functions for amps A and B
extern void SPI_init();
extern void SPI_verify();
extern uint16_t SPI_getAmplInfo();

// SPI1 port functions for amps C and D, added 04 june 2019 JE
extern void SPI1_init();
extern void SPI1_verify();
extern uint16_t SPI1_getAmplInfo();
