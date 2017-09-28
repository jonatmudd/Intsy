
// -------------------------------------
// DEFINE SPI HARDWARE PINS for TEENSY 3.1 to INTAN RHD2132 interface
//  07 Nov 2016, JE
// Last Modified: 20 Dec 2016: option to divide down SPI clock
//                01 May 2017: changing SPI clock speed for config only
// ------------------------------------
const int CSpin = 10; // Custom chipSelect Pin Selected by us
const int MOSIpin = 11;
const int MISOpin = 12;
const int SCKpin = 13;
const int INTAN_MAX_SPI_TRANSFER_RATE = 24000000; // 24 MHz is max SPI CLK rate for  RHD2000 see page 15 of datasheet.
const int CLK_DIV = 2;  // divisor for max transfer rate.  Setting SPI CLK = Max transfer rate, sometimes leads to convert errors. JE 20 Dec 2016
const int SPI_TRANSFER_RATE_DIV = INTAN_MAX_SPI_TRANSFER_RATE / CLK_DIV;

SPISettings SPIsettingsFast(SPI_TRANSFER_RATE_DIV, MSBFIRST, SPI_MODE0); // normal faster SPI bus settings for converting adc data, etc.
SPISettings SPIsettingsConfig(SPI_TRANSFER_RATE_DIV, MSBFIRST, SPI_MODE0);  //slow down clock in hopes that config of apwr registers is more robust.  JE 01 may 2017. 
                                                                                    // Factor of 16 clk div seems to work much better, much more robust than clk div = 2.
                                                                                    // Note however that clk div = 32 does NOT work. (too slow on SPI bus??)



// function for config and begin of SPI interface
// use of header and extern taken from here: http://electronics.stackexchange.com/questions/21025/call-serial-print-in-a-separate-tab-header-file
extern void SPI_init();
extern void SPI_verify();
extern uint16_t SPI_getAmplInfo();

