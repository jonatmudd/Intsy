
/*

   Ambulatory 64 channel system with:
   Teensy 3.6 + 2 x Intan RHD2132 basic interface + ADXL335 accelerometer + onboard microSD logging capabiilty
   Developed by: Jon Erickson, Washington and Lee University, Dept of Physics and Engineering
   Contact: ericksonj@wlu.edu

  Revision History:

  JE 16 Dec 2018:   - Significant updates to SD logging complete.  Using pre-allocated, pre-erased file to minimize SD write latency.
                      SD logging requires downloading Bill Greiman's SdFat libarary for Teensy: https://github.com/greiman/SdFat)
                      (Massive thanks to Bill for making this library freely available!)
                      Sd logging tests completed using fs = 100 Hz and 500 Hz; data sync and SD writes were observed to have 100% fidelity (no missing bits, no miswrites)
                      Recommend using modern/high-end SD card for minimizing latency. Tests in the Erickson lab were done using modern Samsung Evo Plus 32 GB SD card (10 bucks from BestBuy)
                    - Streaming to UART serial max streaming rate is enforced to 50 Hz x 64 chans or 100 Hz x 32 chan.  These correspond to max Bluetooth 2.0 module bandwidth.
                      Subsampled data can be streamed to serial for viewing purposes.
                        Future development may allow selecting individual channels with high streaming rates.
                    - Duration of Recording/Logging (hrs) is a required user input
                    - Note: some pin assingments have changed for SPI and UART rts/cts. These have been updated appropriately
                    - This firmware is ostensibly for 64 channel device, but can be easily modified to 32 chan operation.
                       Future development may allow auto-detection of Intan RHD2000 device present/plugged into hardware port


  JE 10 July 2018:  - started development, based on Intan_LV_interface_64chan_alpha. Some pin assignments have changed moving to T3.6
                    - Major updates/upgrades include:
                      1. Streaming to microSD card (should make this optional, but for now always turned on, since this is intended for ambulatory system)
                      2. ADXL335  analaog  accelerometer measurements (x,y,z) - useful for determining motion/resting periods of test subject
                      3. Circular buffer implementation to improve robustness of ADC conversion timing in interval timer interrupts

  JE 24 Oct 2018:  -capability to stream to serial and to Sd file at different rates (has to be integer multiples).  For example, stream data to SD at 100 Hz, but stream data to PC to check signal quality at (max) 50 Hz.
                   - add 'check in' capability - serial stream can be turned on or off at will


  General Info:
  SPI interface for communicating with 2x Intan RHD2132 bioamplifier chip.
  Interrupts are used to convert ADC channels, aux cmd results, and timestamp at which conversion starts
  Serial data is written to Serial port (USB or UART) once data stream starts. Intan data streams are also written to SD card, along with accelerometer readings.
  Serial Port can be either USB serial or UART/Bluetooth with CTS/RTS flow control enabled.


  Character inputs control triggering of events to validate SPI interface, config registers, set filter limits, set sampling period, start and stop data acquisition, etc.

  Companion LabView front-end provides real-time visualization similar to Biosemi ActiveView system.
  See: Intan_mainControlPanel_64chan_ambulatory_v02.vi (also written by JE and availble on github site).
  Termination characters are sent to help sync LabView to the Teensy.

  Additionally, 2 digital outputs are available on the Teensy, pins 21 and 22.  These may be used for digital triggers.  Currently, a short train of pulses is output on pin 22
  at the start and stop of data acquisition.  The ISR for converting ADC data turns on immediately after the falling edge of the last of 3 pulses output on pin 22.  Stopping
  the ISR for data acquisition is signaled for 5 short pulses (first rising edge signifies end of ADC conversion).

  This software is covered by the MIT license.  See below for details.

  Copyright (c) 2017 - 2018  Jonathan C Erickson

  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction,
  including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so,
  subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/




#include <stdlib.h>
#include <SPI.h>
#include <SdFat.h>  //requires Bill Greiman's sdFat library (downloaded July 2018 from: https://github.com/greiman/SdFat)

#include "IntanDefaultConfig.h"    // Define various 
#include "SPIconfig.h"             // Define SPI hardware pins on Teensy
#include "IntanCmd.h"              // Intan commands for read, write, calibrate, convert
#include "FilterSettingDefaults.h" // Define default Intan bandpass setting, as well as easy options
#include "setSamplingPeriod.h"     // Define sampling period for interval timer object (for Teensyduino interrupts)
#include "TeensyRestartCmd.h"      // JE don't need this since we can use _restart_Teensy() instead
#include "AccelerometerConfig.h"    // Define acceleromter pins and variables that store readings
#include "CircularBufferConfig.h"  // Define circular buffer. This is crucial element to control data flow
#include "SDwrite.h"               // setting up SD card for writing data



const byte TERM_CHAR = 4;          //termination character, used to sync with Labview R/W.  4 = EOT = end of transmission in ASCII
const int DIG_OUT_START_PULSE = 22;  // output digital pulses on pin 3 to signal start of data acq



IntervalTimer SamplingPeriodTimer;          // Create an IntervalTimer object
int SAMPLE_PERIOD_MICROS = DEFAULT_TS ;     // value can be set with 'm' event below, defaults to 10000 us = 100 Hz per channel
int STREAM_SAMPLING_RATIO = 2;               // value can be set with 'm' event below.  default values will produce 50 Hz max serial stream rate.



// ------------------------------------------
//  For autodetect-hardware config here.
//  Either Serial of Serial1 should be connected
// -------------------------------------------

Stream  *SERIALNAME;
//port will be set later according to which serial comm is live
byte initialByte;  // initial byte sent to one of the serial ports

boolean SerialSetFlag = false; // serial port set?   initialize start false
// then turn true once we detect a byte at one of the ports

boolean StreamToSerial = true; // flag to stream data to serial or not.  Used in loop() to stream data to serial (or not).

const int BAUD_RATE_UART_BT =  115200; // baud rate Serial1 port, this is set equal to RN-42 baud rate (Silver Mate from Sparkfun).  Have trialed 230400 baud rate on 01 May 2017 - did not work. 115200 is the max for now.
const int BAUD_RATE_USB =  9600; // baud rate Serial port, only affects UART, not USB which always runs at 12 MB/s, see: https://www.pjrc.com/teensy/td_serial.html

const int RTS_TEENSY = 2;  //for RTS/CTS hardware flow control with UART RN-42 bluetooth
const int CTS_TEENSY = 18;





// ------------------------------------------
//  Declare some variables, used later. Voltatile is needed for interrupts to do A/D conversion with convert() cmd
// -------------------------------------------

const unsigned int SPI_PIPE_DELAY = 2; //Intan's results return 2 commands later in SPI command pipe.

int numAmpsVerified;                              // value returns for SPIgetAmplInfo.  Should be either 32 or 64 (or 0 if error).
// could use in future to determine which of these 3 possibilities



// byte adcbytes [NUM_BYTES_PER_SCAN];           // allocate memory to copy adcbytes for serial.write();
// JE 19 June 2017. this is declared below in loop().  faster to declare here so we don't have to keep allocating over and over?


unsigned int Navail;
unsigned int NumTx;

uint16_t throwAwayRead;  //place holders for 2 delay in SPI command: throw away data

int serialID;  // USB or UART serial port connected? 0 = Serial (USB); 1 = Serial1 (UART); added JE 27 Feb 2017



// -------------------------------------------------------------------------------
//  SETUP:  start serial port, and initialize and verify SPI interface only (JE 1 Dec 2016)
// -------------------------------------------------------------------------------

void setup() {

  Serial1.setRX(27); // set alternate RX and TX pins
  Serial1.setTX(26);
  // turn on both USB and UART serial ports for listening
  Serial.begin(BAUD_RATE_USB);
  Serial1.begin(BAUD_RATE_UART_BT);


  Serial.println("Press any key to begin.");
  Serial1.println("Press any key to begin.");
  Serial1.attachRts(RTS_TEENSY); // for RTS/CTS flow control.  More robust BT link, if used
  Serial1.attachCts(CTS_TEENSY); // for robust BT link, if used

  analogReadResolution(16); //enter bits of resolution for Teensy's ADC here
  // Teensy3.6 has native 13 bit resolution.
  // 16 bits just pads with extra zeros and makes it easy to divide into low and high byte


  while (!SerialSetFlag) {

    /* wait for bytes to arrive on USB serial or UART serial
       if we get one, we'll set that port and turn off the other one
    */
    if (Serial.available() > 0) {
      //Serial.println("Serial (USB) says hello world");
      //Serial.println("Set SERIALNAME = Serial (USB)");
      SERIALNAME = & Serial;
      SERIALNAME->println("USB connection verified");
      SERIALNAME->write(TERM_CHAR);
      //SERIALNAME->println(SERIALNAME->read());

      Serial1.end();  // turn off the UART serial. We don't need it anymore since we are using USB serial

      serialID = 0;
      SerialSetFlag = true; // this will get us out of the while loop. NEVER rest SerialSetFlag after this!
    }

    if (Serial1.available() > 0) {
      //Serial1.println("Serial1 (UART) says hello world");
      //Serial1.println("Set SERIALNAME = Serial1 (UART)");
      SERIALNAME = & Serial1;
      SERIALNAME->println("UART connection verified");
      SERIALNAME->write(TERM_CHAR);
      //SERIALNAME->println(SERIALNAME->read());

      Serial.end(); //turn off the USB serial. We don't need it anymore since we are using UART (bluetooth)

      serialID = 1;
      SerialSetFlag = true;
    }

  }

  //Do serialFlush and SPI init AFTER establishing which hardware port is connected

  // Flush buffer again to control reading next bytes after user input prompts
  serialFlush();


  // configure and begin SPI transaction
  SPI_init();

  // set pulse start pin low upon power up or restart.  This will output pulses to signify begin/end of data acq sequence of scanADC isr
  pinMode(DIG_OUT_START_PULSE , OUTPUT);
  digitalWriteFast(DIG_OUT_START_PULSE , LOW);   // turn the LED on (HIGH is the voltage level)




  // -------------------------------------------
  // Set up SD card for writing: JE 10 July 2018
  // Moved to SDwrite.ino 11 July 2018
  // ------------------------------------------
  //  if (!sdEx.begin()) {
  //    sdEx.initErrorHalt("SdFatSdioEX begin() failed");
  //  }
  //  else { // SD memory card initiailzed; now open a file
  //    sdEx.chvol(); // make sdEx the current volume.
  //    Serial.println("SD card initialized.");
  //  }
  //  char* fname = "jetest012.bin";
  //  Serial.print("Opening data file: ");
  //  Serial.println(fname);
  //  if (!dataFile.open(fname, O_RDWR | O_CREAT)) {
  //    sdEx.errorHalt("open failed");
  //    //delay(1000);
  //  }
  //  else {
  //    Serial.println("dataFile open for business!");
  //    delay(100);
  //  }


  // Flush buffer again to control reading next bytes after user input prompts
  serialFlush();


}  //end setup()




