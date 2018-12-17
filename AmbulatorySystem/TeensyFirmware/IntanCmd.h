// -----------------------------------------------------------------------------
// INTAN COMMAND PREFIXES for Read, Write, Calibrate and ADC conversion of chans
// -Modified: JE 19 June 2017 function type defs include CSpin to select which
// Intan amp we want to communicate with.  
// Pin 10 is CS amp A
// Pin 9 is  CS amp B
// These are defined in SPIconfig.h
// -----------------------------------------------------------------------------


//Intan's expected leading (MSB) for defining R/W to on chip registers:
// B is the binary formatter, see: https://www.arduino.cc/en/Reference/Byte
const byte WRITE_REG_CMD = B10000000;   // Intan RHD2132's prefix for write register command
const byte READ_REG_CMD  = B11000000;  // Intan RHD2132's prefix for read register command

const byte CALIBRATE_REG_CMD_MSBs  = B01010101;  // Intan RHD2132's  prefix for ADC self calibration, first 8 bits = 1 byte
const byte CALIBRATE_REG_CMD_LSBs  = B00000000;  // Intan RHD2132's  second 8 bits

const byte CONVERT_CHAN_CMD  = B00000000;  // Intan RHD2132's  prefix for convert channel command

extern uint16_t readRegister(byte thisRegister, int CSpin);
extern uint16_t writeRegister(byte thisRegister, byte thisValue, int CSpin);
extern uint16_t convertChannel(byte thisChannel, int CSpin);
extern void calibrateADC(int CSpin);   
