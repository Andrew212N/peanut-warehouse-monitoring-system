#include "WiFi_Secrets.h"

char key[] = SECRET_KEY;
char shutDownKey[] = SHUTDOWN_KEY;
char server1[] = SERVER;
char configPath[] = CONFIG_PATH;

char prevChar = '\0';
String newLocation;
String responseBody;

bool isRedirect = false;
bool jsonStarted = false;

int shutDown = 0;
int alarmResponded = 0;


extern int currentCredentialPosition;

// ------------------------------------------responsestar------------------------------------------------------------------------------------
// Parse the JSON response from google sheets
// ------------------------------------------------------------------------------------------------------------------------------
void parseWiFiResponse(String response) {
  Watchdog.reset();
  // Assuming there are always 4 comma-separated values
  // SSID, Password, Daylight Savings Indicator, UTC Time Difference
  int numValues = 4;
  String values[numValues];

  int startIndex = 0;
  int endIndex = 0;
  int i = 0;

  // Going through each section of the response, separated by comma
  while ((endIndex = response.indexOf(",", startIndex)) != -1 && i < numValues) {
    Serial.println("Parsing response...");
    values[i] = response.substring(startIndex, endIndex);
    startIndex = endIndex + 1;
    i++;
  }
  Serial.println("Done parsing response from config sheet!");

  // Extract the last value after the final comma (if all values are present)
  if (i < numValues) {
    values[i] = response.substring(startIndex);
  }
  // Check if any of the values are null
  for (i = 0; i < numValues; i++) {
    if (values[i] == NULL)
      return;
  }

  // Convert values to respective types and update variables
  strcpy(ssidBuffer, values[0].c_str());      // Copy SSID to buffer
  strcpy(passwordBuffer, values[1].c_str());  // Copy password to buffer
  daylightSavings = values[2].toInt();        // Convert daylight savings string to integer
  utcTimeDiff = values[3].toInt();            // Convert UTC time difference string to integer

  updateCredentials(currentCredentialPosition, ssidBuffer, sizeof(ssidBuffer), passwordBuffer, sizeof(passwordBuffer));
  /*
  Serial.flush();
  Serial.print("SSID: ");
  Serial.println(ssidBuffer);
  Serial.print("Password: ");
  Serial.println(passwordBuffer);
  Serial.print("DaylightSavings: ");
  Serial.println(daylightSavings);
  Serial.print("utcTimeDiff: ");
  Serial.println(utcTimeDiff);
*/
}

// ------------------------------------------------------------------------------------------------------------------------------
// Update WiFi configurations
// ------------------------------------------------------------------------------------------------------------------------------
void updateConfig() {
  Watchdog.reset();
  // Close any connection before sending a new request to free the socket on the Nina module
  while (client.available()) {
    client.read();
  }
  client.stop();

  if (client.connectSSL(server1, 443)) {
    //send a key to google sheets, google sheets compares and returns data if match
    Serial.println("Connected to Config Server");
    client.print("GET ");
    client.print(configPath);
    client.print("?");
    client.print("key=" + String(key));
    client.println(" HTTP/1.1");
    client.print("Host: ");
    client.println(server1);
    client.println("Connection: close");
    client.println();

    // Read response headers and get new location
    while (client.connected()) {
      if (client.available()) {
        String line = client.readStringUntil('\n');
        if (line.startsWith("Location: ")) {
          isRedirect = true;
          newLocation.remove(0);
          newLocation = line.substring(10);
        } else if (line == "\r") {
          break;  // End of headers
        }
      }
    }
    Serial.println("Parsed New Location");

    // Parse the new location and make a new request
    if (isRedirect) {
      Watchdog.reset();
      Serial.println("Redirecting to: " + newLocation);
      int protocolEnd = newLocation.indexOf("://");
      int pathStart = newLocation.indexOf("/", protocolEnd + 3);
      String newServer = newLocation.substring(protocolEnd + 3, pathStart);
      String newPath = newLocation.substring(pathStart);

      if (newServer != server1) {
        //Serial.println("New server: " + newServer);
        //Serial.println("New path: " + newPath);
        // Close any connection before sending a new request to free the socket on the Nina module
        while (client.available()) {
          client.read();
        }
        client.stop();
        // Make a new request to the new location
        if (client.connectSSL(server1, 443)) {
          client.print("GET ");
          client.print(newPath);
          client.println(" HTTP/1.1");
          client.print("Host: ");
          client.println(newServer);
          client.println("Connection: close");
          client.println();

          Serial.println("Connected to New HTTP Request location");

          // Read response status code and body
          while (client.connected()) {
            if (client.available()) {
              char c = client.read();
              //Serial.print(c); // Print each byte of the response
              responseBody += c;
              // Check if the response ends
              if (c == '}') {
                client.stop();
                break;  // Break out of the loop when end of line is found
              }
            }
          }
          Serial.println("Got the HTTP config response");
          //Serial.println(responseBody);

          // Extract Sheets response from HTTP responsee
          int startIndex = responseBody.indexOf('{');
          String extractedText = responseBody.substring(startIndex + 1);  // +1 to skip '{'

          // Extract until the closing brace '}'
          int endIndex = extractedText.indexOf('}');
          if (endIndex != -1) {
            extractedText = extractedText.substring(0, endIndex);  // Exclude closing brace
          }
          Serial.println(extractedText);
          parseWiFiResponse(extractedText);
          // Clearing the response
          responseBody = "";

        } else {
          Serial.println("Connection to new server failed");
        }
      }
      isRedirect = false;
    }
    // Close any connection before sending a new request to free the socket on the Nina module
    client.stop();
  } else {
    Serial.println("HTTPS connection failed. Couldn't get WiFi config.");
  }
}

