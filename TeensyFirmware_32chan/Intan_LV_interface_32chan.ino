
 /*
    Teensy 3.2 + Intan RHD2132 basic interface
    Developers: Jon Erickson, Jamie Hayes, Mauricio Bustamante.
    Contact: ericksonj@wlu.edu
    
  Revision History:
    
    JE 01 Dec 2016: -added more switch cases to trigger functions (like events in LV).
                    -added setSamplingPeriod to control interrupt interval
                    -updated filter controls to allow for Off-chip R_L selection
    JE 12 Dec 2016: -serial data flow changed to allow interface with LabView front end
    JE 14 Dec 2016: -choose serial port based on initial input character. This will help autodetect whether we are using USB serial or UART (intended for RN-42 bluetooth, in this case)
    JE 20 Dec 2016: - option to divide down SPI ClK in SPIconfig.ino.  This seems to help with incorrect reads from Reg 40-44 which showing data link can flake out
    JE 28 Apr 2017: - add RTS/CTS flow control for BT - more robust wireless link.
                    - set interrupt priority for intervaltimer controlling sampling to 0 (highest priority)
                   - writeRegister now returns uint_16 in IntanCmd
                   - explicitly reading out bits set in Reg 14-17 after config (to diagnose UART missing data issue).
   JE 01 May 2017: - more explicit local control of SPI.beginTransaction and endTransaction.
                   - add dig pulse output as start stop trigger signal. Data acq effectively begins on falling edge of pulse, stops on rising edge of stop pulse

   JE 02 May 2017: -fixed bug in FilterSettingsDefaults.  Solved the insidious bug of APWR[0-7] overwriting with indeterminate data, intermittently powering down amps 

   JE 17 May 2017: -fixed bug in setting IntanDefaultConfig where reg[0] ADC comparator select was set to 0 instead of 2.  Still measured sensible signals, not sure what the difference is setting to 2?
  
 General Info:
 SPI interface for communicating with Intan RHD2132 bioamplifier chip.
 Interrupts are used to convert ADC channels, aux cmd results, and timestamp at which conversion starts
 Serial data is written to port once data stream starts. Port can be either USB serial or UART/Bluetooth with CTS/RTS flow control enabled.


 
 Character inputs control triggering of events to validate SPI interface, config registers, set filter limits, set sampling period, start and stop data acquisition
 
 LabView frontend provides real-time visualization similar to Biosemi ActiveView system.  See: Intan_mainControlPanel.vi (also written by JE). Termination characters are
 sent to help sync LabView to the hardware.

 Additionally, 2 digital outputs are available on the Teensy, pins 21 and 22.  These may be used for digital triggers.  Currently, a short train of pulses is output on pin 22
 at the start and stop of data acquisition.  The ISR for converting ADC data turns on immediately after the falling edge of the last of 3 pulses output on pin 22.  Stopping
 the ISR for data acquisition is signaled for 5 short pulses (first rising edge signifies end of ADC conversion).

 This software is covered by the MIT license.  See below for details.

 Copyright <2017> <Jonathan C Erickson>

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction,
including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, 
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. 
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, 
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */



#include <string.h> // we'll need this for subString.  JE: For what in particular?  7/19 2015
#include <stdlib.h>
#include <SPI.h>

#include "IntanDefaultConfig.h"    // Define various 
#include "SPIconfig.h"             // Define SPI hardware pins on Teensy
#include "IntanCmd.h"              // Intan commands for read, write, calibrate, convert
#include "FilterSettingDefaults.h" // Define default Intan bandpass setting, as well as easy options
#include "setSamplingPeriod.h"     // Define sampling period for interval timer object (for Teensyduino interrupts)
#include "TeensyRestartCmd.h"      // JE don't need this since we can use _restart_Teensy() instead


const byte TERM_CHAR = 4; //termination character, used to sync with Labview R/W.  4 = EOT = end of transmission in ASCII
const int DIG_OUT_START_PULSE = 22;  // output digital pulses on pin 3 to signal start of data acq

/* JE: 07 Nov 2016
  Using IntervalTimer to control sampling period.  Using Metro() library is another option, but is more finnicky and only good down to about 4ms resolution, which isn't all that bad.
*/


IntervalTimer SamplingPeriodTimer;          // Create an IntervalTimer object

int SAMPLE_PERIOD_MICROS = DEFAULT_TS ;     // value can be set with 'm' event below, defaults to 10000 us = 100 Hz per channel




