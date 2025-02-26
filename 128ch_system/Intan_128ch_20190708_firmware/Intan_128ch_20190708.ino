
/*
   128 channel Intsy system with:
   Teensy 3.6 + (up to) 4x Intan RHD2132 amplifier boards + ADXL335 accelerometer + onboard microSD logging capabiilty
   Developed by: Jon Erickson, Washington and Lee University, Dept of Physics and Engineering
   Contact: ericksonj@wlu.edu

  Revision History:

  JE 09 Jan 2020:   - temp change A18 to A21 for TTL direct measurement. A18 measures on-board flex sensor via voltage divider
  JE 08 July 2019:  - adding support for another analog input, intended to be flex sensor readings for Intsy system

  JE 24 June 2019:  - various updates complete to serial streaming selected subset of channels now complete
                    - enable measure Vdd on Intan amp A 
                    - add hello blink indicating serial ports are connected, LED blinks 10 times fast (100 ms on/off intervals)


  JE 04 June 2019:  - Adding (up to) 128 channel capabilty (amps C and D)
                    - Improved control of realtime data streaming for visualizating subsets of channels
                      Data is *always* logged to SD, unless log file is finished
                    
                    - Autodetect number of amplifiers
                    - Leaner/faster writing to SD (maybe later?)
                    - June 08 2019: update serial stream channels only at beginning of data frame.



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
#include "SetStreamChannels.h"     // select subset of channels to stream over serial for real time visualization

const uint16_t INTSY_MAGIC_NUM = 0x2121;  // 'magic' number used as sync bytes to verify Teensy/SD card function (this is !! in ASCII)
const byte TERM_CHAR = 4;          //termination character, used to sync with Labview R/W.  4 = EOT = end of transmission in ASCII

// JE 08 July 2019: now using this pin to measure flex sensor reading, no output of digital pulse start
//const int DIG_OUT_START_PULSE = 37;  // output digital pulses on pin 3 to signal start of data acq


IntervalTimer SamplingPeriodTimer;          // Create an IntervalTimer object
int SAMPLE_PERIOD_MICROS = DEFAULT_TS;     // value can be set with 'm' event below, defaults to 10000 us = 100 Hz per channel
int STREAM_SAMPLING_RATIO = 2;               // value can be set with 'm' event below.  default values will produce 50 Hz max serial stream rate.

bool DEBUG_VERBOSE = false;                //verbose debugging option, prints to serial

// ------------------------------------------
//  For autodetect-hardware config here.
//  Either Serial of Serial1 should be connected
// -------------------------------------------

Stream  *SERIALNAME;
//port will be set later according to which serial comm is live
byte initialByte;  // initial byte sent to one of the serial ports

bool SerialSetFlag = false; // serial port set?   initialize start false
// then turn true once we detect a byte at one of the ports

bool StreamToSerial = true; // flag to stream data to serial or not.  Used in loop() to stream data to serial (or not).




const int BAUD_RATE_UART_BT =  115200; // baud rate Serial1 port, this is set equal to RN-42 baud rate (Silver Mate from Sparkfun).  Have trialed 230400 baud rate on 01 May 2017 - did not work. 115200 is the max for now.
const int BAUD_RATE_USB =  9600; // baud rate Serial port, only affects UART, not USB which always runs at 12 MB/s, see: https://www.pjrc.com/teensy/td_serial.html

const int RTS_TEENSY = 25;  //for RTS/CTS hardware flow control pins for UART RN-42 bluetooth
const int CTS_TEENSY = 18;
const int RX_PIN_TEENSY = 27;  // set Rx/Tx pin numbers for UART RN-42 bluetooth comms (over Serial1)
const int TX_PIN_TEENSY = 26;

const int NUM_ADC_BITS_TEENSY = 16;  // Set resolution of Teensy analog reads.
// Note: Teensy3.6 has native 13 bit resolution.
// 16 bits (=2 bytes) is convenient for file format but doesn't offer any additional resolution



// ------------------------------------------
//  Declare some variables, used later. Voltatile is needed for interrupts to do A/D conversion with convert() cmd
// -------------------------------------------

const unsigned int SPI_PIPE_DELAY = 2; //Intan's results return 2 commands later in SPI command pipe.

int numAmpsVerified;                              // value returns for SPIgetAmplInfo.  Should be either integer multiple of 32x (or 0 if error).
// could use in future to determine which amps are connected



// byte adcbytes [NUM_BYTES_PER_SCAN];           // allocate memory to copy adcbytes for serial.write();
// JE 19 June 2017. this is declared below in loop().  faster to declare here so we don't have to keep allocating over and over?


unsigned int Navail;
unsigned int NumTx;

uint16_t throwAwayRead;  //place holders for 2 delay in SPI command: throw away data

int serialID;  // USB or UART serial port connected? 0 = Serial (USB); 1 = Serial1 (UART)



// -------------------------------------------------------------------------------
//  SETUP:  start serial port, and initialize and verify SPI interface only (JE 1 Dec 2016)
// -------------------------------------------------------------------------------

void setup() {

  Serial1.setRX(RX_PIN_TEENSY); // set alternate RX and TX pins
  Serial1.setTX(TX_PIN_TEENSY);
  // turn on both USB and UART serial ports for listening
  Serial.begin(BAUD_RATE_USB);
  Serial1.begin(BAUD_RATE_UART_BT);




  // JE 22 June 2019: Do NOT EVER attempt to wait for Serial.
  //When USB is not plugged in, it never navigates this connection
  //and we get stuck forever in the while loop.
  // It is perfectly fine to wait for Serial 1; even when Bt not paired, it still starts
  // connection and then we move on

  //  NEVER DO THIS-BAD BAD BAD JE
  //  while (!Serial) {
  //    ;
  //  }

  //OK to wait for serial 1 (bluetooth)
  while (!Serial1) {
    ;
  }

  // blink LED to incidate Serial connection setup is complete and ready to go
  for (int ii = 0; ii < 10; ii++) {
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH);
    delay(100);
    digitalWrite(LED_BUILTIN, LOW);
    delay(100);
  }

  Serial.println("Press any key to begin.");
  Serial1.println("Press any key to begin.");
  Serial1.attachRts(RTS_TEENSY); // for RTS/CTS flow control.  More robust BT link, if used
  Serial1.attachCts(CTS_TEENSY); // for robust BT link, if used


  // Set analogRead properties for  accelerometer readings
  analogReadResolution(NUM_ADC_BITS_TEENSY); //set resolution (number bits) for Teensy's ADC here
  analogReference(EXTERNAL);  //using AREF pin, Set analog ref to EXTERNAL before calling analogRead: https://learn.adafruit.com/adafruit-analog-accelerometer-breakouts/arduino-wiring
  pinMode(PIN_AIN_FLEX, INPUT); // set flex sensor pin to input (should be input by default?)

  // int waitCounter = 0;

  while (!SerialSetFlag) {
    if (DEBUG_VERBOSE) {
      Serial.println("USB is  waaaaaiiiiting..."); //print over usb
      Serial1.println("BT is  waaaaaiiiiting..."); //print over bt
      Serial.print("SerialSetFlag = ");
      Serial.println(SerialSetFlag);
    }
    /* wait for bytes to arrive on USB serial or UART serial
       if we get one, we'll set that port and turn off the other one
    */
    if (Serial.available() > 0) {
      // Serial.println(Serial.read());
      //Serial.println("Serial (USB) says hello world");
      //Serial.println("Set SERIALNAME = Serial (USB)");
      SERIALNAME = & Serial;
      SERIALNAME->println("USB connection verified");

      SERIALNAME->write(TERM_CHAR);


      //Serial1.println("USB connection is now connected. Turning off Serial1 (Bluetooth).");
      // Serial1.write(TERM_CHAR);
      //SERIALNAME->println(SERIALNAME->read());

      Serial1.end();  // turn off the UART serial. We don't need it anymore since we are using USB serial

      serialID = 0;
      SerialSetFlag = true; // this will get us out of the while loop. NEVER rest SerialSetFlag after this!
    }

    if (Serial1.available() > 0) {
      //Serial1.println(Serial1.read());

      SERIALNAME = & Serial1;
      SERIALNAME->println("UART/BT connection verified");
      SERIALNAME->write(TERM_CHAR);

      //Serial.println("Bluetooth connection is now connected. Turning off USB serial.");
      // Serial.write(TERM_CHAR);
      //SERIALNAME->println(SERIALNAME->read());

      Serial.end(); //turn off the USB serial. We don't need it anymore since we are using UART (bluetooth)

      serialID = 1;
      SerialSetFlag = true;
    }
    //delay(500);
    // Serial.println(waitCounter++);
  }



  //Do serialFlush and SPI init AFTER establishing which hardware port is connected

  // Flush serial read buffer again to control reading next bytes after user input prompts
  serialFlush();

  // Use 'b' command' to configure and begin SPI, JE 12 June 2019
  // configure and begin SPI transaction
  // SPI_init();
  // SPI1_init();


  // JE 08 July 2019 --commenting out, repurposing this pin for analog reads with flex sensor
  // set pulse start pin low upon power up or restart.  This will output pulses to signify begin/end of data acq sequence of scanADC isr
  // pinMode(DIG_OUT_START_PULSE , OUTPUT);
  // digitalWriteFast(DIG_OUT_START_PULSE , LOW);


  // Flush buffer again to control reading next bytes after user input prompts
  serialFlush();


}  //end setup()




