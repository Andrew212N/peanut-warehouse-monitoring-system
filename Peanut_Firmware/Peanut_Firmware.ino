// https://forum.arduino.cc/t/what-does-client-stop-do-and-how-does-if-client-work-from-the-server-side/239211
#include <Arduino.h>
#include <Adafruit_SCD30.h>
#include <Wire.h>
#include <WiFi.h>
#include <SPI.h>
#include <WiFiNINA.h>
#include <WiFiUdp.h>
#include <RTCZero.h>
#include <FlashStorage_SAMD.h>
#include <Adafruit_SleepyDog.h>

Adafruit_SCD30 scd30 = Adafruit_SCD30();  // SCD30 library
WiFiClient client;
WiFiUDP Udp;
RTCZero rtc;

#include "wiring_private.h"  // pinPeripheral() function
#include "Flash_Storage.h"
#include "Wifi_Data.h"
#include "WiFi_Config.h"
#include "WiFi_Secrets.h"  // NOT INCLUDED IN THE REPO, MAKE YOUR OWN
#include "Time.h"

#define SEN_PWR_EN 25  // controls sensor power
#define PH3_THRESHOLD 5

Uart Serial3(&sercom1, 12, 10, SERCOM_RX_PAD_3, UART_TX_PAD_2);
TwoWire Wire1(&sercom2, 4, 3);

char ssid[] = SECRET_SSID;
char pass[] = SECRET_PASS;

unsigned long previousMillis = 0;
unsigned long currentMillis = 0;
const unsigned long dayInterval = 86400000;
const unsigned long hourInterval = 3600000;
const unsigned long minuteInterval = 60000;

unsigned long timeout = 20000;

bool justChecked = false;
int ph3_countdown = 0;
int responseTime = 0;

int day = 0;
int month = 0;
int year = 0;
int second = 0;
int minute = 0;
int hour = 0;
float temperature = 0;
float humidity = 0;
float co2 = 0;
float so2 = 0;
float hcho = 0;
float ph3 = 0;

int firsttime = 0;

void SERCOM1_Handler() {
  Serial3.IrqHandler();
}

// ------------------------------------------------------------------------------------------------------------------------------
// Generate 20 kHz clock for PH3 I2C SCL
// ------------------------------------------------------------------------------------------------------------------------------
void ph3_clockDivider(void) {
  GCLK->GENDIV.reg = GCLK_GENDIV_DIV(12) |  // Divide the 48MHz clock source by divisor 12: 48MHz/12=4MHz
                     GCLK_GENDIV_ID(3);     // Select Generic Clock (GCLK) 3

  while (GCLK->STATUS.bit.SYNCBUSY)
    ;  // Wait for synchronization

  GCLK->GENCTRL.reg = GCLK_GENCTRL_IDC |          // Set the duty cycle to 50/50 HIGH/LOW
                      GCLK_GENCTRL_GENEN |        // Enable GCLK3
                      GCLK_GENCTRL_SRC_DFLL48M |  // Set the 48MHz clock source
                      GCLK_GENCTRL_ID(3);         // Select GCLK3

  while (GCLK->STATUS.bit.SYNCBUSY)
    ;  // Wait for synchronization

  Wire1.begin();                                     // Set-up the I2C port
  sercom2.disableWIRE();                             // Disable the I2C SERCOM
  GCLK->CLKCTRL.reg = GCLK_CLKCTRL_CLKEN |           // Enable GCLK3 as clock soruce to SERCOM2
                      GCLK_CLKCTRL_GEN_GCLK3 |       // Select GCLK3
                      GCLK_CLKCTRL_ID_SERCOM2_CORE;  // Feed the GCLK3 to SERCOM2

  while (GCLK->STATUS.bit.SYNCBUSY)
    ;  // Wait for synchronization

  SERCOM2->I2CM.BAUD.bit.BAUD = 4000000 / (2 * 20000) - 1;  // Set the I2C clock rate to 20kHz
  sercom2.enableWIRE();
}

