byte hcho_dataArray[12] = { 0 };
uint8_t hcho_command[] = { 0xFF, 0x01, 0x87, 0x00, 0x00, 0x00, 0x00, 0x00, 0x78 };
uint8_t hcho_index = 0;
uint8_t hcho_checksum = 0;
bool hcho_verified = false;
bool hcho_done = false;
float hcho_concentration = 0;

// ------------------------------------------------------------------------------------------------------------------------------
// Returns HCHO concetration
// ------------------------------------------------------------------------------------------------------------------------------
float HCHO_getdata() {
  hcho_done = false;
  // command for combined reading of gas concentration value, temperature and humidity
  Serial3.write(hcho_command, sizeof(hcho_command));
  delay(100);

  while (!hcho_done) {
    if (Serial3.available() > 0) {
      byte data = Serial3.read();

      switch (data) {
        case 0xFF:
          hcho_dataArray[hcho_index] = data;
          hcho_index++;
          delay(5);

          // calculate checksum to verify integrity of the recieved data
          while (hcho_index < 13) {
            data = Serial3.read();
            hcho_dataArray[hcho_index] = data;
            if (hcho_index < 12) {
              hcho_checksum += data;
            }
            hcho_index++;
            delay(5);
          }

          // Checksum: 1 ~ 11 bits of data are added to generate an 8-bit data, each bit is inverted, and 1 is added at the end
          hcho_checksum = ~hcho_checksum + 1;

          // Checking for data integrity
          if (hcho_checksum == hcho_dataArray[12]) {
            hcho_verified = 1;
            hcho_checksum = 0;
          }

          hcho_index = 0;
          break;

        default:
          break;
      }
    }

    // calculate HCHO concentration if raw concentration was verified
    if (hcho_verified) {
      hcho_verified = 0;
      // Gas concentration value = gas concentration high bit * 256 + gas concentration bit
      hcho_concentration = (float)(((float)hcho_dataArray[6] * 256.0) + ((float)hcho_dataArray[7])) / 1.0;
      hcho_done = true;
    }
  }

  return hcho_concentration;
}
