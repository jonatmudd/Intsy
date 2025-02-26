
//this function acquires analog reads 
//byte *AcquireAccelBytes(byte accelarray[6])
//{
//   
//   //acquire 16 bit resolution analog readings (0-3.3V maps to 0 to 2^16-1)
//   accelX = analogRead(PIN_ACCEL_X);
//   accelY = analogRead(PIN_ACCEL_Y);
//   accelZ = analogRead(PIN_ACCEL_Z);
//
//   // break these into individual bytes, for convenience in data streaming
//    accelarray[0] = lowByte(accelX);
//    accelarray[1] = highByte(accelX);
//
//    accelarray[2] = lowByte(accelY);
//    accelarray[3] = highByte(accelY);
//
//    accelarray[4] = lowByte(accelZ);
//    accelarray[5] = highByte(accelZ);
//    
//    
//    return accelarray;
//}

//See this links for example and documentation (note: returning structure is certainly possible in this context: https://arduino.stackexchange.com/questions/30205/returning-an-int-array-from-a-function

// Struct definition.
//struct ArrayAccel {
//    int array[6];
//};
//
//ArrayAccel function()
//{
//   //acquire 16 bit resolution analog readings (0-3.3V maps to 0 to 2^16-1)
//   accelX = analogRead(PIN_ACCEL_X);
//   accelY = analogRead(PIN_ACCEL_Y);
//   accelZ = analogRead(PIN_ACCEL_Z);
//
//    ArrayAccel a;
//    a.array[0] = lowByte(accelX);
//    a.array[1] = highByte(accelX);
//    
//    a.array[2] = lowByte(accelY);
//    a.array[3] = highByte(accelY);
//    
//    a.array[4] = lowByte(accelZ);
//    a.array[5] = highByte(accelZ);
//    
//    
//    return a;
//}
