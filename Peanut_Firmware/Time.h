extern int day;
extern int month;
extern int year;
extern int second;
extern int minute;
extern int hour;

bool timeSuccess = false;

unsigned int localPort = 8888;          // local port to listen for UDP packets
IPAddress timeServer(132, 163, 96, 6);  // time.nist.gov NTP server
const int NTP_PACKET_SIZE = 48;         // NTP time stamp is in the first 48 bytes of the message
byte packetBuffer[NTP_PACKET_SIZE];     // buffer to hold incoming and outgoing packets

// ------------------------------------------------------------------------------------------------------------------------------
// Print date and time to serial monitor
// ------------------------------------------------------------------------------------------------------------------------------
void print2digits(int number) {
  if (number < 10) {
    Serial.print("0");
  }
  Serial.print(number);
}

void printTime() {
  print2digits(month);
  Serial.print("/");
  print2digits(day);
  Serial.print("/");
  Serial.print(year);
  Serial.print("  @");

  print2digits(hour);
  Serial.print(":");
  print2digits(minute);
  Serial.print(":");
  print2digits(second);
  Serial.println("");
}

// ------------------------------------------------------------------------------------------------------------------------------
// Update sensor date and time
// ------------------------------------------------------------------------------------------------------------------------------
void updateTime(void) {
  second = rtc.getSeconds();
  minute = rtc.getMinutes();
  hour = rtc.getHours();
  day = rtc.getDay();
  month = rtc.getMonth();
  year = rtc.getYear();
}

// ------------------------------------------------------------------------------------------------------------------------------
// send an NTP request to the time server at the given address
// ------------------------------------------------------------------------------------------------------------------------------
unsigned long sendNTPpacket(IPAddress& address) {
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);

  // Initialize values needed to form NTP request
  packetBuffer[0] = 0b11100011;  // LI, Version, Mode
  packetBuffer[1] = 0;           // Stratum, or type of clock
  packetBuffer[2] = 6;           // Polling Interval
  packetBuffer[3] = 0xEC;        // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  //            .
  //            .
  //            .
  //            .
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;

  // all NTP fields have been given values, now you can send a packet requesting a timestamp:
  Udp.beginPacket(address, 123);  // NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
  return 0;
}

// ------------------------------------------------------------------------------------------------------------------------------
// setting the time based on server response
// ------------------------------------------------------------------------------------------------------------------------------
void sendTimeRequest() {
  Watchdog.reset();
  timeSuccess = false;
  int parseAttempt = 0;
  while (!Udp.parsePacket()) {
    parseAttempt++;
    Serial.print("Making time request...");
    sendNTPpacket(timeServer);
    delay(2000);
    if (Udp.parsePacket()) {
      timeSuccess = true;
      break;
    }

    if (parseAttempt >= 5) {
      Serial.println("Time request was not successful. Will try again in an hour!");
      timeSuccess = false;
      updateTime();
      break;
    }
  }
  if (timeSuccess == true) {
    // Serial.println("Packet recieved.");
    Udp.read(packetBuffer, NTP_PACKET_SIZE);

    // calculations
    unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
    unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
    unsigned long secsSince1900 = highWord << 16 | lowWord;
    const unsigned long seventyYears = 2208988800UL;
    unsigned long epoch = secsSince1900 - seventyYears;

    Serial.println("Comparing Epoch to previous value...");
    if (epoch > rtc.getEpoch()) {
      // setting epoch/UNIX Time used for the Date
      rtc.setEpoch(epoch + (3600 * utcTimeDiff) + (3600 * daylightSavings));

      // setting the time
      rtc.setSeconds(epoch % 60);
      rtc.setMinutes((epoch % 3600) / 60);
      rtc.setHours(((epoch + utcTimeDiff * 3600 + daylightSavings * 3600) % 86400L) / 3600);

      delay(100);

      Serial.println("time sync successful!");
      Serial.print("The current time is ");
      printTime();
      return;
    } else {
      Serial.println("Time sync failed, new time is less than old time");
      return;
    }
  }
}
