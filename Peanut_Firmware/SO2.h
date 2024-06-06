byte so2_dataArray[12] = { 0 };
uint8_t so2_command[] = { 0xFF, 0x00, 0x87, 0x00, 0x00, 0x00, 0x00, 0x00, 0x79 };
uint8_t so2_index = 0;
uint8_t so2_checksum = 0;
bool so2_verified = 0;
bool so2_done = false;
float so2_concentration = 0;

// ------------------------------------------------------------------------------------------------------------------------------
// Returns SO2 concetration
// ------------------------------------------------------------------------------------------------------------------------------
float SO2_getdata() {
  so2_done = false;
  // command for combined reading of gas concentration value, temperature and humidity
  Serial5.write(so2_command, sizeof(so2_command));
  delay(100);

  while (!so2_done) {
    if (Serial5.available() > 0) {
      byte data = Serial5.read();

      switch (data) {
        case 0xFF:
          so2_dataArray[so2_index] = data;
          so2_index++;
          delay(5);

          // calculate checksum to verify integrity of the recieved data
          while (so2_index < 13) {
            data = Serial5.read();
            so2_dataArray[so2_index] = data;
            if (so2_index < 12) {
              so2_checksum += data;
            }
            so2_index++;
            delay(5);
          }

          // Checksum: 1 ~ 11 bits of data are added to generate an 8-bit data, each bit is inverted, and 1 is added at the end
          so2_checksum = ~so2_checksum + 1;

          // Checking for data integrity
          if (so2_checksum == so2_dataArray[12]) {
            so2_verified = 1;
            so2_checksum = 0;
          }

          so2_index = 0;
          break;

        default:
          break;
      }
    }

    // calculate SO2 concentration if raw concentration was verified
    if (so2_verified) {
      so2_verified = 0;
      // Gas concentration value = gas concentration high bit * 256 + gas concentration bit
      so2_concentration = ((float)(((float)so2_dataArray[6] * 256.0) + ((float)so2_dataArray[7])) / 1.0) - 3.000;
      so2_done = true;

      if (so2_concentration < 0) {
        so2_concentration = 0;
      }
    }
  }

  return so2_concentration;
}