// ------------------------------------
// Subroutine FinishLogFile(), closes SD card data file, blinks LED to let user know logging is finished
void FinishLogFile(double TotalBytesWritten) {

  dataFile.truncate(TotalBytesWritten);
  dataFile.close();

  // blink light to signify logginc complete
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
  delay(500);
  digitalWrite(LED_BUILTIN, LOW);
  delay(500);

}


// ----------------------------------------------------------------
//    Subroutine: FLUSH SERIAL INPUT--Little helper function to help control user input, not INTAN related per se
// ---------------------------------------------------------------
void serialFlush() {

  while (SERIALNAME->available() > 0) {
    char chuckaway = SERIALNAME->read();
  }

}


// ----------------------------------------------------------------
//    Subroutine: pulse the digital pin as signal of start and stop acq
// ---------------------------------------------------------------

void digPulseDataAcq(int digPin , int T_high, int Npulses) {
  //send out series of N brief digital pulses as starting acq signal. T_high time pulse is high (50% duty cycle)
  for (int m = 0; m < Npulses; m++) {
    pinMode(digPin, OUTPUT);
    digitalWriteFast(digPin, HIGH);   // turn the LED on (HIGH is the voltage level)
    delay(T_high);               // wait for a second
    digitalWriteFast(digPin, LOW);    // turn the LED off by making the voltage LOW
    if (m < Npulses - 1) { // don't delay on last pulse, start right away on data acq
      delay(T_high);
    }
  }

}