// ------------------------------------------
//  For autodetect-hardware config here.
//  Either Serial of Serial1 should be connected
// -------------------------------------------

Stream  *SERIALNAME;
//port will be set later according to which serial comm is live
byte initialByte;  // initial byte sent to one of the serial ports

boolean SerialSetFlag = false; // serial port set?   initialize start false
// then turn true once we detect a byte at one of the ports

const int BAUD_RATE_UART_BT =  115200; // baud rate Serial1 port, this is set equal to RN-42 baud rate (Silver Mate from Sparkfun).  Have trialed 230400 baud rate on 01 May 2017 - did not work. 115200 is the max for now.
const int BAUD_RATE_USB =  9600; // baud rate Serial port, only affects UART, not USB which always runs at 12 MB/s, see: https://www.pjrc.com/teensy/td_serial.html

const int RTS_TEENSY = 5;  //for RTS/CTS hardware flow control with UART RN-42 bluetooth
const int CTS_TEENSY = 20;

// ------------------------------------------
//  Declare variables used later. Voltatile is needed for interrupts to do A/D conversion with convert() cmd
// -------------------------------------------

const int NUM_AMPS =  32; //number of amplifiers with RHD2132 chip. Fancier/better would be to allocate this after reading number of amps from register on-board chip
const int NUM_AUX_CMD = 2;
const unsigned int NUM_BYTES_PER_SCAN = 2 * NUM_AUX_CMD + 4 + 2 * NUM_AMPS; //32 adc conversion data (bytes each) + 4 bytes for timestamp (32 bits) + 2 auxiliary cmd results
const unsigned int SPI_PIPE_DELAY = 2; //Intan's results return 2 commands later in SPI command pipe.

volatile uint16_t dataArray [NUM_AMPS];           // allocate memory space for each round-robin channel sampling of chans + aux commands + 1 timestamp
volatile boolean  dataAcqComplete = false;        // boolean that indicates when we are finished filling up the adc buffer, will trigger SERIALNAME transmit

volatile uint16_t Result_aux_cmd [NUM_AUX_CMD];   // allocate memory space for auxiliary commands included in each sampling period
volatile unsigned long timestamp;                 // for timestamps (32 bits)

// byte adcbytes [NUM_BYTES_PER_SCAN];           // allocate memory to copy adcbytes for serial.write();  
                                                  // this is declared below in loop(). 

unsigned int Navail;
unsigned int NumTx;

uint16_t throwAwayRead;  //place holders for 2 delay in SPI command: throw away data

int serialID;  // USB or UART serial port connected? 0 = Serial (USB); 1 = Serial1 (UART); added JE 27 Feb 2017



// -------------------------------------------------------------------------------
//  SETUP:  start serial port, and initialize and verify SPI interface only (JE 1 Dec 2016)
// -------------------------------------------------------------------------------

void setup() {

  // turn on both USB and UART serial ports for listening
  Serial.begin(BAUD_RATE_USB);
  Serial1.begin(BAUD_RATE_UART_BT);


  Serial.println("Press any key to begin.");
  Serial1.println("Press any key to begin.");
  Serial1.attachRts(RTS_TEENSY); // for RTS/CTS flow control.  More robust BT link, if used
  Serial1.attachCts(CTS_TEENSY); // for robust BT link, if used



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

}  //end setup()




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




// -------------------------------------------------------------------
//       Define interrupt routine here
//       Convert and write ADC data from all (N = 32) chans of RHD2132
// ------------------------------------------------------------------

// The interrupt will convert ADC data from all chans on RHD200 chip
// then write it to serial port

// functions called by IntervalTimer should be short, run as quickly as
// possible, and should avoid calling other functions if possible.

/* One data frame consists of the following. Two auxiliary commands are needed because each convert cmd generates
      result transmitted over MOSI two commands later (see Intan RHD2000 datasheet, page 15):

      CONVERT(0)  --> aux_cmd1 result arrives (cmd from previous frame)
      CONVERT(1) -->  aux_cmd2 result arrives (cmd from previous frame)
      CONVERT(2)  --> ADC(0) result arrives
      CONVERT(3)  --> ADC(1) result arrives
       ...
      CONVERT(30) --> ADC(28) result arrives
      CONVERT(31) --> ADC(29) result arrives
      aux_cmd1    --> ADC(30) result arrives
      aux_cmd2    --> ADC(31) result arrives
*/



