
// Ciruclar Buffer Implementation
// This just declares all of the data structures involved to keep main function a bit cleaner
// Can also think about using circular buffer library for implementing e.g.: https://github.com/tonton81/Circular_Buffer
// However, not sure if this is interrupt safe.
// This one seems to be: https://github.com/rlogiacco/CircularBuffer
//
// Last modified: JE 04 June 2019, adding amps C and D


const int BUFSIZE = 65536; // Teensy data read buffer size. At ~ 300 bytes/scan x 100 scans/s sampling rate, we have a 30 kB/s stream, so this size buffers nearly 1 seconds, which seems generous enough?
static volatile uint16_t dataBuffer[BUFSIZE]; //circular array buffer, store 16 bit ints since Intan returns 16 bit numbers, and so does accelerometer. Use volatile for shared variables
static volatile int head, tail;

volatile bool  dataAcqComplete = false;        // boolean that indicates when we are finished filling up the adc buffer, will trigger SERIALNAME transmit

volatile unsigned long timestamp;                 // for timestamps (32 bits)


// SPI 0 amps A and B
volatile uint16_t dataArray_ampA [NUM_AMPS_PER_CHIP];      // amp A: allocate memory space for each round-robin channel sampling of chans
volatile uint16_t dataArray_ampB [NUM_AMPS_PER_CHIP];      // amp B: allocate memory space for each round-robin channel sampling of chans 

volatile uint16_t Result_aux_cmd_ampA [NUM_AUX_CMD+1];   // allocate memory space for auxiliary commands included in each sampling period  + 1 Vdd reading (amp A only!)
volatile uint16_t Result_aux_cmd_ampB [NUM_AUX_CMD];   // allocate memory space for auxiliary commands included in each sampling period


// SPI 1 amps C and D
volatile uint16_t dataArray_ampC [NUM_AMPS_PER_CHIP];      // amp A: allocate memory space for each round-robin channel sampling of chans 
volatile uint16_t dataArray_ampD [NUM_AMPS_PER_CHIP];      // amp B: allocate memory space for each round-robin channel sampling of chans 

volatile uint16_t Result_aux_cmd_ampC [NUM_AUX_CMD];   // allocate memory space for auxiliary commands included in each sampling period
volatile uint16_t Result_aux_cmd_ampD [NUM_AUX_CMD];   // allocate memory space for auxiliary commands included in each sampling period



// Serial Tx data buffer; Serial write data rate can be integer multiple less than sampling rate
const int WORDS_PER_DATA_FRAME =  144;         // 144 words = 128 (ADC amps A, B, C, D)
                                               //             + 8 (aux cmd A, B and C, D)
                                               //             + 2 (timestamp)
                                               //             + 3 (accel x, y, z)
                                               //             + 1 (battery voltage reading)
                                               //             + 1 (flex sensor analog reading) (JE: added 08 July 2019)
                                               //             + 1 (magic word sync byte) 
                                               
const int MIN_BUFFER_BYTES_TO_WRITE = 512;    // minimum number of bytes in buffer to operate on. 512 is a sensible choice b/c SD cards have 512 byte sectors 15 Dec 2018
int N_FRAME_WORDS = 144;                      // Frame Words for serial Tx, will be set equal to WORDS_PER_DATA_FRAME * UART_RATIO. Example: 73*4, means there are 292 words in a frame, we read the first 73, then skip 3*73, then read the next 73 and so on..
uint16_t SerialTxArray[BUFSIZE/4];            //array that holds Serial Tx data to be transmitted, size is somewhat arbitrary, should be a fraction of BUFSIZE (depending on fs/serial Tx rate ratio?)
bool CopyToSerialTx = true;                // flag that controls when we are filling up serial tx buffer, vs. not filling because ADC sampling rate exceeds Serial Tx rate.
int CounterSerialTx = 0;                      // initializes how many words (elements) are written into CounterSerialTX, eventually fill up to WORDS_PER_DATA_FRAME number of elements
int RingCountSerialTx = 0;                    // initializes integer that set equal to WORDS_PER_DATA_FRAME * STREAM_SAMPLING_RATIO 

//JE 06 June 2019 - upated serial buffer writing to accomodate subset of channels

int SerialBufIndx = 0; // keeps track of position in CircBuf for elements which we write into Serial Buffer
int SERIAL_MIN_BYTES_TX = 128; // minimum serial Tx packet size to write.  Idea is to prevent many calls to Serial.Write with small packet size (expensive operation to call each time?)




