//0's or 1's to designate channel as selected (1) or not (0) for streaming for real time visualization
unsigned int AAchans = 0; // A amps, specifies all amps off (no amp data streaming until requested)
unsigned int BBchans = 0; // B amps
unsigned int CCchans = 0; // C amps
unsigned int DDchans = 0; // D amps
unsigned int XLchans = 0; // 3x accel, Vdd, Vflex

const byte STREAM_CHAN_NUM_ELEMENTS = 5;  //use 5 elements to specify which channels to stream
byte CHAN_CHANGE_TERM_CHARS[3] = {4, 4, 4}; //for syncing LV comms
int myMaxChans = 0;

//byte NchansStream = 32; //32 chans streaming by default
byte NumStreamChansReq = 0; //number of channels requested for streaming, 32 by default
//intMaxChansForStream = -1; //JE don't do this...doesn't work as global, don't understand why "initialize to -1 to indicate this hasn't been set yet.  Call MaxChansForStream() functino to compute this"
bool StreamBitsMask[WORDS_PER_DATA_FRAME] = {0}; //initialize all channels to be off (none streaming)...these are requested from user, but do not lock in until we are reading at the first word of a data frame
bool BitsMaskLocked[WORDS_PER_DATA_FRAME] = {0}; // we'll copy the requested bit mask into this LOCKED bit mask only when SerialBufReadIndx indicates we are at beginning of data frame

     // Initial config of locked-in bit mask set for serial stream should include sync characters (4 bytes total).
// This ensures there is always some characters requested for streaming to serial. JE 20 Juen 2019
//BitsMaskLocked[0] = true; // Always stream first sync character from Intan 'I' = 73 in ascii.
//BitsMaskLocked[WORDS_PER_DATA_FRAME - 1] = true; //Always streams Intsy magic num, used as sync check

bool StreamChanChangePending = false;   // flag to determine if a channel change is pending as we complete a data frame from previous selection
// maybe implement this feature in LV instead? 07 June 2019
// else we need to make a copy of bitsmask before updating, then lock in the new mask after the frame read is complete

// define max bandwidth streaming over BT or USB
const float BT_MAX_BW = 115200 / 10 * 0.6; // 115200 baud ~ 115200 bits/s ~ 11520 bytes of info/s. also multiply an empirical fudge factor...can't normally achieve max streaming rate, but 70% is realistic
const float USB_MAX_BW = 12 * 1024 * 1024 * 0.80; // 12 MB/s, 0.80 is fudge factor saying we can get about 75% max of actual USB speed, conservative estiamte

const byte headerBytes = 4; // =2 Intan aux cmd (e.g., 73) + 2 sync bytes (0x2121= magicNum), need header bytes to compute max number of chans that can be streamed


//bool FrameWriteComplete = false; //have we finished aggregating data for all subset chans moving through circBuf?


// reads in 4 unsigned ints from user, modifies globals <xx>Chans variable accordingly.
extern byte StreamChansSelect(int MaxChansAllowed); //returns the total number of channels set for streaming
extern int MaxChansForStream(); // returns the max number of channels that can be streamed at full data rate
//extern int ComputeMaxStreamRatio(byte Nchans); //returns ratio between streaming and sampling periods



