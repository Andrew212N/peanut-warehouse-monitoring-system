const int START_ADDRESS_1 = 0;
const int START_ADDRESS_2 = 128;

struct Credentials {
  char WiFi_pass[64];
  char WiFi_ssid[64];
};

int startAddress = 0;
char my_test[64];

Credentials newCredentials;
Credentials existingCredentials;
Credentials existingCredentials1;
Credentials existingCredentials2;

// ------------------------------------------------------------------------------------------------------------------------------
// Print WiFi Credentials
// ------------------------------------------------------------------------------------------------------------------------------
void printCredentials(Credentials& credentials) {
  Serial.println("===============");
  Serial.print("New Password: ");
  Serial.println(credentials.WiFi_pass);
  Serial.print("New SSID: ");
  Serial.println(credentials.WiFi_ssid);
  Serial.println("===============");
}

// ------------------------------------------------------------------------------------------------------------------------------
// Retrieve WiFi credentials from flash storage
// ------------------------------------------------------------------------------------------------------------------------------
void retrieveCredentials(int start_address, char* ssid_buff, size_t ssid_buff_size, char* pass_buff, size_t pass_buff_size) {
  // void retrieveCredentials(void) {
  Serial.println();
  Serial.print("Getting WiFi credentials from flash storage on ");
  Serial.println(BOARD_NAME);
  Serial.println(FLASH_STORAGE_SAMD_VERSION);

  Serial.print("EEPROM length: ");
  Serial.println(EEPROM.length());

  Serial.print("Reading from position: ");
  Serial.println(start_address);

  // Example: Modify credentials and save back to EEPROM
  // strcpy(newCredentials.WiFi_ssid, "Peanut Warehouse WiFi");
  // strcpy(newCredentials.WiFi_pass, "ISCS2023");

  // EEPROM.put(START_ADDRESS, newCredentials);

  EEPROM.get(start_address, existingCredentials);

  Serial.print("SSID: ");
  Serial.println(existingCredentials.WiFi_ssid);
  Serial.print("Password: ");
  Serial.println(existingCredentials.WiFi_pass);

  // Copy existing credentials to ssid and password
  strncpy(ssid_buff, existingCredentials.WiFi_ssid, (ssid_buff_size - 1));
  strncpy(pass_buff, existingCredentials.WiFi_pass, (pass_buff_size - 1));

  Serial.println(ssid_buff);
  Serial.println(pass_buff);

  Serial.println("Successfully retrieved WiFi credentials from flash storage");
}

// ------------------------------------------------------------------------------------------------------------------------------
// Compare credentials
// ------------------------------------------------------------------------------------------------------------------------------
bool compareCredentials() {
  // Retrieve the existing credentials from both locations
  EEPROM.get(START_ADDRESS_1, existingCredentials1);
  EEPROM.get(START_ADDRESS_2, existingCredentials2);

  // Compare newCredentials with existingCredentials1
  if (strcmp(newCredentials.WiFi_ssid, existingCredentials1.WiFi_ssid) == 0 && strcmp(newCredentials.WiFi_pass, existingCredentials1.WiFi_pass) == 0) {
    Serial.println("Match found at position 1");
    return true;  // Match found
  }

  // Compare newCredentials with existingCredentials2
  if (strcmp(newCredentials.WiFi_ssid, existingCredentials2.WiFi_ssid) == 0 && strcmp(newCredentials.WiFi_pass, existingCredentials2.WiFi_pass) == 0) {
    Serial.println("Match found at position 2");
    return true;  // Match found
  }

  Serial.println("No match found on flash");
  return false;  // No match found
}

// ------------------------------------------------------------------------------------------------------------------------------
// Update flash storage with new WiFi credentials
// ------------------------------------------------------------------------------------------------------------------------------
void updateCredentials(int currentCredPosition, char* ssid_buff, size_t ssid_buff_size, char* pass_buff, size_t pass_buff_size) {
  Serial.print(F("\nStart EEPROM_put on "));
  Serial.println(BOARD_NAME);
  Serial.println(FLASH_STORAGE_SAMD_VERSION);

  Serial.print("EEPROM length: ");
  Serial.println(EEPROM.length());

  // Copy ssid and pass to newCredentials
  strncpy(newCredentials.WiFi_ssid, ssid_buff, sizeof(newCredentials.WiFi_ssid));
  strncpy(newCredentials.WiFi_pass, pass_buff, sizeof(newCredentials.WiFi_pass));

  if (compareCredentials() == false) {
    if (currentCredPosition == 0 || currentCredPosition == 2) {
      startAddress = 0;
      Serial.println("Updating credential at position: 1st");
    } else if (currentCredPosition == 1) {
      startAddress = 128;
      Serial.println("Updating credential at position: 2nd");
    }

    EEPROM.put(startAddress, newCredentials);

    if (!EEPROM.getCommitASAP()) {
      Serial.println("CommitASAP not set. Need commit()");
      EEPROM.commit();
    }

    Serial.println("Done writing new credentials to EEPROM: ");
    printCredentials(newCredentials);
  } else {
    Serial.println("Credential already exists in flash. No need for an update!");
  }
}
