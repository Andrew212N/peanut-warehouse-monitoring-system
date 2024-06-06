#include "WiFi_Secrets.h"

// Configure the pins used for the ESP32 connection
#if defined(ADAFRUIT_FEATHER_M0)
#define SPIWIFI SPI      // The SPI port
#define SPIWIFI_SS 16    // Chip select pin
#define ESP32_RESETN 13  // Reset pin
#define SPIWIFI_ACK 8    // a.k.a BUSY or READY pin
#define ESP32_GPIO0 -1   // -1 since not connected
#endif

#define ESP32_RESET_PIN 13

extern char ssid[];
extern char pass[];
char flash_ssid[64];
char flash_pass[64];


int currentCredentialPosition = 0;  // 0 = default credential, 1 = 1st credential on flash, 2 = 2nd credential on flash

char server[] = SERVER;
char path[] = HTTP_REQ_PATH;

char passwordBuffer[64];
char ssidBuffer[64];
int daylightSavings;   // 1 = true (+1 hour)(occurs March to November), 0 = false (occurs November to March)
int utcTimeDiff = -6;  // Deafaulted to CST

String strHour;
String strMinute;
String strSecond;
String strYear;

int keyIndex = 0;
int status = WL_IDLE_STATUS;

const int TIMEOUT_VALUE = 20000;
int httpFail = 0;

// ------------------------------------------------------------------------------------------------------------------------------
// wifi setup and connect
// ------------------------------------------------------------------------------------------------------------------------------
void connectToWiFi() {
  int wifiAttempt = 0;
  WiFi.setPins(SPIWIFI_SS, SPIWIFI_ACK, ESP32_RESETN, ESP32_GPIO0, &SPIWIFI);

  // check for the WiFi module:
  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println("Communication with WiFi module failed!");
    delay(1000);
  }

  // check if firmware version is up to date
  String fv = WiFi.firmwareVersion();
  if (fv < "1.7.0") {
    Serial.println("Please upgrade the firmware");
  }

  // connecting to wifi
  while (status != WL_CONNECTED) {
    Serial.println();

    // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
    // WiFi.setHostname("Peanut");

    if (status != WL_CONNECTED && wifiAttempt <= 1) {
      Serial.print("Attempting to connect to WiFi using default credentials...");
      status = WiFi.begin(ssid, pass);
      if (status == WL_CONNECTED) {
        Serial.println("CONNECTED!");
        currentCredentialPosition = 0;
      } else {
        Serial.println("FAILED!");
        Watchdog.reset();
      }
    } else if (status != WL_CONNECTED && wifiAttempt >= 2 && wifiAttempt <= 3) {
      Serial.print("Attempting to connect to WiFi using 1st credentials from flash storage...");
      char flash_ssid[64];
      char flash_pass[64];
      retrieveCredentials(0, flash_ssid, sizeof(flash_ssid), flash_pass, sizeof(flash_pass));
      status = WiFi.begin(flash_ssid, flash_pass);
      Serial.println(flash_ssid);
      Serial.println(flash_pass);
      delay(100);

      if (status == WL_CONNECTED) {
        Serial.println("CONNECTED!");
        currentCredentialPosition = 1;
      } else {
        Serial.println("FAILED!");
        Watchdog.reset();
      }
    } else if (status != WL_CONNECTED && wifiAttempt >= 4 && wifiAttempt <= 5) {
      Serial.print("Attempting to connect to WiFi using 2nd credentials from flash storage...");
      char flash_ssid[64];
      char flash_pass[64];
      retrieveCredentials(128, flash_ssid, sizeof(flash_ssid), flash_pass, sizeof(flash_pass));
      status = WiFi.begin(flash_ssid, flash_pass);
      Serial.println(flash_ssid);
      Serial.println(flash_pass);
      delay(100);

      if (status == WL_CONNECTED) {
        Serial.println("CONNECTED!");
        currentCredentialPosition = 2;
      } else {
        Serial.println("FAILED!");
        Watchdog.reset();
      }
    }

    // switch(status) {
    //   case WL_IDLE_STATUS:
    //     Serial.println("Idle");
    //     break;
    //   case WL_NO_SSID_AVAIL:
    //     Serial.println("No SSID Available");
    //     break;
    //   case WL_SCAN_COMPLETED:
    //     Serial.println("Scan Completed");
    //     break;
    //   case WL_CONNECT_FAILED:
    //     Serial.println("Connect Failed");
    //     break;
    //   case WL_CONNECTION_LOST:
    //     Serial.println("Connection Lost");
    //     break;
    //   case WL_DISCONNECTED:
    //     Serial.println("Disconnected");
    //     break;
    //   default:
    //     break;
    // }

    wifiAttempt++;

    if (wifiAttempt >= 6) {
      Serial.println();
      Serial.println("SYSTEM RESETTING....");
      NVIC_SystemReset();
    }

    // wait 1 seconds for connection:
    delay(1000);
  }

  wifiAttempt = 0;

  if (status == WL_CONNECTED) {
    Serial.print("WiFi firmware version: ");
    Serial.println(fv);
  }
  return;
}