/* One sampling period consists of 32 convert commands, followed by 2 auxilary commands
  Recall delay in SPI pipeline of 2 commands, hence the extra 2 aux commands at end of frame
*/

void scanADC(void) {

  timestamp = micros(); //time stamp in microseconds


  // Loop over 32 calls to CONVERT(0), ..., CONVERT(31)
  for (byte nnn = 0; nnn < NUM_AMPS; nnn++) {
    if (nnn < SPI_PIPE_DELAY) {

      Result_aux_cmd[nnn] = convertChannel(nnn); //this reads auxiliary data from commands sent last sampling period

    }
    else { // (n >= 2)
      dataArray[nnn - SPI_PIPE_DELAY] = convertChannel(nnn); //n-2 accounts for 2 command delay in SPI pipe

    }

  }

  // Now call 2 auxiliary commands, into which ADC(30) and ADC(31) results are returned
  dataArray[30] = readRegister(40); //Returns 'I' from INTAN, if data link properly established, 2 SPI commands later
  dataArray[31] = readRegister(41); //Returns 'N' from INTAN, if data link properly established


  dataAcqComplete = true;  //set flag to say we are finished converting data, this will trigger serial transmit in main loop()

}





// -------------------------------------
//       main loop
// ------------------------------------

void loop() {


  boolean dataReadyCopy; //if true, all data has been acquired, we have data in buffer ready to transmit over serial
  byte adcbytes [NUM_BYTES_PER_SCAN];   // allocate memory to hold bytes returned from SPI pipe: timestamp + adc bytes + aux cmds
  unsigned long timestampCopy;


  // ---------  COMMAND/EVENT SWITCHYARD   --------------
  /*
    get user commands for starting and stopping (b, v, i, c, m, f, a, s, p, r, q)
    start SPI interface = 'b';   verify SPI interface = 'v';
    get Intan amplifier info  = 'i';
    configure reg 0-17 = 'c';
    configure filter settings = 'f';
    set sampling rate = 'm'
    adc calibration = 'a'
    star data stream = 's'; pause = 'p'; reboot Teensy = 'r';
    unsigned long dw1 = micros();
  */

  // This block of code is critical: it is used to trigger various actions described above.
  // It is the way the Teensy communicates with the outside world.
  if (SERIALNAME->available() > 0) {
    //checks if user sent any serial input


    //SERIALNAME->println("Serial port input has been received from user.");

    char inChar = SERIALNAME->read();
    serialFlush(); //make sure serial buffer is flushed so we detect next user input properly

    // JE 12 Dec 2016: don't print this anymore for LV interface
    // SERIALNAME->print("Read char from serial input: ");
    // SERIALNAME->println(inChar);


    // Enumerate all possible modular actions here.  User send serial character to trigger these events.
    switch (inChar) {

      case 'b': // start SPI interface
        SPI_init();
        break;


      case 'v': // verify SPI interface
        SPI_verify();   // checks reg 40-44 for INTAN characters
        break;


      case 'i': // get amplifier info
        SPI_getAmplInfo();   // get more info about amplifiers: number, chip revision, etc. SPI pins defined in SPIconfig.h
        break;


      case 'c':  // configure Intan RHD2000 registers 0-7 and 14-17
        ConfigureRegisters_0_17(); // see IntanDefaultConfig
        break;

      case 'f':  // configure Intan RHD2000 filters
        configFilterSettings(); //see FilterSettingDefaults.h and .ino
        break;

      case 'm':  // compute and set sampling rate
        SAMPLE_PERIOD_MICROS = computeSamplingPeriod(); //see setSamplingPeriod.h and .ino
        break;

      case 'a': // adc calibration cmd
        SPI.beginTransaction(SPIsettingsConfig);
        calibrateADC();      // see IntanCmd.h and .ino
        SPI.endTransaction();
        break;

      case 's':  // start data stream, turns on timer object
        SERIALNAME->flush(); // wait for all data in Tx buffer to transmit

        // configure the SPI bus.  This uses faster clock speed 12 MHz for converting channels. JE 01 May 20176
        SPI.beginTransaction(SPIsettingsFast);

        // output a digital pulse to signal start of data acq
        // digPulseDataAcq(int digPin , int T_high, int Npulses);
        digPulseDataAcq(DIG_OUT_START_PULSE, 50, 2); // 100 ms long active high pulse (easy/clean to record with 512 Hz sampling rates). 
                                                       // Try single pulse for now.  Data acq effectively begins on falling edge of pulse

        //start the interval timer, turns on interrupts for precisely timed sampling ADC
        SamplingPeriodTimer.begin(scanADC, SAMPLE_PERIOD_MICROS);
        SamplingPeriodTimer.priority(10);            // 0 is max priority.  JE 27 apr 2017. 
        break;

      case 'p':  // pause data stream

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

      case 'q': //quit the program
        //any use for this?. Do we want to end SPI interface/transaction too? else
        break;

      case 'r':  // reboot teensy
        SERIALNAME->println("User requsted to reboot Teensy.  Rebooting in 2000 ms...");
        delay(2000);
        //_reboot_Teensyduino_();
        softRestart();  // defined in TeenyRestartCmd
        break;

      default:
        SERIALNAME->println("Sorry, this command not recognized. Use characters  v, i, m, f, c, a, s, p, o, r, q");
        SERIALNAME->write(TERM_CHAR); // print termination character, to keep synced with LabView VISA R/W operations.
        break;
    }



  } // end if Serial.available() looking for user input to trigger event on Teensy




  // JE copy contents of data buffer then write to serial; we can't use Serial.write() with volatile type.

  noInterrupts(); //disable interrupts so we can safely copy data from volatile type
  dataReadyCopy = dataAcqComplete;

  // check if we have data to copy
  if (dataReadyCopy == false) {
    interrupts();  // no data ready to copy and write adc data to serial, re-enable interrupts
  }


  else { // data is ready to copy and write to serial

    timestampCopy = timestamp;  //copy timestamp, interrupts are still off so value can't change while we are reading/copying variable.

    // Copy data into byte array that we'll write all at once with single call to serial.write()
    // JE: formatting akin to Intan native .rhd file, however we use 'IN' in place of magic number

    // write low then high byte since that's how it is stored in RAM
    adcbytes[0] = lowByte(Result_aux_cmd[0]); // ascii 'I', if data link is synced
    adcbytes[1] = highByte(Result_aux_cmd[0]);
    adcbytes[2] = lowByte(Result_aux_cmd[1]); // ascii 'N' if data link is synced
    adcbytes[3] = highByte(Result_aux_cmd[1]);

    // timestamp has 32 bits, convert into bytes using little endian format
    adcbytes[4] =  timestamp & 0xFF;
    adcbytes[5] = (timestamp >> 8) & 0xFF;
    adcbytes[6] = (timestamp >> 16) & 0xFF;
    adcbytes[7] = (timestamp >> 24) & 0xFF;

    // ADC data from each amp, bytes indexed [8] through [71] (= 64 bytes = 32*2 bytes)
    for (unsigned int n = 0; n < NUM_AMPS; n++) {
      adcbytes[8 + 2 * n] = lowByte(dataArray[n]); //8+ is offset
      adcbytes[8 + 2 * n + 1] = highByte(dataArray[n]);
    }


    dataAcqComplete = false; //reset boolean flag for next go-around

    NumTx = SERIALNAME->write(adcbytes, NUM_BYTES_PER_SCAN);  //serial.write(buf, len), where len  = num bytes

    interrupts(); // done copying data, re-enable interrupts.



    // Write data bytes to serial port once we have it copied (minimizing amount of time interrupts are off)
    // JE: IMPORTANT - should we do all serial writes while interrupts are turned off to make sure data buffer doesnt' get overwritten?
    // Or maybe implement circular buffer?  Need to revisit this eventually 16 Dec 2016
    // Can write() be interrupted? If so, we could get interrupt called while we are doing write(), then data would be overwritten by the time we finish.
    // If write() can't be interrupted, then we might as well write() before we turn on interrupts again.


  }


  /*
    // Write data bytes to serial port once we have it copied (minimizing amount of time interrupts are off)
    // JE: IMPORTANT - should we do all serial writes while interrupts are turned off to make sure data buffer doesnt' get overwritten?
    // Or maybe implement circular buffer?  Need to revisit this eventually 16 Dec 2016
    if (dataReadyCopy) {

      //write all bytes in single scan to serial port
      NumTx = SERIALNAME->write(adcbytes, NUM_BYTES_PER_SCAN);  //serial.write(buf, len), where len  = num bytes


    }
  */

} // end loop()