// ------------------------------------------------------------------------------------------------------------------------------
// configure communication for sensors and setup wifi
// ------------------------------------------------------------------------------------------------------------------------------
void setup(void) {
  Serial.begin(115200);
  // while (!Serial) delay(10);     // will pause until serial console opens

  // uart communication for SO2 sensor
  Serial5.begin(9600);
  Serial5.flush();

  // uart communication for HCHO sensor
  Serial3.begin(9600);
  pinPeripheral(12, PIO_SERCOM);
  pinPeripheral(10, PIO_SERCOM);
  Serial3.flush();

  //setting the time based on server response
  rtc.begin();

  // i2c communication for SCD30 sensor
  Wire.begin();
  Wire.setClock(400000);

  // i2c communication for PH3 sensor
  ph3_clockDivider();
  pinPeripheral(4, PIO_SERCOM_ALT);
  pinPeripheral(3, PIO_SERCOM_ALT);

  // enable power to all sensors
  pinMode(SEN_PWR_EN, OUTPUT);
  digitalWrite(SEN_PWR_EN, LOW);

  // Setup and connect to wifi
  connectToWiFi();
  printWifiStatus();

  // Try to initialize CO2 sensor
  if (!scd30.begin()) {
    Serial.println("Failed to find SCD30 chip");
    while (1) {
      delay(10);
      if (scd30.begin()) {
        Serial.println("SCD30 Found!");
        break;
      }
    }
  }

  Serial.print("Measurement Interval: ");
  Serial.println(scd30.getMeasurementInterval());
  Serial.println();

  Udp.begin(localPort);
  client.setTimeout(timeout);
  delay(100);
  updateConfig();
  sendTimeRequest();

  Watchdog.enable(60000);
}

#include "SO2.h"
#include "HCHO.h"
#include "PH3.h"

// ------------------------------------------------------------------------------------------------------------------------------
// Update sensor data
// ------------------------------------------------------------------------------------------------------------------------------
void updateData(void) {
  temperature = scd30.temperature * 9 / 5 + 32;
  humidity = scd30.relative_humidity;
  co2 = scd30.CO2;
  so2 = SO2_getdata();
  hcho = HCHO_getdata();
  ph3 = PH3_getdata();
}

// ------------------------------------------------------------------------------------------------------------------------------
// Print sensor data to serial monitor
// ------------------------------------------------------------------------------------------------------------------------------
void printData(void) {
  Serial.print("Temperature: ");
  Serial.print(temperature);
  Serial.println(" degrees F");

  Serial.print("Relative Humidity: ");
  Serial.print(humidity);
  Serial.println(" %");

  Serial.print("CO2 concentration: ");
  Serial.print(co2, 3);
  Serial.println(" ppm");

  Serial.print("SO2 concentration: ");
  Serial.print(so2, 3);
  Serial.println(" ppm");

  Serial.print("HCHO concentration: ");
  Serial.print(hcho, 3);
  Serial.println(" ppm");

  Serial.print("PH3 concentration: ");
  Serial.print(ph3, 3);
  Serial.println(" ppm");
  Serial.println();
}

