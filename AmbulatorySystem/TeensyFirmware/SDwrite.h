// ------------------------------
// Configuring SD card for writes
// -----------------------------

// ------Create sd card volume  and datafile to write to
SdFatSdioEX sdEx;  //  JE changed from sdio to sdioEX 20 Nov 2018, fastest implementation, requires newer/fast SD card
File dataFile;   // data file instance
boolean SDcardInitStatus = false;
boolean SDcardFileOpenStatus = false;

// file size to be logged. when full, logging with automatically stop
uint32_t log_file_size;

//for flushing buffered SD to FAT
elapsedMillis flushSDElapsedTime;
const int SD_FLUSH_TIMER_MS = 10*1000; // data will be flushed every 10 s (worst case we lose last 10 s of data

// for defining file name
#define FILE_BASE_NAME "Intsy_"

// Define function prototype to get user settings 
extern boolean initSDcard();
//extern boolean SDcardOpenFile(); // OLD do not use.  Use SDcardCrateLogFile() instead
extern boolean SDcardCreateLogFile(); // added JE 2018 Nov 20, creating a contiguous, pre-erased file to minimize logging latency