// ----------------------------------------------------------------
//   Circular buffer subfunctions for reading and writing value
//   These are interrupt safe for single producer-consumer architecture
//   Data is produced in scanADC() ISR and consumed in loop()
//   See: https://www.downtowndougbrown.com/2013/01/microcontrollers-interrupt-safe-ring-buffers/
//
// ---------------------------------------------------------------


uint16_t readCircBuf(void) {
  int val = dataBuffer[tail];
  tail = (tail + 1) % BUFSIZE;
  return val;
} //Note we are not checking if there is any data in the buffer in the first place
// Best practice is that we check if there is data prior to calling this



boolean writeCircBuf(uint16_t samp) {
  int next_head = (head + 1) % BUFSIZE;
  if (next_head != tail) {
    /* there is room */
    dataBuffer[head] = samp;
    head = next_head;
    return true;
  } else {
    /* no room left in the buffer */
    return false;
  }
}


//Note, no protection or warning against overwrites
// PJRC adc interrupts example.
// uint16_t readSampleFromBuf(void)
//{
//  int  h, t;
//  uint16_t val;
//
//  /*
//    do {
//      h = head;
//      t = tail;                   // wait for data in databuffer
//    } while (h == t);
//  */
//  // if(h==t){return null;}  //no data in buffer to retrieve, return empty
//
//  t = tail;
//  if (++t >= BUFSIZE) t = 0;
//  val = dataBuffer[t];                // remove 1 sample from databuffer
//  tail = t;                           // update tail position
//  return val;
//}



