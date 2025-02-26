// ------------------------------------------------------------------------
//  Select Channel Subset to stream.
//  Reads in 5 unsigned ints from user, modifies globals <xx>Chans variable accordingly.
// User enters 5x 32bit unsigned integers (4x for amps A-D, last integer in order of LSB to MSB is ax, ay, az, Vdd, Vflex), 
//  Stream Bits Mask Format specified below. For now, turning off streaming of all but one aux_cmd, and no timestamp (to save bandwidth)
//  Max number of channels to stream is capped; additional channels requested beyond max will NOT be included in stream.
//  Stream bits are set in order below.
//
//  Last modified:
//          JE 05 June 2019 added SPI1 for amps C and D (128ch system)
//          JE 07 June 2019 remove serial debug
//          JE 17 June 2019: updating termination characters for syncing to LV comms
//                           added documentation for bit mask format
//          JE 08 July 2019: adding Vflex to bitmask
/* Stream Bits Mask Format Elements (144 total):

Two elements (4 bytes) are ALWAYS transmitted:
Mask[0] = 1; 'I' sync character from amp A, always set to 1
Mask[end] = 1; 0x2121 'magic number' to verify serial com synced (this is decimal 8481, ascii !!.  8481 is somewhat unlikely to appear randomly in 16-bit datastream centered around ~32000)

Optional elements to transmit (2 bytes each)
 
 StreamBitsMask[0-7] AuxCmd values returned, 2 each from each of 4 amps e.g. ('I', 'N'). By default ampA, aux[0] ALWAYS turned on, others off
 StreamBitsMask[8-9] are timestamp bytes(2 words = 4 bytes], by default turned off
 Mask[10+ 0-31]: amp A channels
 Mask[10+ 32-63] = amp B channels
 Mask[10+ 64-95] = amp C channels
 Mask [10+ 96-127] = amp D channels
 Mask[138-140] = accelerometer (ax,ay,az)
 Mask[141] = Vdd (battery voltage powering Intan amps ~ 3.3V)
 Mask[142] = Vflex (flex sensor analog voltage)
 Mask [143] = Intsy MagicNum (=0x2121, always on)
*/
// ------------------------------------------------------------------------




//---------
// Compute how many channels can be streamed at sampiling rate
// If the number of total requested channels is greater than the max allowed,
// requested list is trimmed down to max allowed
//--------
int MaxChansForStream() {

  float BW;

  //set available bandwidth depending on BT or USB connection
  if (serialID == 1) { // serialID=1 identifies bluetooth connected
    BW = BT_MAX_BW;
  }
  else { //USB connected
    BW = USB_MAX_BW;
  }

  //int NchansMaxUSB = constrain(round(0.5 * ((USB_MAX_BW * SAMPLE_PERIOD_MICROS / 1000000) - headerBytes)), 1, 128);
  // int NchansMaxBT = constrain(round(0.5 * ((BT_MAX_BW * SAMPLE_PERIOD_MICROS / 1000000) - headerBytes)), 1, 128);
  int NchansMax = constrain(round(0.5 * ((BW * SAMPLE_PERIOD_MICROS / 1000000) - headerBytes)), 1, 128);


  //inform user max chans that can be streamed
  SERIALNAME->println("Max Number of Chans Streaming at Fs:  ");
  SERIALNAME->write(TERM_CHAR);
  SERIALNAME->println(NchansMax);
  SERIALNAME->write(TERM_CHAR);



  return NchansMax;

}


