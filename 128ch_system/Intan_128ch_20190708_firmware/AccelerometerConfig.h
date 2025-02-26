//accelerometer pins for analog reads
const int PIN_ACCEL_X = 34;
const int PIN_ACCEL_Y = 35;
const int PIN_ACCEL_Z = 36;
uint16_t accelX;
uint16_t accelY;
uint16_t accelZ;

//for reading flex sensor/auxiliary analog input, tucking it here, since its just another analog read
const int PIN_AIN_FLEX = 40; //  was 37 = A18 for flex sensor. JE jan 09 2020, temporary change for direct measuring of TTL pulse for phase shift measurements
uint16_t Vflex;