// -------------------------------------------------------------------
//       DEFINE INTERRUPT SERVICE ROUTINE HERE
//       Convert and write ADC data from all (N = 64) chans of 2x RHD2132
//       And call auxilary commands as part of round robin sampling
//       And get acceleromter data (added 10 July 2018, JE)
//       and get timestamp (microseconds)
//
//
// ------------------------------------------------------------------

// The interrupt will convert ADC data from all chans on RHD2000 chips;
// get auxilary data commands (data sync check); read accelerometers;
// and get microsecond timestamp, then store it to circular buffer.
// The circular buffer stores data until it can be sent to serial port and SD card,
// which happens in loop()

// functions called by IntervalTimer should be short, run as quickly as
// possible, and should avoid calling other functions if possible.

/* One data frame consists of the following. Two auxiliary commands are needed because each convert cmd generates
      result transmitted over MOSI two commands later (see Intan RHD2000 datasheet, page 15):

    CURRENT COMMAND       DATA RETURNED
    ---------------       -------------
      CONVERT.A(0)  --> aux_cmd.A(0) result arrives (cmd from previous frame)
      CONVERT.A(1)  --> aux_cmd.A(1) result arrives (cmd from previous frame)
      CONVERT.A(2)  --> ADC.A(0) result arrives
      CONVERT.A(3)  --> ADC.A(1) result arrives
       ...
      CONVERT.A(30) --> ADC.A(28) result arrives
      CONVERT.A(31) --> ADC.A(29) result arrives
      aux_cmd.A(0)  --> ADC.A(30) result arrives
      aux_cmd.A(1)  --> ADC.A(31) result arrives

      CONVERT.B(0)  --> aux_cmd.B(0) result arrives (cmd from previous frame)
      CONVERT.B(1)  --> aux_cmd.B(1) result arrives (cmd from previous frame)
      CONVERT.B(2)  --> ADC.B(0) result arrives
      CONVERT.B(3)  --> ADC.B(1) result arrives
       ...
      CONVERT.B(30) --> ADC.B(28) result arrives
      CONVERT.B(31) --> ADC.B(29) result arrives
      aux_cmd.B(0)  --> ADC.B(30) result arrives
      aux_cmd.B(1)  --> ADC.B(31) result arrives
*/



/* One sampling period consists of 32 convert commands, followed by 2 auxilary commands
  Recall delay in SPI pipeline of 2 commands, hence the extra 2 aux commands at end of frame


  Note that results of aux cmds sent to A and B return in the 'opposite' end of the list.
  For example, the aux cmds sent to B will return 2 SPI commands later when we convert ampA chan[0-1].
  Similarly, when we call aux cmds for ampA, then return 2 commands later when we convert ampA chan[0-1]

  Also note when scan is triggered, 3x analog reads are done for accelerometer data (x,y,z)
  Lastly, 1 sync bit is written to check data stream fidelity

*/