//----------
//function to take user input as five 32-bit values and determine which stream channels are requested
// 4 values x32 for amps A-D
// 5th value for (ax, ay, ay, Vbat)
// Returns total number of stream bits sets (how many channels will be streamed)
//--------
byte StreamChansSelect(int MaxChansAllowed) {

  
  
  SERIALNAME->flush(); // flush the Tx buffer
  
  //SERIALNAME->println("Enter any char to continue"); 
  
// pause here until after Tx serial buffer is flushed   Idea is that we'll continue to read Labview VISA serial buffer
// until there are no bytes left, then pause here to sync R/W.
while (!SERIALNAME->available()) {
    }
 serialFlush(); //flush the serial read buffer (likely \n\r trasnmitted with read)


   
// JE 12 June 2019: Could flush Serial Tx queue here with SERIALNAME->flush();
// Idea would be to CLR host pc serial read buffer before we transmit a  
// term char after Txing all remaining data contents in Teensy serial out buffer.
// used to communicate to LV that a streaming channel change is pending
// Regardless, this just adds data to the host pc serial Rx queue. If we are looking for 
// a termination character---which could appear in data stream, with low probabilty--doesn't 
// have any practical effect: it doesn't change the odds that a character in the data stream 
// matches the termination character which would cause frame mis-reads on host pc
// To reduce odds of unlucky/unfortuante data stream character matches term char,
// one option is to send a sequence of something like 4,2,3 continually changing term char on LV
// thus making it very unlikely that this same sequence appears in data stream by pure chance

  StreamChanChangePending = true; // set to true, will be set back to false when we we are reading serial starting at beginning of new data frame
  byte NumChansRequested = 0; // return value


  uint32_t StreamChans[STREAM_CHAN_NUM_ELEMENTS]; //5 elements, first 4 for amps A-D; last for accel

  // for debug, helpful user message
  SERIALNAME->write("Enter 5x uint32 comma separated to set channels.");//  First four are for amps A-D, last one is 4 bits for accel and Vbat.");//, one bit for each of 32 amps on an Intan amp (max val = 2^32-1 = 4294967295 selects chans). Last is for ax, ay, az ");
  SERIALNAME->write(TERM_CHAR); // print termination character, to keep synced with LabView VISA R/W operations.


  for (int n = 0; n < STREAM_CHAN_NUM_ELEMENTS; n++) {
    // check if user input is available at serial port
    while (!SERIALNAME->available()) {
    }
    StreamChans[n] = SERIALNAME->parseInt();

    // Check each bit to see how many channels requested
    for (int k = 0; k < NUM_AMPS_PER_CHIP; k++) {
      //NumChansRequested += (StreamChans[n] >> k & 1);
      NumChansRequested += bitRead(StreamChans[n], k);
    }
  }

  serialFlush(); //flush the serial read buffer, in case user entered more uint32 than requested



  /* For debug:
    //print A chans requested
    SERIALNAME->print("AchansRequested = ");
    SERIALNAME->println(StreamChans[0], BIN);

    //print A chans requested
    SERIALNAME->print("BchansRequested = ");
    SERIALNAME->println(StreamChans[1], BIN);

    //print A chans requested
    SERIALNAME->print("CchansRequested = ");
    SERIALNAME->println(StreamChans[2], BIN);

    //print D chans requested
    SERIALNAME->print("DchansRequested = ");
    SERIALNAME->println(StreamChans[3], BIN);


    //print accel chans requested
    SERIALNAME->print("AccelchansRequested = ");
    SERIALNAME->println(StreamChans[4], BIN);

    //print total number chans requested
    SERIALNAME->print("NumchansRequested = ");
    SERIALNAME->println(NumChansRequested);

    SERIALNAME->write(TERM_CHAR); // print termination character, to keep synced with LabView VISA R/W operations.
  */
  //compute how many chans max can be streamed at Fstream = fs;
  //int NchansStreamMax = MaxChansForStream();
  int NchansStreamMax = MaxChansAllowed; //copy from input, probably a waste of an int, but not big deal, JE 07 2019

  //for debug:
  //  SERIALNAME->print("NchansStreamMax set equal to MaxChansAllowed: ");
  //  SERIALNAME->println(NchansStreamMax);

  // if too many channels are requested (bandwidth would be exceeded), trim the list to max number possible
  if (NumChansRequested > NchansStreamMax) {

    int chanCount = 0; //initialize

    //modify the list of channels to be streamed
    for (int n = 0; n < STREAM_CHAN_NUM_ELEMENTS; n++) {

      // Check each bit to see whether channels was requested for streaming
      // When channel count gets too high, we'll just force the remainder requested to be off
      for (int k = 0; k < NUM_AMPS_PER_CHIP; k++) {
        // chanCount += (StreamChans[n] >> k & 1);
        chanCount += bitRead(StreamChans[n], k);
        if (chanCount > NchansStreamMax) {
          //clear the bit to set chan streaming off
          bitClear(StreamChans[n], k);

          /* For debug:
            SERIALNAME->print("(Amp,Cleared Bit):  ");
            SERIALNAME->print(n);
            SERIALNAME->print(", ");
            SERIALNAME->println(k);
          */
        }
      }
    }

  }

  // Set streaming channels
  AAchans = StreamChans[0];
  BBchans = StreamChans[1];
  CCchans = StreamChans[2];
  DDchans = StreamChans[3];
  XLchans = StreamChans[4];

  // For debug:
//  SERIALNAME->println("Final Stream Chans Set");
//  SERIALNAME->println(AAchans);
//  SERIALNAME->println(BBchans);
//  SERIALNAME->println(CCchans);
//  SERIALNAME->println(DDchans);
//  SERIALNAME->println(XLchans);

  

  //Update array of boolean values specifying which indices we want to stream out of each data frame.
  StreamBitsMask[0] = 1;  //Streams single 73 'I' character as sync check with Intan chips
  // StreamBitsMask[1-7] are remaining AuxCmd Values returned, by default turned off  to maximize signals streaming
  // StreamBitsMask[8-9] are timestamp bytes(2 words = 4 bytes], by default turned off



  //loop over each element of StreamChans for Amps A-D
  for (int ii = 0; ii < 4; ii++) {

    //loop over each bit of Streamchans[ii]

    for (int jj = 0; jj < NUM_AMPS_PER_CHIP; jj++) { //4x 32-bit numbers to scan through to see which bits are set to 1

      StreamBitsMask[10 + jj + ii*NUM_AMPS_PER_CHIP] = bitRead(StreamChans[ii], jj);
    }
  }

  //See if we want to stream Accel or Battery voltage (ax, ay, az, Vdd, Vflex) bits of StreamChans[4]
  StreamBitsMask[WORDS_PER_DATA_FRAME - 6] = bitRead(StreamChans[4], 0); //ax (1)
  StreamBitsMask[WORDS_PER_DATA_FRAME - 5] = bitRead(StreamChans[4], 1); //ay (2)
  StreamBitsMask[WORDS_PER_DATA_FRAME - 4] = bitRead(StreamChans[4], 2); //az (4)
  StreamBitsMask[WORDS_PER_DATA_FRAME - 3] = bitRead(StreamChans[4], 3); //Vdd (8) (power supply to intan chips)
  StreamBitsMask[WORDS_PER_DATA_FRAME - 2] = bitRead(StreamChans[4], 4); //Vflex (16) (flex sensor output)

  StreamBitsMask[WORDS_PER_DATA_FRAME - 1] = 1; //Always streams Intsy magic num, used as sync check

  /*
    // For debug
      SERIALNAME->println("StreamBitsMask Elements: ");
      for (int p = 0; p < WORDS_PER_DATA_FRAME; p++) {
        SERIALNAME->println(StreamBitsMask[p]);
      }
  */



  StreamChanChangePending = true; //set flag (global) to true to indicate a chan change is pending
                                  // will clear (reset) when we have locked in this channel request


  byte NchansInStream = min(NumChansRequested, NchansStreamMax);
  SERIALNAME->print("Nstreamchan = "); 
  SERIALNAME->println(NchansInStream); 
 // SERIALNAME->write(NchansInStream); //write number of channels to be streamed, LV could read as penultimate character before receiving term char
                                     // LV should know anyway, because it knows number of max streaming channels from main control panel
                                     // and it knows number of channels requested, so just take the min of those two numbers 
  SERIALNAME->write(TERM_CHAR); //use to sync with LV comms
  //return min(NumChansRequested, NchansStreamMax ); //NumChansRequested;
  return NchansInStream;

}