// ------------------------------------------------------------------------------------------------------------------------------
// Print wifi status
// ------------------------------------------------------------------------------------------------------------------------------
void printWifiStatus() {
  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print your board's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("Signal strength (RSSI): ");
  Serial.print(rssi);
  Serial.println(" dBm");
  Serial.println();
}

// ------------------------------------------------------------------------------------------------------------------------------
// this method makes a HTTP connection to the server:
// ------------------------------------------------------------------------------------------------------------------------------
void httpRequest(int month, int day, int year, int hour, int minute, int second,
                 float temperature, float humidity, float co2, float so2, float hcho, float ph3) {
  // Handling edge cases of date formats
  strHour = String(hour);
  if (minute >= 10)
    strMinute = String(minute);
  else
    strMinute = "0" + String(minute);
  if (second >= 10)
    strSecond = String(second);
  else if (second <= 5)
    strSecond = "00";
  else if (second >= 5 && second < 10)
    strSecond = "0" + String(second);
  if (year < 1000)
    strYear = "20" + String(year);
  else
    strYear = String(year);

  unsigned long startTime = millis();

  client.stop();
  delay(100);
  while (client.connected()) {
    while (client.available()) {
      Serial.write(client.read());
    }
  }
  client.stop();
  delay(100);
  // if there's not a successful connection:
  if (!client.connected()) {
    Serial.println("Client is not connected to server yet");

    if (client.connectSSL(server, 443)) {
      Serial.print("Connected to HTTP server...\n");
      client.print("GET ");
      Serial.print("GET ");
      client.print(path);
      Serial.print(path);
      client.print("?");
      Serial.print("?");
      client.print("date=\"" + String(month) + "/" + String(day) + "/" + strYear + "\"&");
      Serial.print("date=\"" + String(month) + "/" + String(day) + "/" + strYear + "\"&");
      if (hour >= 10) {
        client.print("time=\"" + strHour + ":" + strMinute + ":" + strSecond + "\"&");
        Serial.print("time=\"" + strHour + ":" + strMinute + ":" + strSecond + "\"&");
      } else if (hour == 0) {
        client.print("time=\"00:" + strMinute + ":" + strSecond + "\"&");
        Serial.print("time=\"00:" + strMinute + ":" + strSecond + "\"&");
      } else {
        client.print("time=\"0" + strHour + ":" + strMinute + ":" + strSecond + "\"&");
        Serial.print("time=\"0" + strHour + ":" + strMinute + ":" + strSecond + "\"&");
      }
      client.print("temp=" + String(temperature) + "&");
      Serial.print("temp=" + String(temperature) + "&");
      client.print("humidity=" + String(humidity) + "&");
      Serial.print("humidity=" + String(humidity) + "&");
      client.print("co2=" + String(co2) + "&");
      Serial.print("co2=" + String(co2) + "&");
      client.print("so2=" + String(so2) + "&");
      Serial.print("so2=" + String(so2) + "&");
      client.print("hcho=" + String(hcho) + "&");
      Serial.print("hcho=" + String(hcho) + "&");
      client.print("ph3=" + String(ph3) + "");
      Serial.print("ph3=" + String(ph3) + "");
      client.println(" HTTP/1.1");
      Serial.println(" HTTP/1.1");
      client.print("Host: ");
      Serial.print("Host: ");
      client.println(server);
      Serial.println(server);
      client.println("Connection: close");
      Serial.println("Connection: close");
      client.println();
      Serial.println();

      if (client.connected()) {
        Serial.println("Sent HTTP request successfully!");
        Serial.println();
        httpFail = 0;
      }
    } else {
      httpFail++;
      int httpTime = millis() - startTime;
      if (httpTime < TIMEOUT_VALUE) {
        Serial.println("Client failed to connect to server!");
      } else {
        Serial.println("Client connection attempt timed out!");
        Serial.println(httpTime / 100);
      }

      if (httpFail >= 1) {
        Serial.println("Client failed to connect to server. Resetting ESP32 module...");
        NVIC_SystemReset();
        pinMode(ESP32_RESET_PIN, OUTPUT);
        digitalWrite(ESP32_RESET_PIN, LOW);
        delay(100);
        digitalWrite(ESP32_RESET_PIN, HIGH);
      }
      if (httpFail == 3) {
        Serial.println("Client failed to connect to server 4 times in a row. Resetting ESP32 module did not fix the issue. Resetting system...");
        NVIC_SystemReset();
      }
      delay(5000);
    }
    client.stop();
  }
}