void scanADC(void) {




  //time stamp in microseconds at start of conversions
  timestamp = micros();


  // ========  Amp A ===============
  // Loop over 32 calls to CONVERT(0), ..., CONVERT(31)
  for (byte nA = 0; nA < NUM_AMPS_PER_CHIP; nA++) {
    if (nA < SPI_PIPE_DELAY) {


      Result_aux_cmd_ampA[nA] = convertChannel(nA, CSpinA); //this reads auxiliary data from commands sent last sampling period
      //Result_aux_cmd_ampA[0] = 73; //testing serial and SD stream, for debug only!!!
      //Result_aux_cmd_ampA[1] = 78; //testing serial and SD stream, for debug only!!!

    }
    else { // (nA >= 2)
      dataArray_ampA[nA - SPI_PIPE_DELAY] = convertChannel(nA, CSpinA); //n-2 accounts for 2 command delay in SPI pipe
    }
  }

  // Now call 2 auxiliary commands, into which ADC(30) and ADC(31) results are returned
  dataArray_ampA[30] = readRegister(40, CSpinA); //Returns 'I' from INTAN, if data link properly established, 2 SPI commands later
  dataArray_ampA[31] = readRegister(41, CSpinA); //Returns 'N' from INTAN, if data link properly established



  // ========  Amp B ===============
  // Loop over 32 calls to CONVERT(0), ..., CONVERT(31)
  for (byte nB = 0; nB < NUM_AMPS_PER_CHIP; nB++) {
    if (nB < SPI_PIPE_DELAY) {

      Result_aux_cmd_ampB[nB] = convertChannel(nB, CSpinB); //this reads auxiliary data from commands sent last sampling period
      //Result_aux_cmd_ampB[0] = 84; //testing serial and SD stream, for debug only!!!
      //Result_aux_cmd_ampB[1] = 65; //testing serial and SD stream, for debug only!!!


    }
    else { // (n >= 2)
      dataArray_ampB[nB - SPI_PIPE_DELAY] = convertChannel(nB, CSpinB); //n-2 accounts for 2 command delay in SPI pipe
    }

  }

  // Now call 2 auxiliary commands, into which ADC(30) and ADC(31) results are returned
  dataArray_ampB[30] = readRegister(42, CSpinB); //Returns 'T' from INTAN, if data link properly established, 2 SPI commands later
  dataArray_ampB[31] = readRegister(43, CSpinB); //Returns 'A' from INTAN, if data link properly established


  // -------- A and B amplifer converts are now complete ! -------------------- //

  // ----- acquire accelerometer data acquired with analog reads ----------
  //acquire 16 bit resolution analog readings (0-3.3V maps to 0 to 2^16-1)
  accelX = analogRead(PIN_ACCEL_X);
  accelY = analogRead(PIN_ACCEL_Y);
  accelZ = analogRead(PIN_ACCEL_Z);





  // -----------------------------------------------------------------------------//
  //       Implementing Circular Buffer
  //          1. Reformat data into formatted byte stream, easy to parse leter
  //          2. Copy all newly acqruied data bits into circular buffer
  // -----------------------------------------------------------------------------//



  uint16_t adcbytes[NUM_BYTES_PER_SCAN / sizeof(uint16_t)]; // allocate memory to hold bytes returned from SPI pipe: timestamp + aux cmds +  adc bytes from Intan amps + ADXL accelerometer data
  // this is done for convenience in formatting data piped to serial and sd card.
  // the tradeoff is that an extra array copy is required, which typically costs about 50-100 us = 0.1 ms.

  // amp A: write aux cmd results
  adcbytes[0] = Result_aux_cmd_ampA[0]; // ascii 'I', if data link is synced
  adcbytes[1] = Result_aux_cmd_ampA[1]; // ascii 'N' if data link is synced


  // amp B: write aux cmd results
  adcbytes[2] = Result_aux_cmd_ampB[0]; // ascii 'I', if data link is synced
  adcbytes[3] = Result_aux_cmd_ampB[1]; // ascii 'N' if data link is synced


  // timestamp has 32 bits, convert into 16 bit ints using little endian format
  adcbytes[4] =  timestamp & 0xFFFF; //use 0xFFFF and shift 16 to convert to two ints
  adcbytes[5] = (timestamp >> 16) & 0xFFFF;



  for (unsigned int n = 0; n < NUM_AMPS_PER_CHIP; n++) {
    adcbytes[6 + n] = dataArray_ampA[n]; //6+ is offset, after aux cmd for A and B (to check data link) and 4-byte timestamp
  }

  for (unsigned int n = 0; n < NUM_AMPS_PER_CHIP; n++) {
    adcbytes[38 + n] = dataArray_ampB[n]; //38+ is offset, following aux cmds for A and B (8 bytes) + timestamp (4 bytes) + ampA ADC data (64 bytes)
  }

  adcbytes[70] = accelX;
  adcbytes[71] = accelY;
  adcbytes[72] = accelZ;
  adcbytes[73] = 22379; //sync bits to check for checking fidelity of SD writes




  // Add contents of adcbytes to circular buffer.
  for (int jj = 0; jj < sizeof(adcbytes) / sizeof(uint16_t); jj++) {
    writeCircBuf(adcbytes[jj]);
  }

  // JE 11 July 2018: One consideration for faster speed would be to use writeCircBuf above as each SPI result is returned.
  // It's really only for clarity in the code that all bytes are arranged in adcbytes, but there's nothing stopping us
  // from writing the result directly to the circular buffer.  In other words, adcbytes is just 'middleman' here.
  // For now, with intended modest sampling rates, we probably won't run into issues.  Copying array contents takes but a few us



  //set flag to say we are finished converting data, this will trigger serial transmit in main loop()
  dataAcqComplete = true;

}