int ComputeMaxStreamRatio(byte Nchans) {
  float NbytesTx = headerBytes + 2 * Nchans; // how many bytes to send with each data frame (sample); Note: 2*Nchans because bytes transmitted for every channel requested
  float BWrequested = (NbytesTx / SAMPLE_PERIOD_MICROS) * 1000000; // bandwidth requested (bytes/s)

  /* For debug
    SERIALNAME->print("NbytesTx = ");
    SERIALNAME->println(NbytesTx, 6);

    SERIALNAME->print("BWrequested (Bytes/s) = ");
    SERIALNAME->println(BWrequested, 6);
  */
  uint32_t DT_STREAM;
  int BWreqRatio;
  float ratio;
  //compare requested bandwidth to available bandwidth for serial streaming, depends on BT or USB



  if (serialID == 1) { // serialID=1 identifies bluetooth connected
    BWreqRatio = ceil(BWrequested / BT_MAX_BW); // this is the requested BW to available bandwidth ratio.  If ratio <=1, we can stream all samples.  If not, have to downsample by integer multiple of sampling period
    ratio = BWrequested / BT_MAX_BW;
  }
  else { //USB connected
    BWreqRatio = ceil(BWrequested / USB_MAX_BW); // this is the requested BW to available bandwidth ratio.  If ratio <=1, we can stream all samples.  If not, have to downsample by integer multiple of sampling period
    ratio = BWrequested / USB_MAX_BW;
  }

  /* For debug
    SERIALNAME->print("BWratio = ");
    SERIALNAME->println(ratio, 6);
  */
  //check if bandwidht is actually available
  if (BWreqRatio <= 1) { //can stream all samples, Fstream = fs equivalently, DTstream = DTsample
    DT_STREAM = SAMPLE_PERIOD_MICROS;
  }

  else { //BW requetsed too high, have to choose either Fstream = fs/m, m = {1,2,3,....} to satisfy BWactual <= BT_MAX_BW, or decreae number of channels streamed.
    // 05 June 2019: policy decision to make here.  For now, just decrease the stream rate (increase streaming period)
    DT_STREAM = BWreqRatio * SAMPLE_PERIOD_MICROS;

  }

  /*for debug, print out useful info
    SERIALNAME->print("BWratio (integer): ");
    SERIALNAME->println(BWreqRatio);

    SERIALNAME->print("DT_STREAM set to (us): ");
    SERIALNAME->println(DT_STREAM);
    SERIALNAME->write(TERM_CHAR);
  */
  // return DT_STREAM; //streaming period in microseconds
  return BWreqRatio; //this will take the place of SampleRateSerialRatio computed in setSamplingPeriod.ino JE 05 June 2019
}

