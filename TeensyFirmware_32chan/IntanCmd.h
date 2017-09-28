// -----------------------------------------------------------------------------
// INTAN COMMAND PREFIXES for Read, Write, Calibrate and ADC conversion of chans
// -----------------------------------------------------------------------------


//Intan's expected leading (MSB) for defining R/W to on chip registers:
// JE 7/19/2015: B is the binary formatter, see: https://www.arduino.cc/en/Reference/Byte
const byte WRITE_REG_CMD = B10000000;   // Intan RHD2132's prefix for write register command
const byte READ_REG_CMD  = B11000000;  // Intan RHD2132's prefix for read register command

const byte CALIBRATE_REG_CMD_MSBs  = B01010101;  // Intan RHD2132's  prefix for ADC self calibration, first 8 bits = 1 byte
const byte CALIBRATE_REG_CMD_LSBs  = B00000000;  // Intan RHD2132's  second 8 bits

const byte CONVERT_CHAN_CMD  = B00000000;  // Intan RHD2132's  prefix for convert channel command

extern void calibrateADC();
extern uint16_t writeRegister(byte thisRegister, byte thisValue);
extern uint16_t readRegister(byte thisRegister);
extern uint16_t convertChannel(byte thisChannel);