// ---------------------------------------------------------------------------------------
//       main loop()
//  
//  Checks to see if user wants to initiate any action
//  Handles checking circular buffer and writing any contents to SD and/or stream to serial
// ---------------------------------------------------------------------------------------

void loop() {


  boolean dataReadyCopy; //if true, all data has been acquired, we have data in buffer ready to transmit over serial

  // we declare this above as global. is that ok? 19 June 2017
  unsigned long timestampCopy;

  //keep track of how many bytes written to SD card
  static double TotalNumBytesWrittenToSD = 0;
  static int NbytesWrite;


  /*
    %% ---------  COMMAND/EVENT SWITCHYARD   --------------%%
     This block of code is critical: it is used to trigger various actions described below.
     It is the way the Teensy communicates with the outside world.

      User Interaction Command Initiation:
      'o'   probe which serial interface is active (USB or UART/BT)
      'b'   start SPI interface
      'v'   verify SPI interface
      'i'   get Intan amplifier info
      'c'   configure Intan RHD2132 reg 0-17
      'f'   configure INtan RHD2132 filter settings
      'a'   ADC calibration for RHD2132
      'm'   set sampling rate, and set serial streaming rates
      's'   start serial data stream
      'p'   pause ADC conversions (ends SPI transactions)
      't'   pause serial streaming only (SPI transaction remains active)
      'u'   resume serial streaming
      'd'   initialize SD card, get file name suffix and fiile size (MB)
      'l'   finish SD logging
      'r'   reboot Teensy  (DO NOT USE --> doesn't work with Serial monitors or LV interfaces properly)

  */



  //checks if user sent any serial input
  if (SERIALNAME->available() > 0) {


    char inChar = SERIALNAME->read();
    serialFlush(); //make sure serial buffer is flushed so we detect next user input properly

    // JE 12 Dec 2016: don't print this anymore for LV interface
    // SERIALNAME->print("Read char from serial input: ");
    // SERIALNAME->println(inChar);


    // Enumerate all possible modular actions here.  User send serial character to trigger these events.
    switch (inChar) {

      case 'b': // start SPI interface
        SPI_init(); // this tries to initialize 2x RHD2132, see SPIconfig.ino
        break;


      case 'v': // verify SPI interface
        SPI_verify();   // checks reg 40-44 for INTAN characters
        // tries to verify both A and B amp (64 chan setup)
        break;


      case 'i': // get amplifier info
        numAmpsVerified = SPI_getAmplInfo();   // get more info about amplifiers: number, chip revision, etc. SPI pins defined in SPIconfig.h
        break;


      case 'c':  // configure Intan RHD2000 registers 0-7 and 14-17
        ConfigureRegisters_0_17(); // see IntanDefaultConfig, configures both A and B amps
        break;

      case 'f':  // configure Intan RHD2000 filters
        configFilterSettings(); //see FilterSettingDefaults.h and .ino, configures both A and B amps
        break;

      case 'm':  // compute and set sampling rate
        SAMPLE_PERIOD_MICROS = computeSamplingPeriod(); //see setSamplingPeriod.h and .ino
        STREAM_SAMPLING_RATIO = computeSerialStreamRatio(SAMPLE_PERIOD_MICROS);
        N_FRAME_WORDS = WORDS_PER_DATA_FRAME * STREAM_SAMPLING_RATIO;
        break;

      case 'a': // adc calibration cmd
        SPI.beginTransaction(SPIsettingsFast);
        calibrateADC(CSpinA);      // see IntanCmd.h and .ino
        calibrateADC(CSpinB);      // configure both A and B amps
        SPI.endTransaction();
        break;

      case 's':  // start data stream, turns on timer object
        SERIALNAME->flush(); // wait for all data in Tx buffer to transmit

        // configure the SPI bus.  This uses faster clock speed 12 MHz for converting channels. JE 01 May 2017
        SPI.beginTransaction(SPIsettingsFast);

        // output a digital pulse to signal start of data acq
        // digPulseDataAcq(int digPin , int T_high, int Npulses);
        digPulseDataAcq(DIG_OUT_START_PULSE, 50, 2); // 100 ms long active high pulse (easy/clean to record with 512 Hz sampling rates).
        // Try single pulse for now.  Data acq effectively begins on falling edge of pulse

        //start the interval timer, turns on interrupts for precisely timed sampling ADC
        SamplingPeriodTimer.begin(scanADC, SAMPLE_PERIOD_MICROS);
        SamplingPeriodTimer.priority(10);            // 0 is max priority, so 10 grants relatively  high priorty for this ISR.  JE 27 apr 2017.
        // SCB_SHPR3 = 0x20200000;  // Systick = priority 32 (defaults to zero, set lower priority here if needed, but prob not necessary. JE 10 July 2018)
        break;

      case 'p':  // pause interval timer that controls ADC converts

        // turn off interval timer, end the SPI transaction, and output dig pulse to signal end of data stream.
        SamplingPeriodTimer.end();  //disable interrupts to convert and write ADC data
        SPI.endTransaction(); // end SPI transactions when we pause data stream.  SPI transactions always turn on with 's' command above JE 01 may 2017
        digPulseDataAcq(DIG_OUT_START_PULSE , 100, 5);

        //DATA_ACQ = false;  // this will trigger channel conversion to stop
        // SERIALNAME->println("Ending Interrupt Timer");
        // SERIALNAME->println("User paused data acq. Enter 's' to resume; or 'r' to soft-restart Teensy interface");

        SERIALNAME->write(TERM_CHAR); // print termination character, to keep synced with LabView VISA R/W operations.
        break;

      case 'o':  //probing which serial port is active, Serial (USB) or Serial1 (UART). print out little message to indicate which
        if (serialID == 0 ) {
          SERIALNAME->write("USB Serial"); // print termination character, to keep synced with LabView VISA R/W operations.
          SERIALNAME->write(TERM_CHAR); // print termination character, to keep synced with LabView VISA R/W operations.
        }
        else {
          SERIALNAME->write("UART Serial1"); // print termination character, to keep synced with LabView VISA R/W operations.
          SERIALNAME->write(TERM_CHAR); // print termination character, to keep synced with LabView VISA R/W operations.
        }
        break;

      case 't': //stop streaming to serial
        // JE 31 Oct 2018, Write recognizeable block of characters to identify where we paused?  Safe to write to serial terminal, but do NOT write to SD card b/c we have to remove it from data stream later.
        SERIALNAME->write("Stream to Serial (UART) turning off now.");
        SERIALNAME->write(TERM_CHAR); // print termination character, to keep synced with LabView VISA R/W operations.
        StreamToSerial = false;  //value of boolean is checked further down in loop to determine whether to stream to serial. Note serial is not turned off, in case we'd like to reconnect
        break;

      case 'u': //resume streaming to serial
        // JE 30 Oct 2018, Write recognizeable block of characters to identify where we paused?  Make sure this write block is cleared before data logging! (writes to UART but never to SDfile)
        SERIALNAME->write("Stream to Serial (UART) turning on now");
        SERIALNAME->write(TERM_CHAR); // print termination character, to keep synced with LabView VISA R/W operations.
        StreamToSerial = true;  //value of boolean is checked further down in loop to determine whether to stream to serial. Note serial is not turned off, in case we'd like to reconnect
        break;

      case 'd': // configure SD card and open file to write
        SDcardInitStatus = initSDcard();   // initialize SDfat volume
        // JE 2018 Nov 20: try opening preallocated, pre-erased file for lower latency in writes
        //SDcardFileOpenStatus = SDcardOpenFile(); // open data file, user prompted to enter name
        SDcardFileOpenStatus = SDcardCreateLogFile();
        break;

      case 'l': //close SD log file, JE 07 Dec 2018
        FinishLogFile(TotalNumBytesWrittenToSD);
        break;

      case 'r':  // reboot teensy
        SERIALNAME->println("User requsted to reboot Teensy.  Rebooting in 2000 ms...");
        delay(2000);
        //_reboot_Teensyduino_();
        softRestart();  // defined in TeenyRestartCmd
        break;

      default:
        SERIALNAME->println("Sorry, this command not recognized. Use characters: o, b, v, i, c, f, a, m, d, s, p, t, u, l");
        SERIALNAME->write(TERM_CHAR); // print termination character, to keep synced with LabView VISA R/W operations.
        break;
    }



  } // end if Serial.available() looking for user input to trigger event on Teensy


  else {
    SysCall::yield(); // could perhaps speed up loop by using invere logic--check if no data is available, if so yield.

  }





  // Each time through the loop, check contents of circular buffer.
  // When the buffer has sufficient amount of elements (currently set to 512 bytes), we'll pop them then write to SD card and/or stream them to serial


  //Turn off interrupts so we can read head and tail current position and figure out if there is data in the circular buffer that can be written to sd card and/or serial
  noInterrupts();
  int curr_head, curr_tail, NumElementsInBuffer;
  //float ts = micros(); //measure how much time interrupts are disabled
  curr_head = head;
  curr_tail = tail;

  interrupts(); //re-enable interrupts




  // compute how many elements currently in data buffer
  if (curr_head == curr_tail) {
    NumElementsInBuffer =  0;
  }
  else if (curr_head > curr_tail) {
    NumElementsInBuffer =  curr_head - curr_tail;
  }
  else {
    NumElementsInBuffer =  BUFSIZE - abs(curr_tail - curr_head);
  }



  // if buffer contains sufficient number of elements, Copy Nelements into an array and write it to SD and/or Serial
  if (NumElementsInBuffer > MIN_BUFFER_BYTES_TO_WRITE) { // JE 06 Dec 2018, set to 512 to force larger block to be written to SD

    uint16_t bufCopy[NumElementsInBuffer];
    CounterSerialTx = 0; //reset serial counter each time we copy data buffer contents into serialTx contents

    // Loop to copy elements from circular buffer, one at at time.
    for (int m = 0; m < NumElementsInBuffer; m++) {

      bufCopy[m] = readCircBuf(); //get data from circular buffer (make non-volatile copy)


      //Copy chunks of data into Serial Tx buffer.  How much depends on Sampling Rate/Serial Tx Rate ratio (must be integer)
      if (StreamToSerial && STREAM_SAMPLING_RATIO > 1) {


        if (CopyToSerialTx) { //CopyToSerialTx == true;
          SerialTxArray[CounterSerialTx] = bufCopy[m];
          CounterSerialTx++; //increment counter for each data element read

        }

        RingCountSerialTx++;




        if ( (RingCountSerialTx % N_FRAME_WORDS) >= WORDS_PER_DATA_FRAME)  {
          CopyToSerialTx = false; //
        }

        else if (RingCountSerialTx == N_FRAME_WORDS) {
          RingCountSerialTx = 0; //reset RingCountSerialTx
          CopyToSerialTx = true; //reset CopytoSerialTx = true
        }



      } //end checking whether we are writing subsampled data to serial

    }   //end loop copying data from circular buffer



    // Write data to SD card.
    if (TotalNumBytesWrittenToSD + NbytesWrite <= log_file_size) { //check to make sure there is sufficient space to write bytes
      NbytesWrite = dataFile.write((uint8_t*)bufCopy, sizeof(bufCopy));
      TotalNumBytesWrittenToSD += NbytesWrite;

      //If we don't write all bytes requested, turn on LED as a warning light
      if (NbytesWrite != sizeof(bufCopy)) {
        pinMode(LED_BUILTIN, OUTPUT);
        digitalWrite(LED_BUILTIN, HIGH);
      }
    }
    else { //no more space
      FinishLogFile(TotalNumBytesWrittenToSD);
    }



    //  periodically flush SD card to update its FAT: addedJE 20 Nov 2018
    if (flushSDElapsedTime >= SD_FLUSH_TIMER_MS) {
      dataFile.flush();  // flush data to commit SD buffer to physical memory locations.  Inefficient to do this everytime we write a block of data?
                         // The risk is that could lose all data if we don't flush at regular intervals...could do this on longer intervals with a simple timer operation

      flushSDElapsedTime = 0; //reset timer, timing precision isn't critical
    }


    //write Serial Stream stream, if serial streaming enabled.  We write either bufCopy or SerialTxArray serial port (UART) depending on Sampling Rate to Stream Rate ratio
    if (StreamToSerial) {
      if (STREAM_SAMPLING_RATIO <= 1) {  //Write all samples of bufCopy to Serial
        SERIALNAME->write((uint8_t*)bufCopy, sizeof(bufCopy)); // JE specify number of bytes? or number of elements?
      }
      else { //Else we're just writing a fraction of data chunks stored in SerialTxArray buffer


        SERIALNAME->write((uint8_t*)SerialTxArray, 2 * CounterSerialTx); // 2 bytes per word. CounterSerialTx always post-increments, giving us the correct number of bytes to transmit. JE 2018 Oct 26

      }

    } // if stream to serial
  } // if there's enough elements in buffer

} //loop