// ------------------------------------------------------------------------------------------------------------------------------
// Power off sensors if PH3 concentration crosses threshold
// ------------------------------------------------------------------------------------------------------------------------------
void shutdownCheck(void) {
  if (ph3 >= PH3_THRESHOLD) {
    ph3_countdown = millis();
  }

  // if (firsttime == 0) {
  //   ph3 = 10;
  //   firsttime = 1;
  // } else {
  //   ph3 = 0;
  // }

  while (ph3 >= PH3_THRESHOLD) {
    Watchdog.reset();
    Serial.println("PH3 has exceeded threshold!");
    updateAlarmConfig();
    Watchdog.reset();
    if (alarmResponded == 1) {
      Serial.println("User has responded!");
      if (shutDown == 1) {
        Serial.println("User wants to shut down the system!");
        digitalWrite(SEN_PWR_EN, HIGH);
        if (digitalRead(SEN_PWR_EN) == HIGH) {
          Serial.println("Turning off all sensors!");
          while (ph3 >= PH3_THRESHOLD) {
            Watchdog.reset();
            delay(3500);
            Watchdog.reset();
            ph3 = PH3_getdata();
            Serial.println(ph3);
            Watchdog.reset();
            if (ph3 < PH3_THRESHOLD) {
              // Wire.begin();
              // Wire.setClock(400000);
              digitalWrite(SEN_PWR_EN, LOW);
              delay(100);
              Watchdog.reset();
              if (digitalRead(SEN_PWR_EN) == LOW) {
                // NVIC_SystemReset();
                // Watchdog.reset();
                Serial.println("PH3 has returned to normal level. Turning on power to all sensors!");
                Wire.begin();
                Wire.setClock(400000);
                // Try to initialize CO2 sensor
                if (!scd30.begin()) {
                  Serial.println("Failed to find SCD30 chip");
                  scd30.reset();
                  delay(100);
                  while (1) {
                    // scd30.reset();
                    Watchdog.reset();
                    delay(10);
                    if (scd30.begin()) {
                      Serial.println("SCD30 Found!");
                      break;
                    }
                  }
                } else {
                  Serial.println("SCD30 successfully reinitialized after turning on power");
                }
                delay(100);
                break;
              }
            }
          }
        }
      } else {
        Serial.println("User did not want to shutdown the system!");
        break;
      }
    } else {
      if ((millis() - ph3_countdown) >= (minuteInterval * 5)) {
        Serial.println("5 minutes have passed since ph3 crossed threshold!");
        Watchdog.reset();
        digitalWrite(SEN_PWR_EN, HIGH);
        if (digitalRead(SEN_PWR_EN) == HIGH) {
          Serial.println("User did not response. Turning off all sensors!");
          while (ph3 >= PH3_THRESHOLD) {
            Watchdog.reset();
            delay(1500);
            ph3 = PH3_getdata();
            Serial.println(ph3);
            Watchdog.reset();
            if (ph3 < PH3_THRESHOLD) {
              Watchdog.reset();
              digitalWrite(SEN_PWR_EN, LOW);
              delay(100);
              if (digitalRead(!SEN_PWR_EN) == LOW) {
                Wire.begin();
                Wire.setClock(400000);
                // NVIC_SystemReset();
                Watchdog.reset();
                Serial.println("PH3 has returned to normal level. Turning on power to all sensors!");
                if (!scd30.begin()) {
                  Serial.println("Failed to find SCD30 chip");
                  while (1) {
                    Watchdog.reset();
                    delay(10);
                    if (scd30.begin()) {
                      Serial.println("SCD30 Found!");
                      break;
                    }
                  }
                }
                break;
              }
            }
          }
        }
      }
    }
    Watchdog.reset();
    delay(15);
    ph3 = PH3_getdata();
  }
  // ph3_countdown = 0;
}


// ------------------------------------------------------------------------------------------------------------------------------
// loop
// ------------------------------------------------------------------------------------------------------------------------------
void loop() {
  Serial.println("Entering main loop...");
  updateTime();
  printTime();
  Serial.println("Updated the date and time");

  // sync time every hour
  currentMillis = millis();
  Serial.println(currentMillis);
  if (currentMillis - previousMillis >= (hourInterval)) {
    Watchdog.reset();
    Serial.println("An hour has passed since time and WiFi configuration sync. Updating...");
    updateConfig();
    delay(100);
    sendTimeRequest();
    previousMillis = currentMillis;
    Serial.println("Data Updated");
  }

  // update data and send HTTPS request every 15 seconds
  Serial.println(second);
  if (int(second) % 15 == 0) {
    Serial.println("Second is divisible by 15");
    if (scd30.dataReady()) {
      Serial.println("SCD30 data is ready");
      if (!scd30.read()) {
        Serial.println("Error reading sensor data");
        return;
      }
      updateData();
      Serial.println("Updated the sensor data");
      printTime();
      printData();

      delay(2000);

      httpRequest(month, day, year, hour, minute, second, temperature, humidity, co2, so2, hcho, ph3);
      Serial.println("Back to main loop");

      shutdownCheck();
      Serial.println("Done checking ph3 threshold");
      delay(1050);
    } else {
      Serial.println("SCD30 data is not ready. Resetting system...");
      NVIC_SystemReset();
    }
  }

  //reconnect to wifi if connection lost
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi got disconnected! Will reconnect shortly...");
    status = WL_IDLE_STATUS;
    Watchdog.reset();
    connectToWiFi();
  }
  Watchdog.reset();
}
