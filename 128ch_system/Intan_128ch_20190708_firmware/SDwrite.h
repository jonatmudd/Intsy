// ------------------------------
// Configuring SD card for writes
// -----------------------------

// ------Create sd card volume  and datafile to write to
// 
// ------Create sd card volume  and datafile to write to
// SDFAT library circa apr 2024 requires updated SD types JE 05 Apr 2024
SdFs sdEx;  //  JE changed from sdio to sdioEX 20 Nov 2018, fastest implementation, requires newer/fast SD card
//SdFat sdEx;  //  JE changed from sdio to sdioEX 20 Nov 2018, fastest implementation, requires newer/fast SD card
FsFile dataFile;   // data file instance

bool SDcardInitStatus = false;
bool SDcardFileOpenStatus = false;

// file size to be logged. when full, logging with automatically stop
uint32_t log_file_size;

//for flushing buffered SD to FAT
elapsedMillis flushSDElapsedTime;
const int SD_FLUSH_TIMER_MS = 10*1000; // data will be flushed every 10 s (worst case we lose last 10 s of data

// for defining file name
#define FILE_BASE_NAME "Intsy_"

// Define function prototype to get user settings 
extern bool initSDcard();
//extern bool SDcardOpenFile(); // OLD do not use.  Use SDcardCrateLogFile() instead
extern bool SDcardCreateLogFile(); // added JE 2018 Nov 20, creating a contiguous, pre-erased file to minimize logging latency