// ------------------------------------
// Subroutine FinishLogFile(), closes SD card data file, blinks LED  with 1 s period to let user know logging is finished
void FinishLogFile(double TotalBytesWritten) {

  dataFile.truncate(TotalBytesWritten);
  dataFile.close();

  // blink light to signify logging complete
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



bool writeCircBuf(uint16_t samp) {
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
      CONVERT.A(48) --> ADC.A(30) result arrives
      aux_cmd.A(0)  --> ADC.A(31) result arrives
      aux_cmd.A(1)  --> ADC.A(48) result arrives (power supply voltage reading--amp A only!)

      CONVERT.B(0)  --> aux_cmd.B(0) result arrives (cmd from previous frame)
      CONVERT.B(1)  --> aux_cmd.B(1) result arrives (cmd from previous frame)
      CONVERT.B(2)  --> ADC.B(0) result arrives
      CONVERT.B(3)  --> ADC.B(1) result arrives
       ...
      CONVERT.B(30) --> ADC.B(28) result arrives
      CONVERT.B(31) --> ADC.B(29) result arrives
      aux_cmd.B(0)  --> ADC.B(30) result arrives
      aux_cmd.B(1)  --> ADC.B(31) result arrives
      ....

      Repeat for AMP C and AMP D
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
     // Result_aux_cmd_ampA[0] = 73; //testing serial and SD stream, for debug only!!!
     // Result_aux_cmd_ampA[1] = 78; //testing serial and SD stream, for debug only!!!

    }
    else { // (nA >= 2)
       dataArray_ampA[nA - SPI_PIPE_DELAY] = convertChannel(nA, CSpinA); //n-2 accounts for 2 command delay in SPI pipe
      
      //for debug only!
      //dataArray_ampA[nA - SPI_PIPE_DELAY] = nA + 0 + '0'; //n-2 accounts for 2 command delay in SPI pipe
    }
  }

  /*
        command sequence in round robin:
        CONVERT.A(48) --> ADC.A(30) result arrives
        aux_cmd.A(0)  --> ADC.A(31) result arrives
        aux_cmd.A(1)  --> ADC.A(48) result arrives (power supply voltage reading)
  */

  // Now call Vdd convert command +  2 auxiliary commands, into which ADC(30) and ADC(31) results are returned
  dataArray_ampA[30] = convertChannel(48, CSpinA); //voltage power supply reading; Intan RDH2000 datasheet pp32: The supply voltage of the chip (VDD) may be measured by sampling channel 48 of the ADC.  VDD(V) = 0.0000748 × result
  dataArray_ampA[31] = readRegister(40, CSpinA); //Returns 'I' from INTAN, if data link properly established, 2 SPI commands later
  Result_aux_cmd_ampA[2] = readRegister(41, CSpinA); //Returns 'N' from INTAN, if data link properly established



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

      //for debug only!!
      // dataArray_ampB[nB - SPI_PIPE_DELAY] = nB + 32 + '0'; //n-2 accounts for 2 command delay in SPI pipe
    }

  }

  // Now call 2 auxiliary commands, into which ADC(30) and ADC(31) results are returned
  dataArray_ampB[30] = readRegister(42, CSpinB); //Returns 'T' from INTAN, if data link properly established, 2 SPI commands later
  dataArray_ampB[31] = readRegister(43, CSpinB); //Returns 'A' from INTAN, if data link properly established


  // -------- A and B amplifer converts are now complete ! -------------------- //




  // ========  Amp C ===============
  // Loop over 32 calls to CONVERT(0), ..., CONVERT(31)
  for (byte nC = 0; nC < NUM_AMPS_PER_CHIP; nC++) {
    if (nC < SPI_PIPE_DELAY) {


      Result_aux_cmd_ampC[nC] = convertChannel_SPI1(nC, CSpinC); //this reads auxiliary data from commands sent last sampling period
      //Result_aux_cmd_ampC[0] = 73; //testing serial and SD stream, for debug only!!!
      //Result_aux_cmd_ampC[1] = 78; //testing serial and SD stream, for debug only!!!

    }
    else { // (nC >= 2)
      dataArray_ampC[nC - SPI_PIPE_DELAY] = convertChannel_SPI1(nC, CSpinC); //n-2 accounts for 2 command delay in SPI pipe
    }
  }

  // Now call 2 auxiliary commands, into which ADC(30) and ADC(31) results are returned
  dataArray_ampC[30] = readRegister_SPI1(40, CSpinC); //Returns 'I' from INTAN, if data link properly established, 2 SPI commands later
  dataArray_ampC[31] = readRegister_SPI1(41, CSpinC); //Returns 'N' from INTAN, if data link properly established



  // ========  Amp D ===============
  // Loop over 32 calls to CONVERT(0), ..., CONVERT(31)
  for (byte nD = 0; nD < NUM_AMPS_PER_CHIP; nD++) {
    if (nD < SPI_PIPE_DELAY) {

      Result_aux_cmd_ampD[nD] = convertChannel_SPI1(nD, CSpinD); //this reads auxiliary data from commands sent last sampling period
      //Result_aux_cmd_ampD[0] = 84; //testing serial and SD stream, for debug only!!!
      //Result_aux_cmd_ampD[1] = 65; //testing serial and SD stream, for debug only!!!


    }
    else { // (n >= 2)
      dataArray_ampD[nD - SPI_PIPE_DELAY] = convertChannel_SPI1(nD, CSpinD); //n-2 accounts for 2 command delay in SPI pipe
    }

  }

  // Now call 2 auxiliary commands, into which ADC(30) and ADC(31) results are returned
  dataArray_ampD[30] = readRegister_SPI1(42, CSpinD); //Returns 'T' from INTAN, if data link properly established, 2 SPI commands later
  dataArray_ampD[31] = readRegister_SPI1(43, CSpinD); //Returns 'A' from INTAN, if data link properly established


  // -------- C and D amplifer converts are now complete ! -------------------- //






  // ----- acquire accelerometer data acquired with analog reads ----------
  //acquire 16 bit resolution analog readings (0-3.3V maps to 0 to 2^16-1)
  accelX = analogRead(PIN_ACCEL_X);
  accelY = analogRead(PIN_ACCEL_Y);
  accelZ = analogRead(PIN_ACCEL_Z);
  
  Vflex  = analogRead(PIN_AIN_FLEX); // reads flex sensor voltage

  


  // -----------------------------------------------------------------------------//
  //       Implementing Circular Buffer
  //          1. Copy all newly acquired data bytes directly into circular buffer
  //          2. The order in which these are written determines the format of a data frame 
  // -----------------------------------------------------------------------------//



  // uint16_t adcbytes[NUM_BYTES_PER_SCAN / sizeof(uint16_t)]; // allocate memory to hold bytes returned from SPI pipe: timestamp + aux cmds +  adc bytes from Intan amps + ADXL accelerometer data

  // this is done for convenience in formatting data piped to serial and sd card.
  // the tradeoff is that an extra array copy is required, which typically costs about 50-100 us = 0.1 ms.

  // amp A: write aux cmd results
  //adcbytes[0] = Result_aux_cmd_ampA[0]; // ascii 'I', if data link is synced
  //adcbytes[1] = Result_aux_cmd_ampA[1]; // ascii 'N' if data link is synced

  writeCircBuf(Result_aux_cmd_ampA[0]);
  writeCircBuf(Result_aux_cmd_ampA[1]);


  // amp B: write aux cmd results
  // adcbytes[2] = Result_aux_cmd_ampB[0]; // ascii 'I', if data link is synced
  //adcbytes[3] = Result_aux_cmd_ampB[1]; // ascii 'N' if data link is synced

  writeCircBuf(Result_aux_cmd_ampB[0]);
  writeCircBuf(Result_aux_cmd_ampB[1]);


  writeCircBuf(Result_aux_cmd_ampC[0]);
  writeCircBuf(Result_aux_cmd_ampC[1]);

  writeCircBuf(Result_aux_cmd_ampD[0]);
  writeCircBuf(Result_aux_cmd_ampD[1]);



  // timestamp has 32 bits, convert into 16 bit ints using little endian format
  // adcbytes[4] =  timestamp & 0xFFFF; //use 0xFFFF and shift 16 to convert to two ints
  //  adcbytes[5] = (timestamp >> 16) & 0xFFFF;

  writeCircBuf(timestamp & 0xFFFF); //use 0xFFFF and shift 16 to convert to two ints
  writeCircBuf((timestamp >> 16) & 0xFFFF);

  /*
    for (unsigned int n = 0; n < NUM_AMPS_PER_CHIP; n++) {
      adcbytes[6 + n] = dataArray_ampA[n]; //6+ is offset, after aux cmd for A and B (to check data link) and 4-byte timestamp

    for (unsigned int n = 0; n < NUM_AMPS_PER_CHIP; n++) {
      adcbytes[38 + n] = dataArray_ampB[n]; //38+ is offset, following aux cmds for A and B (8 bytes) + timestamp (4 bytes) + ampA ADC data (64 bytes)
    }

    }
  */
  for (unsigned int n = 0; n < NUM_AMPS_PER_CHIP; n++) {
    writeCircBuf( dataArray_ampA[n] );
  }

  for (unsigned int n = 0; n < NUM_AMPS_PER_CHIP; n++) {
    writeCircBuf( dataArray_ampB[n] );
  }

  for (unsigned int n = 0; n < NUM_AMPS_PER_CHIP; n++) {
    writeCircBuf( dataArray_ampC[n] );
  }

  for (unsigned int n = 0; n < NUM_AMPS_PER_CHIP; n++) {
    writeCircBuf( dataArray_ampD[n] );
  }




  writeCircBuf( accelX );
  writeCircBuf( accelY );
  writeCircBuf( accelZ );
  writeCircBuf( Result_aux_cmd_ampA[2] ); //Vdd reading from amp A
  writeCircBuf( Vflex ); // flex sensor voltage, added JE 08 July 2019
  writeCircBuf( INTSY_MAGIC_NUM );



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


  bool dataReadyCopy; //if true, all data has been acquired, we have data in buffer ready to transmit over serial

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
      'f'   configure Intan RHD2132 filter settings
      'a'   ADC calibration for RHD2132
      'm'   set sampling rate, and set serial streaming rates
      'h'   select channels to stream from each amp
      's'   start serial data stream
      'p'   pause ADC conversions (ends SPI transactions)
      't'   pause serial streaming only (SPI transaction remains active)
      'u'   resume serial streaming
      'd'   initialize SD card, get file name suffix and fiile size (MB)
      'l'   finish SD logging
      'e'   toggle verbose debug mode
      'r'   reboot Teensy  (DO NOT USE --> doesn't work with Serial monitors or LV interfaces properly)

  */



  //checks if user sent any serial input
  if (SERIALNAME->available() > 0) {


    char inChar = SERIALNAME->read();

    if (DEBUG_VERBOSE) {
      SERIALNAME->print("input character received: ");
      SERIALNAME->println(inChar);
    }

    serialFlush(); //make sure serial buffer is flushed so we detect next user input properly

    // JE 12 Dec 2016: don't print this anymore for LV interface
    // SERIALNAME->print("Read char from serial input: ");
    // SERIALNAME->println(inChar);


    // Enumerate all possible modular actions here.  User send serial character to trigger these events.
    switch (inChar) {

      case 'b': // start SPI interface
        SPI_init();  // initializes 2x RHD2132 on SPI0, see SPIconfig.ino (SPI0)
        SPI1_init(); // initializes 2x RHD2132 on SPI1
        break;


      case 'v': // verify SPI interface
        SPI_verify();   // checks reg 40-44 for INTAN characters on A and B (SPI0)
        SPI1_verify();  // checks reg 40-44 for INTAN characters on C and D (SPI1)
        // tries to verify both A and B amp (64 chan setup)
        break;


      case 'i': // get amplifier info
        numAmpsVerified = SPI_getAmplInfo();   // SPI0 (amps A and B): get more info about amplifiers: number, chip revision, etc. SPI pins defined in SPIconfig.h
        numAmpsVerified = SPI1_getAmplInfo();  // SPI1 (amps C and D): get more info about amplifiers: number, chip revision, etc. SPI pins defined in SPIconfig.h
        break;


      case 'c':  // configure Intan RHD2000 registers 0-7 and 14-17
        ConfigureRegisters_0_17(); // see IntanDefaultConfig, configures both A and B amps on SPI0
        ConfigureRegisters_0_17_SPI1(); // see IntanDefaultConfig, configures both C and D amps on SPI1
        break;

      case 'f':  // configure Intan RHD2000 filters
        configFilterSettings(); //see FilterSettingDefaults.h and .ino, configures both A and B amps on SPI0
        // configFilterSettings_SPI1(); // configures both C and D amps on SPI1, do it all in one function JE 06 June 2019
        break;


      case 'a': // adc calibration cmd
        // A and B amps on SPI0
        SPI.beginTransaction(SPIsettingsFast);
        calibrateADC(CSpinA);      // see IntanCmd.h and .ino
        calibrateADC(CSpinB);      // calibrate both A and B amps
        SPI.endTransaction();

        // C and D amps on SPI1
        SPI1.beginTransaction(SPIsettingsFast);
        calibrateADC_SPI1(CSpinC);      // see IntanCmd.h and .ino
        calibrateADC_SPI1(CSpinD);      // calibrate both C and D amps
        SPI1.endTransaction();

        break;


      case 'm':  // compute and set sampling rate AND compute how max channels that can be streamed at this rate
        SAMPLE_PERIOD_MICROS = computeSamplingPeriod(); //see setSamplingPeriod.h and .ino
        myMaxChans = MaxChansForStream(); //computes max valid number of chans that can be streamed at a given sampling rate
        // serial output can be captured by LV to help determine which subset of requested channels are actually streamed (when bandwidth limited)
        //      SERIALNAME->print("myMaxChans set to: ");
        //      SERIALNAME->println(myMaxChans);
        // JE 05 June 2019: STEAM_SAMPLING_RATIO now set more smartly using ComputeMaxStreamRatio() function triggered via 'h' case below to set channels.
        // STREAM_SAMPLING_RATIO = computeSerialStreamRatio(SAMPLE_PERIOD_MICROS);
        // N_FRAME_WORDS = WORDS_PER_DATA_FRAME * STREAM_SAMPLING_RATIO;

        break;

      case 'h': //select channels to stream (amps, accel, Vdd)
        serialFlush(); //flush the serial read buffer
        NumStreamChansReq = StreamChansSelect(myMaxChans); //returns how mnay channels will be streaming
        // STREAM_SAMPLING_RATIO = ComputeMaxStreamRatio(NumStreamChansReq);
        // N_FRAME_WORDS = WORDS_PER_DATA_FRAME * STREAM_SAMPLING_RATIO;
        break;



      case 's':  // start data stream, turns on timer object

        if (DEBUG_VERBOSE) {
          SERIALNAME->write("Stream to Serial starting!");
          SERIALNAME->write(TERM_CHAR);
        }


        // configure the SPI bus.  This uses faster clock speed 12 MHz for converting channels. JE 01 May 2017
        SPI.beginTransaction(SPIsettingsFast);
        SPI1.beginTransaction(SPIsettingsFast);

        // output a digital pulse to signal start of data acq---commented out JE July 08 2019. Using pin 37 for analog reads instead
        // digPulseDataAcq(DIG_OUT_START_PULSE, 50, 2); // 100 ms long active high pulse (easy/clean to record with 512 Hz sampling rates).

        //start the interval timer, turns on interrupts for precisely timed sampling ADC
        SamplingPeriodTimer.begin(scanADC, SAMPLE_PERIOD_MICROS);
        SamplingPeriodTimer.priority(10);            // 0 is max priority, so 10 grants relatively  high priorty for this ISR.  JE 27 apr 2017.
        // SCB_SHPR3 = 0x20200000;  // Systick = priority 32 (defaults to zero, set lower priority here if needed, but prob not necessary. JE 10 July 2018)
        break;

      case 't': //stop streaming to serial
        // JE 31 Oct 2018, Write recognizeable block of characters to identify where we paused?  Safe to write to serial terminal, but do NOT write to SD card b/c we have to remove it from data stream later.
        SERIALNAME->write("Stream to Serial turning off now.");
        SERIALNAME->write(TERM_CHAR); // print termination character, to keep synced with LabView VISA R/W operations.
        StreamToSerial = false;  //value of boolean is checked further down in loop to determine whether to stream to serial. Note serial is not turned off, in case we'd like to reconnect
        break;

      case 'u': //resume streaming to serial
        // JE 30 Oct 2018, Write recognizeable block of characters to identify where we paused?  Make sure this write block is cleared before data logging! (writes to UART but never to SDfile)
        SERIALNAME->write("Stream to Serial (UART) turning on now");
        SERIALNAME->write(TERM_CHAR); // print termination character, to keep synced with LabView VISA R/W operations.
        StreamToSerial = true;  //value of boolean is checked further down in loop to determine whether to stream to serial. Note serial is not turned off, in case we'd like to reconnect
        break;

      case 'p':  // pause interval timer that controls ADC converts

        // turn off interval timer, end the SPI transaction, and output dig pulse to signal end of data stream.
        SamplingPeriodTimer.end();  //disable interrupts to convert and write ADC data
        SPI.endTransaction(); // end SPI0 transactions when we pause data stream.  SPI transactions always turn on with 's' command above JE 01 may 2017
        SPI1.endTransaction(); // end SPI0 transactions when we pause data stream.  SPI transactions always turn on with 's' command above JE 01 may 2017

// ---commented out JE July 08 2019. Using pin 37 for analog reads instead
       // digPulseDataAcq(DIG_OUT_START_PULSE , 100, 5);

        
        // SERIALNAME->println("Ending Interrupt Timer");
        // SERIALNAME->println("User paused data acq. Enter 's' to resume; or 'r' to soft-restart Teensy interface");

        SERIALNAME->write(TERM_CHAR); // print termination character, to keep synced with LabView VISA R/W operations.
        break;

      case 'o':  //probing which serial port is active, Serial (USB) or Serial1 (UART). print out little message to indicate which
        if (serialID == 0 ) {
          SERIALNAME->write("USB Serial connection."); // print termination character, to keep synced with LabView VISA R/W operations.
          SERIALNAME->write(TERM_CHAR); // print termination character, to keep synced with LabView VISA R/W operations.
        }
        else {
          SERIALNAME->write("UART/BT on Serial1 connection."); // print termination character, to keep synced with LabView VISA R/W operations.
          SERIALNAME->write(TERM_CHAR); // print termination character, to keep synced with LabView VISA R/W operations.
        }
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

      case 'e': //toggle verbose debug
        DEBUG_VERBOSE = !DEBUG_VERBOSE ;
        if (DEBUG_VERBOSE) {
          SERIALNAME->println("Verbose debug ON");
        }
        else {
          SERIALNAME->println("Verbose debug OFF");
        }
        SERIALNAME->write(TERM_CHAR); // print termination character, to keep synced with LabView VISA R/W operations.
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
  // When the buffer has sufficient amount of elements (currently set to 512 bytes), we'll read/de-queue them and then write to SD card and/or stream them to serial


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



  // If circular buffer contains sufficient number of elements, copy Nelements into an array and write it to SD and optionally the SerialBuffer (if we are streaming live)
  // Min typically set to 512 bytes = page size in most SD cards
  // Also write elements if a streaming channel change is pending and there is *any* data waiting in circular buffer to be processed, do so now (helps get ready/reset for new channel stream)
  if ((NumElementsInBuffer > MIN_BUFFER_BYTES_TO_WRITE) ||  (StreamChanChangePending && NumElementsInBuffer > 0)) {

    uint16_t bufCopy[NumElementsInBuffer];  //array to hold copy of contents


    // JE 14 June 2019: should only reset this back to 0 after we finish writing the data copied into SerialTxBuf
    // or when stream change change is requested/complete
    //CounterSerialTx = 0; //keeps track of index into which we write elements in SerialBuf to Tx.
    //Refreshes everytime we start processing elements in the circular buffer

    // Loop to copy elements from circular buffer, one at at time.
    for (int m = 0; m < NumElementsInBuffer; m++) {

      bufCopy[m] = readCircBuf(); // get data from circular buffer (make non-volatile copy)
      // bufCopy array is ultimately what gets written to SD card


      // Pending streaming channel update?
      // If so, lock in new channels ONLY when SerialBufIndx==0, indicating we are at beginning of frame
      // 08 June 2019 JE

      //for debug
      if (DEBUG_VERBOSE) {
        SERIALNAME->print("SerialBufIndx = ");
        SERIALNAME->println(SerialBufIndx);
      }

      if (StreamChanChangePending && SerialBufIndx == 0 ) {

        //copy pending bit mask into locked bit mask
        memcpy(BitsMaskLocked, StreamBitsMask, sizeof(BitsMaskLocked)); //copy pending bit mask into locked bit mask
        StreamChanChangePending = false;// clear the pending change flag
        CounterSerialTx = 0; //reset serial counter so that we'll start copying elements into first position of SerialTxBuf array, JE 14 June 2019


        // For syncing with LV data streaming, send out arbitrary 3 byte code (4,4,4) or (4,2,3)  and set as CHAN_CHANGE_TERM_CHARS in SetStreamChannels.h
        // Code is arbitrary, but choose something that is unlikely to occur by chance in actual channel stream.
        // Amplifiers rarely saturate low, and if they do they would go to 0, so 4 is a smart choice of value.
        // LV receives this characters are recognizes it is go time again to stream data from new stream channel array

        SERIALNAME->write( (uint8_t*)CHAN_CHANGE_TERM_CHARS, sizeof(CHAN_CHANGE_TERM_CHARS));


        //         debug serial output:
        if (DEBUG_VERBOSE) {
          SERIALNAME->println("Locked in New BitMask and cleared flag!!");
          for (int og = 0; og < WORDS_PER_DATA_FRAME; og++) {
            SERIALNAME->print(StreamBitsMask[og]);
            SERIALNAME->println(BitsMaskLocked[og]);

          }
        }

      } //end lock in new stream channel bit mask


      if (DEBUG_VERBOSE) {
        //for debug
        SERIALNAME->print("BitMaskLocked= ");
        SERIALNAME->println( BitsMaskLocked[SerialBufIndx]);
      }



      /*  PREPARE and WRITE SERIAL DATA STREAM
          Copy  data elements into Serial Tx buffer if we are streaming and we want to stream this particular channel
          Do NOT copy elements into SerialTxbuffer while channel change is pending.  Want to flush out the 'old channel' channel
          to get ready for the new.  This policy is set to be more easily compatible with LV - JE 14 June 2019
      */

      if ( (StreamToSerial && BitsMaskLocked[SerialBufIndx]) && !StreamChanChangePending ) {


        if (DEBUG_VERBOSE) {
          SERIALNAME->print("CounterSerialTx = ");
          SERIALNAME->println( CounterSerialTx );
        }

        SerialTxArray[CounterSerialTx] = bufCopy[m];
        CounterSerialTx++; //increment counter for each data element copied into serial stream buffer
      }

      // Update SerialBufIndx (index in circular array from which we are possibly taking element value to stream to serial)
      // Note SerialBufIndx is different than loop index m:
      // m resets to 0 everytime some data is copied from circular buffer; SerialBufIndx resets independently from m
      // and basically counts where we are inside a single data frame


      SerialBufIndx = ++SerialBufIndx % WORDS_PER_DATA_FRAME;



    }   //end loop copying data from circular buffer



    // ------------------------------------------------
    //  Write SerialTxArray to Serial port (BT or USB)
    // -----------------------------------------------
    // While not waiting for a channel change to take place ('normal' conditions), we'll wait for the serialTx array to accumulate a 'lot' of elements before writing
    //  Checking this condition helps ensure we don't keep asking for a large number of very small transfers---Serial write operations are expensive!

//    if ( !StreamChanChangePending && (CounterSerialTx >= SERIAL_MIN_BYTES_TX)) {
      
      if ( !StreamChanChangePending) { //don't worry about minimum number of serial bytes for now, just send when data is ready to ship
      
      
       SERIALNAME->write((uint8_t*)SerialTxArray, 2 * CounterSerialTx); // 2 bytes per word. CounterSerialTx always post-increments, giving us the correct number of bytes to transmit. JE 2018 Oct 26
      //Reset serial counter to start back filling elements beginning of serial buffer next time
      CounterSerialTx = 0;
    }


    /*  JE 14 June 2019: Policy decision here: If we are waiting to for new channel stream to lock in,
         we basically don't care about sending any more data from the old channel stream, so might as well
         not send anything at all, while we chan change is pending, hence commented out code block below


      if (StreamChanChangePending && CounterSerialTx > 0) {
      SERIALNAME->write((uint8_t*)SerialTxArray, 2 * CounterSerialTx); // 2 bytes per word. CounterSerialTx always post-increments, giving us the correct number of bytes to transmit. JE 2018 Oct 26
      SERIALNAME->flush(); // JE: should we actually wait for Tx buf to flush? 14 june 2019
      //Reset serial counter to start back filling elements beginning of serial buffer next time
      CounterSerialTx = 0;
      }
    */



    // ------------------------------------------------
    // Write data to SD card.
    // ALWAYS write data for all channels, all samples! (regardless of whether we are streaming for visualization)
    // ------------------------------------------------
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



    //  periodically flush SD card to update its FAT to prevent losing data on power (or other) failure
    if (flushSDElapsedTime >= SD_FLUSH_TIMER_MS) {
      dataFile.flush();  // flush data to commit SD buffer to physical memory locations.  Inefficient to do this everytime we write a block of data?
      // The risk is that could lose all data if we don't flush at regular intervals...could do this on longer intervals with a simple timer operation

      flushSDElapsedTime = 0; //reset timer, timing precision isn't critical
    }


    /* OLD STUFF commented out 06 June 2019
        //write Serial Stream stream, if serial streaming enabled.  We write either bufCopy or SerialTxArray serial port (UART) depending on Sampling Rate to Stream Rate ratio
        if (StreamToSerial) {
          if (STREAM_SAMPLING_RATIO <= 1) {  //Write all samples of bufCopy to Serial
            SERIALNAME->write((uint8_t*)bufCopy, sizeof(bufCopy)); // JE specify number of bytes? or number of elements?
          }
          else { //Else we're just writing a fraction of data chunks stored in SerialTxArray buffer


            SERIALNAME->write((uint8_t*)SerialTxArray, 2 * CounterSerialTx); // 2 bytes per word. CounterSerialTx always post-increments, giving us the correct number of bytes to transmit. JE 2018 Oct 26

          }

        } // if stream to serial

    */
  } // if there's enough elements in circular buffer, or if a stream channel change is pending

} //loop







