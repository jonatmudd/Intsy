// ----------------------------------------
// Initialize SD card volume and open file
// Get SD card file name from serial input
// Filename is restricted to 64 characters max
//  11 July 2018, JE
// ---------------------------------------




bool initSDcard() {

  bool SDcardInit_success = false;
  SERIALNAME->println("Iniatalize SD card...");
  if (!sdEx.begin()) {
    SERIALNAME->println("SdFatSdioEX begin() failed. Check SD card is present in microSD slot.");
  }
  else { // SD memory card initiailzed; now open a file
    sdEx.chvol(); // make sdEx the current volume.
    SDcardInit_success = true;

    SERIALNAME->println("Finished!");
  }

  SERIALNAME->write(TERM_CHAR); //write termination character, used to sync with LabView
  return SDcardInit_success;
}




/* ----------------------------------------------------------------------------------
    Create pre-allocated, pre-erased file. Should drastically reduce SD write latency
    Block of code adapted from:
    https://github.com/tni/teensy-samples/blob/master/SdFatSDIO_low_latency_logger.ino

---------------------------------------------------------------------------------------*/

bool SDcardCreateLogFile() { // added JE 2018 Nov 20, creating a contiguous, pre-erased file to minimize logging latency


  bool SDcardOpenFile_success = false;


  // Set file name suffix.  Base file name is "Intsy_"
  // filename for file.createcontigous for SD must be DOS 8.3 format:
  //  http://www.if.ufrj.br/~pef/producao_academica/artigos/audiotermometro/audiotermometro-I/bibliotecas/SdFat/Doc/html/class_sd_base_file.html#ad14a78d348219d6ce096582b6ed74526

  const uint8_t BASE_NAME_SIZE = sizeof(FILE_BASE_NAME) - 1;
  const uint8_t FILE_NAME_DIM  = BASE_NAME_SIZE + 7;
  char binName[FILE_NAME_DIM] = FILE_BASE_NAME "00.tsy";
  char fname[FILE_NAME_DIM];
  strcpy(fname, binName);
  //SERIALNAME->println("Input SD card filename extension:");
  SERIALNAME->println(F("\nEnter two digit file suffix xx (Intsy_xx.tsy)"));
  SERIALNAME->write(TERM_CHAR); // print termination character, to keep synced with LabView VISA R/W operations.
  //Serial.write(name, BASE_NAME_SIZE);

  for (int i = 0; i < 2; i++) {
    while (!SERIALNAME->available()) {
      SysCall::yield();
    }
    char c = SERIALNAME->read();
    //Serial.write(c);
    /* Skip checking this for now, user could enter anything
      if (c < '0' || c > '9') {
      Serial.println(F("\nInvalid digit"));
      return;
      }
    */
    fname[BASE_NAME_SIZE + i] = c;
  }






  serialFlush();


  SERIALNAME->print("Set SDcard data file name: ");
  SERIALNAME->println(fname);
  //SERIALNAME->write(TERM_CHAR); // print termination character, to keep synced with LabView VISA R/W operations.



  //for pre-allocated, pre-erased file.  Copied from Teensy36_SDFatSDIO_LowLatencyLogger, JE 20 Nov 2018
  //uint32_t first_sector = 0;
  //uint32_t last_sector = 0;
  uint32_t next_sector = 0;


  // Create a pre-allocated, pre-erased contiguous file. Any existing file with the same name, is erased.
  // Return true on success.
  //bool create(const char* name, uint32_t size) {
  SERIALNAME->println(F("Creating file (will remove any old file by the same name): "));
  SERIALNAME->println(fname);
  //if(next_sector) close();
  //last_error = E_ok;
  sdEx.remove(fname); // will remove file if already exist.  should probably warn user of this JE 13 Dec 2018

  SERIALNAME->write(TERM_CHAR); // print termination character, to keep synced with LabView VISA R/W operations.


  // User input for file size
  SERIALNAME->println(F("Enter data file size (MB)"));
  SERIALNAME->write(TERM_CHAR); // print termination character, to keep synced with LabView VISA R/W operations.


  // wait for user to input desired sampling rate
  while (!SERIALNAME->available()) {
    SysCall::yield();    // wait for user input
  }

  uint32_t LogFileSz_MB = SERIALNAME->parseInt();

  log_file_size = LogFileSz_MB * 1024 * 1024;

  //uint32_t log_file_size = 2ull * 1024 * 1024 * 1024;  // 2GB %should be determined by Nchans x Fs x Experiment Duration

  SERIALNAME->print(F("File Size (MB) set to:)"));
  SERIALNAME->println(log_file_size);


  serialFlush();





  // probably just have user input log_file_size directly
  if (!dataFile.createContiguous(sdEx.vwd(), fname , log_file_size)) { //requires const char* input ie: "logJE001.bin"
    SERIALNAME->println("ContigFile: createContiguous() failed");
    //last_error = E_create_contiguous;
    return false;
  }
  uint32_t first_sector, last_sector;
  if (!dataFile.contiguousRange(&first_sector, &last_sector)) {
    SERIALNAME->println("PreallocatedFile: contiguousRange() failed");
    // last_error = E_create_contiguous;
    return false;
  }
  uint32_t first_erase_sector = first_sector;
  const size_t erase_count = 64 * 1024; // number of sectors to erase at once
  while (first_erase_sector <= last_sector) {
    if (!sdEx.card()->erase(first_erase_sector, min(first_erase_sector + erase_count, last_sector))) {
      SERIALNAME->println("PreallocatedFile: erase() failed");
      //last_error = E_erase;
      return false;
    }
    first_erase_sector += erase_count;
  }
  SERIALNAME->print("First sector, Last sector: ");
  SERIALNAME->print(first_sector);
  SERIALNAME->print(",");
  SERIALNAME->println(last_sector);

  //this->first_sector = first_sector;
  //this->last_sector = last_sector;
  next_sector = first_sector;

  dataFile.flush();
  

  SERIALNAME->println(F("Finished pre-allocating and pre-erasing log file"));
  SERIALNAME->write(TERM_CHAR); // print termination character, to keep synced with LabView VISA R/W operations.
  return true;
}



