#define SENSOR_ADDRESS 0x7E    // device address for sensor
#define CONC_REG_ADDRESS 0xD1  // register containing concentration raw value
#define DEC_REG_ADDRESS 0xD2   // register containing decimal place raw value

byte data = 0;                      // stores current read byte
byte ph3_concDataArray[7] = { 0 };  // stores read data from the register with concentration value (0xD1)
byte ph3_decDataArray[6] = { 0 };   // stores read data from the register with no. of decimal places (0xD2)
uint8_t ph3_concIndex = 0;          // index for concentration data array
uint8_t ph3_decIndex = 0;           // index for decimal data array
uint8_t ph3_concChecksum = 0;       // checksum for concentration value
uint8_t ph3_decChecksum = 0;        // checksum for decimal value
uint8_t ph3_decimal = 0;            // 8-bit decimal value from 0xD2
float ph3_concentration = 0;        // concentration value after using formula
bool ph3_concVerified = false;      // indicates successful concentration read
bool ph3_decVerified = false;       // indicates successful decimal read
bool read_done = false;             // read status
bool ph3_done = false;              // actual concentration status

// ------------------------------------------------------------------------------------------------------------------------------
// Returns PH3 concetration
// ------------------------------------------------------------------------------------------------------------------------------
float PH3_getdata() {
  ph3_done = false;

  while (!ph3_done) {
    // Read from register 0xD1 to get raw concentration
    Wire1.beginTransmission((uint8_t)SENSOR_ADDRESS);
    Wire1.write((int)CONC_REG_ADDRESS);
    Wire1.endTransmission(false);
    // Time interval requirement for reading and writing data: >=1s
    delay(1000);

    // request 7 bytes of data from sensor
    Wire1.requestFrom((uint8_t)SENSOR_ADDRESS, (size_t)7, (bool)true);

    // if data available then read 7 bytes of data
    if (Wire1.available() >= 1) {
      ph3_concIndex = 0;
      ph3_concChecksum = 0;
      while (ph3_concIndex < 7) {
        data = Wire1.read();
        ph3_concDataArray[ph3_concIndex] = data;

        // calculate checksum: add-8 sum of byte[0] to byte[5]
        if (ph3_concIndex < 6) {
          ph3_concChecksum += data;
        }
        ph3_concIndex++;

        delay(50);
      }
      read_done = true;
    }

    // Checking for data integrity given new data was read
    if (read_done && ((byte)ph3_concChecksum == ph3_concDataArray[6])) {
      ph3_concVerified = true;
    }

    read_done = false;

    // If reading concentration was successful, read from register 0xD2 to get number of decimal places
    if (ph3_concVerified) {
      Wire1.beginTransmission(SENSOR_ADDRESS);
      Wire1.write((int)DEC_REG_ADDRESS);
      Wire1.endTransmission(false);
      // Time interval requirement for reading and writing data: >=1s
      delay(1000);

      // request 6 bytes of data from sensor
      Wire1.requestFrom((uint8_t)SENSOR_ADDRESS, (size_t)6, true);

      // if data available, then read 6 bytes of data
      if (Wire1.available() >= 0) {
        ph3_decIndex = 0;
        ph3_decChecksum = 0;
        while (ph3_decIndex < 6) {
          data = Wire1.read();
          ph3_decDataArray[ph3_decIndex] = data;

          // calculate checksum: add-8 sum of byte[0] to byte[4]
          if (ph3_decIndex < 5) {
            ph3_decChecksum += data;
          }
          ph3_decIndex++;

          delay(50);
        }
        read_done = true;
      }

      // Checking for data integrity
      if (read_done && (ph3_decChecksum == ph3_decDataArray[5])) {
        ph3_decVerified = true;
      }

      read_done = false;
    }

    // if concentration raw value and decimal raw value are correct then calculate actual concentration
    if (ph3_concVerified && ph3_decVerified) {
      ph3_concVerified = 0;
      ph3_decVerified = 0;
      ph3_decimal = (uint8_t)ph3_decDataArray[0];
      // Gas concentration value = (uint16_t)(byte[0]<<8|byte[1]) / 2^decimal place
      ph3_concentration = ((float)((uint16_t)((ph3_concDataArray[0] << 8) | ph3_concDataArray[1]))) / ((float)(pow(10, ph3_decimal)));
      ph3_done = true;
    }
  }

  return ph3_concentration;
}