// ------------------------------------------responsestar------------------------------------------------------------------------------------
// Parse the JSON response from google sheets
// ------------------------------------------------------------------------------------------------------------------------------
void parseAlarmResponse(String response) {
  Watchdog.reset();
  int numValues = 2;
  int startIndex = 0;
  int endIndex = 0;
  int i = 0;
  String values[numValues];

  // Going through each section of the response, separated by comma
  while ((endIndex = response.indexOf(",", startIndex)) != -1 && i < numValues) {
    Serial.println("Parsing response...");
    values[i] = response.substring(startIndex, endIndex);
    startIndex = endIndex + 1;
    i++;
  }

  // Extract the last value after the final comma (if all values are present)
  if (i < numValues) {
    values[i] = response.substring(startIndex);
  }

  // Check if any of the values are null
  for (i = 0; i < numValues; i++) {
    if (values[i] == NULL)
      return;
  }

  // Convert values to respective types and update variables
  shutDown = values[0].toInt();
  alarmResponded = values[1].toInt();

  // Serial.flush();
  Serial.print("shutDown value: ");
  Serial.println(shutDown);
  Serial.print("alarmResponded value: ");
  Serial.println(alarmResponded);
}


// ------------------------------------------------------------------------------------------------------------------------------
// Update Fire Alarm configurations
// ------------------------------------------------------------------------------------------------------------------------------
void updateAlarmConfig() {
  Watchdog.reset();
  // Close any connection before sending a new request to free the socket on the Nina module
  while (client.available()) {
    client.read();
  }
  client.stop();

  if (client.connectSSL(server1, 443)) {
    //send a key to google sheets, google sheets compares and returns data if match
    Serial.println("Connected to Config Server");
    client.print("GET ");
    client.print(configPath);
    client.print("?");
    client.print("key=" + String(shutDownKey));
    client.println(" HTTP/1.1");
    client.print("Host: ");
    client.println(server1);
    client.println("Connection: close");
    client.println();

    // Read response headers and get new location
    while (client.connected()) {
      if (client.available()) {
        String line = client.readStringUntil('\n');
        if (line.startsWith("Location: ")) {
          isRedirect = true;
          newLocation.remove(0);
          newLocation = line.substring(10);
        } else if (line == "\r") {
          break;  // End of headers
        }
      }
    }

    // Parse the new location and make a new request
    if (isRedirect) {
      Watchdog.reset();
      //Serial.println("Redirecting to: " + newLocation);
      int protocolEnd = newLocation.indexOf("://");
      int pathStart = newLocation.indexOf("/", protocolEnd + 3);
      String newServer = newLocation.substring(protocolEnd + 3, pathStart);
      String newPath = newLocation.substring(pathStart);

      if (newServer != server1) {
        while (client.available()) {
          client.read();
        }
        client.stop();

        // Make a new request to the new location
        if (client.connectSSL(server1, 443)) {
          client.print("GET ");
          client.print(newPath);
          client.println(" HTTP/1.1");
          client.print("Host: ");
          client.println(newServer);
          client.println("Connection: close");
          client.println();

          Serial.println("Connected to New location");

          // Read response status code and body
          while (client.connected()) {
            if (client.available()) {
              char c = client.read();
              responseBody += c;
              // Break out of the loop when end of line is found
              if (c == '}') {
                client.stop();
                break;
              }
            }
          }
          //Serial.println(responseBody);

          // Extract Sheets response from HTTP responsee
          int startIndex = responseBody.indexOf('{');
          String extractedText = responseBody.substring(startIndex + 1);  // +1 to skip '{'

          // Extract until the closing brace '}'
          int endIndex = extractedText.indexOf('}');
          if (endIndex != -1) {
            extractedText = extractedText.substring(0, endIndex);  // Exclude closing brace
          }
          Serial.println(extractedText);
          parseAlarmResponse(extractedText);
          // Clearing the response
          responseBody = "";

        } else {
          Serial.println("Connection to new server failed");
        }
      }
      isRedirect = false;
    }
    client.stop();
  } else {
    Serial.println("HTTPS connection failed. Couldn't get user response data");
  }
}
