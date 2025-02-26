// ----------------------------------
//   Soft restart, user can trigger this with proper input to serial monitor
// ---------------------------------

void softRestart() {
 SCB_AIRCR = CPU_RESTART_VAL;  //This is Teensy 3.1 specific.  see: https://forums.adafruit.com/viewtopic.php?f=24&t=74769
//_reboot_Teensyduino();  // Try hard reboot instead? Testing 23 Nov 2016. JE
}
