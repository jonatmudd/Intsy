// -----------------------------------------------------------------------------
// DEFINE DEFAULT VALUES FOR WRITING INTAN RHD2132 REGISTERS
// These define various modes of operation with the chip, such as abs mode
// Note that WEAK_MISO can be set 0 if expanding to 64 channel system
// ** indicates user selectable
//
// Last modified:
//            -JE 04 June 2019, adding SPI1 for amps C and D (128ch) system
// -----------------------------------------------------------------------------

//configuration constants
const int NUM_AMPS_PER_CHIP =  32; //number of amplifiers with 2xRHD2132 chip. Fancier/better would be to allocate this after reading number of amps from register on-board chip
const int NUM_AUX_CMD = 2; // 2 aux commands x 2 chips.
const int NUM_INTAN_AMPS = 4; //hardcode for now, might make this variable/function return value later

const unsigned int NUM_BYTES_PER_SCAN = NUM_INTAN_AMPS * (2 * NUM_AUX_CMD  + 2 * NUM_AMPS_PER_CHIP) + 4 + 6 + 2 + 2; 
                  // Num Intan chips * 32 adc conversion data (2 bytes each)
                  //  + 2 auxiliary cmd results (2 bytes each) 
                  // + 4 bytes for 32 bit timestamp 
                  //  + 6 (accelerometer xyz)
                  // + 2 (Vbattery)
                  //  +1 sync word
                  // = 4*(64 + 4) + 4 + 6 + 2 + 2= 286 bytes



// extern function declaration
extern void ConfigureRegisters_0_17(); //amps A and B on SPI0
extern void ConfigureRegisters_0_17_SPI1(); // amps C and D on SPI1


// REG 0
const byte ADC_REF_BW = 3; // always set to 3 (see RHD2000 datasheet, page 19)
byte AMP_fast_settle = 0; //Set to 1 to enable fast settle function on chip power up, thenn set to 0 after that to resume normal function, see pg 19 and 24 of Intan RHD2000 datasheet
byte AMP_Vref_En = 1; // wait 100 us before ADC calibration; set to 0 to turn off this feature **
const byte ADC_comparator_bias = 3; // always set to 3 for normal operation
const byte ADC_comparator_select = 2; // always set to 2 for normal operation

// REG 1
byte ADC_BUFFER_BIAS = 32; // optimum current value. See page 27 **
byte VDD_SENSE_EN = 1; // set to 1 to enable Vdd supply measurement**

// REG 2
const byte MUX_BIAS = 40; // set to 40 for <120kS/s

// REG 3
const byte MUX_LOAD = 0; // always set to 0 for normal operation (page 20)
byte TEMP_S1 = 0; // temperature sense enable   **
byte TEMP_S2 = 0; // temperature sense enable   **
byte TEMP_EN = 0; // set to 0 to disable and save power ** 
byte DIGOUT_HiZ = 1; // set to 1 for high impedance mode. **
byte DIGOUT = 0; // value driven to Aux Dig Out, if DIGOUT_HiZ = 0, set to 0 for now  **

// REG 4
const byte WEAK_MISO = 0; // set to 0 for multiple chips sharing single MISO line, when CS pulled high, MISO goes into hi Z state. (Intan RHD2000 datasheet, pg 20)
const byte TWOSCOMP = 0; // for unsigned ADC
const byte ABSMODE = 0; // set to  0 for no absolute value function on ADC
byte DSPen = 0; // 0 to disable offset removal for DSP
byte DSP_cutoff = 0; // range = 0-15; does not matter if DSPen = 0

// REG 5 **
const byte Z_DAC_PWR = 0; // 0 for no impedance testing.
const byte Z_LOAD = 0; // always 0 for normal operation
const byte Z_SCALE = 0; // not important since no Z measurement done
const byte Z_CONN_ALL = 0; //0 in normal operation
const byte Z_SEL_POL = 0; // only used for for RHD2216 to select for sign
const byte Z_EN = 0; // set to 0 for basic recovery

// REG 6 **
const byte Z_DAC = 0; // set to 0V

// REG 7 **
const byte Zcheck_SEL = 0 ; // set to 0 because Z_EN = 0

// REG 14-17 (APWR)
byte apwrRegisterAllOn = 0b11111111;



