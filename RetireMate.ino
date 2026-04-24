#include <WiFi.h>
#include <DNSServer.h>  //https://github.com/esp8266/Arduino/tree/master/libraries/DNSServer
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <WiFiManager.h>  //https://github.com/tzapu/WiFiManager
#include <time.h>
#include <SPI.h>
#include <SD.h>
#include <Timezone.h>
#include <TFT_eSPI.h>
#include <TJpg_Decoder.h>
#include <settings.h>
#include <WebServer.h>
#include <FS.h>
#include "LittleFS.h"

//#define FORMAT_LittleFS  // Wipe SPIFFS and all files!

#ifdef LOAD_GFXFF  // Only include the fonts if LOAD_GFXFF is defined in User_Setup.h

// Use these when printing or drawing text in GLCD and high rendering speed fonts
#define GFXFF 1
#define GLCD 0
#define FONT2 2
#define FONT4 4
#define FONT6 6
#define FONT7 7
#define FONT8 8

#define FS12 &FreeSerif12pt7b
#endif  // LOAD_GFXFF

#define VERSION "v1.6"
#define hostNameCYD "RetireMate"  // this will be the AP name you set to setup wifi
#define config "/config.txt"      // where all the settings are stored

#define RED_PIN 22
#define GREEN_PIN 16
#define BLUE_PIN 17

WebServer server(80);

// ---------------- SD Card FILES ----------------
#define SD_MOSI 23  // pin numbers for the CDY 2.8 7789
#define SD_MISO 19
#define SD_SCK 18
#define SD_CS 5

// Use hardware SPI
TFT_eSPI tft = TFT_eSPI();

// ---------------- DISPLAY ----------------
#define SCREEN_W 240
#define SCREEN_H 320

// ----------- CLOCK --------------
bool show24HR = false;
uint8_t lastSecond = 99;
uint8_t lastMinute;
uint8_t lastHour;
uint8_t lastDay;
uint8_t lastMonth;
uint16_t lastYear, myYear;
uint16_t dayOfTheYear;  // day of the year 1 to 366...
uint8_t myHour, myMinute, mySecond, my24Hour, myDay, myMonth, myWeekDay, tempMonth;
uint16_t defMonth, defDay, defYear;  // for the HTML code
bool colonBlink = false;             // we want the colon to blink
const String monthNames[12] = { "January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December" };

// ── Global result variables ───
int g_diffYears = 0;
int g_diffMonths = 0;
int g_diffDays = 0;

// -1 = past, 0 = today, 1 = future
int g_dateStatus = 0;

// ---------------- TIME ----------------
#define NTP_SERVER "pool.ntp.org"
#define NTP_RESYNC_INTERVAL (4 * 3600)
unsigned long lastNtpSync = 0;
static const char ntpServerName[] = "pool.ntp.org";
uint16_t localPort;
uint8_t ntpUpdateFrequency = 123;  // update the time every x minutes
WiFiUDP Udp;

enum TextJustify { JUSTIFY_LEFT,
                   JUSTIFY_CENTER,
                   JUSTIFY_RIGHT };

void drawJustifiedText(int32_t x, int32_t y, const String &str,
                       TextJustify justify, int32_t rightEdge,
                       int16_t lineSpacing, uint8_t font,
                       uint8_t maxLines);
void configModeCallback(WiFiManager *myWiFiManager);
time_t getNtpTime();                                                              // gets the time from the NTP pool
void sendNTPpacket(IPAddress &address);                                           // asks for the time from the NTP pool
void handle_ClockDisplay();                                                       // handles the clock display and retirement counter
static int daysInMonth(int month, int year);                                      // handles leap years for count
void computeDateDiff(int targetYear, int targetMonth, int targetDay);             // fill globals with results
void handleGRID();                                                                // develope helper for screen positions
void drawCounter();                                                               // draws the countdown/up
String readLineFromFile(const char *filename, int targetLine);                    // reads text from SDCARD
bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap);  // draw blocks
void drawJpegFromSD(const char *filename, int16_t posX, int16_t posY);            // load a jpeg from SDCARD
String SendHTML();
void handle_UpDateSettings();
void handle_OnConnect();
void handle_WifiReset();
void redirectHome();
void updateDisplayData();
void writeSettings();
void readSettings();
void setRGB(bool redLevel, bool greenLevel, bool blueLevel);

void setup() {
  Serial.begin(115200);

  SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);  // init the SDcard
  if (!SD.begin(SD_CS)) {
    Serial.println("SD card init failed!");
  }

  tft.init();  // set up the TFT display (CYD V3 ST7789)
  tft.setRotation(0);
  tft.fillScreen(TFT_BLACK);  // erase the TFT

// Enable if you want to erase SPIFFS, this takes some time!
// then disable and reload sketch to avoid reformatting on every boot!
#ifdef FORMAT_LittleFS
  tft.setTextDatum(MC_DATUM);  // Middle Centre datum
  tft.drawString("Formatting LittleFS, so wait!", 120, 195);
  LittleFS.format();
  delay(2000);
#endif
  //
  if (!LittleFS.begin(true)) {  // get the file system running
    Serial.println("Flash FS initialization failed!");
    while (1) yield();  // Stay here twiddling thumbs waiting
  }
  //Serial.println("Flash FS available!");

  // and now the RGB LED on the back of the CYD
  // Only the green is used
  pinMode(RED_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(BLUE_PIN, OUTPUT);
  setRGB(0, 0, 0);

  //WiFiManager
  //Local intialization.
  WiFiManager wifiManager;
  //AP Configuration
  wifiManager.setHostname("RetireMate");  // sets the custom DNS name that appears in your router
  wifiManager.setAPCallback(configModeCallback);
  //Exit After Config Instead of connecting
  wifiManager.setBreakAfterConfig(true);

  if (!wifiManager.autoConnect("RetireMate")) {
    delay(3000);
    ESP.restart();
    delay(5000);
  } else {
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
    }
    // Seed Random With values Unique To This Device
    uint8_t macAddr[6];
    WiFi.macAddress(macAddr);
    String ipaddress = WiFi.localIP().toString();
    Udp.begin(localPort);  // port to use
    setSyncProvider(getNtpTime);
    //Set Sync Intervals
    setSyncInterval(ntpUpdateFrequency * 60);
  }
  // ... WiFi setup ...
  server.on("/", HTTP_GET, handle_OnConnect);
  server.on("/update", HTTP_GET, handle_UpDateSettings);
  server.on("/wifireset", HTTP_GET, handle_WifiReset);
  server.begin();

  // OTA Setup
  // String hostname(HOSTNAME);
  // WiFi.hostname(hostname);
  ArduinoOTA.setHostname("RetireMate");
  // ArduinoOTA.setPassword((const char *)"12345");
  ArduinoOTA.begin();
  tft.fillScreen(TFT_BLACK);  // erase the TFT

  readSettings();  // go read in the settings we used previously
}

void loop() {
  static time_t prevDisplay = 0;
  timeStatus_t ts = timeStatus();
  utc = now();
  switch (ts) {
    case timeNeedsSync:
    case timeSet:
      //update the schedule checking only if time has changed
      if (now() != prevDisplay) {
        prevDisplay = now();
        handle_ClockDisplay();  // go handle the schedule
        // tmElements_t tm;
        // breakTime(now(), tm);
      }
      break;
    case timeNotSet:
      now();
      delay(3000);
      ESP.restart();
  }
  server.handleClient();
  // Handle OTA update requests, so you can update the firmware via Wifi
  ArduinoOTA.handle();
}

//To Display <Setup> if not connected to AP
void configModeCallback(WiFiManager *myWiFiManager) {
  //Serial.println("Setup");
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(TC_DATUM);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.drawString(String(hostNameCYD), SCREEN_W / 2, SCREEN_H / 2, 4);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextFont(2);  // Use built-in font 2
  tft.drawString("Access Point Active", SCREEN_W / 2, (SCREEN_H / 2) + 36, 2);
  tft.unloadFont();  // Remove font to recover memory
  delay(2000);
}

void handle_ClockDisplay() {
  tmElements_t tm;
  breakTime(now(), tm);
  uint8_t shownHR;  // used for the 12/24hr switch
  char buffer[24];  // holds the formated time/day/date
  //
  // --- Current local time ---
  time_t utc = now();
  time_t localTime = timeZoneRule.toLocal(utc, &tcr);

  myWeekDay = weekday(localTime);
  myHour = hourFormat12(localTime);
  myDay = day(localTime);
  my24Hour = hour(localTime);    // 24-hour clock
  myMinute = minute(localTime);  // use localTime
  mySecond = second(localTime);  // use localTime
  myYear = year(localTime);
  myMonth = month(localTime);  // 1 to 12
  //
  // now we draw the time and the date
  uint8_t xpos = (SCREEN_W / 2) - 1;
  uint8_t ypos = 56;
  tft.setTextColor(TFT_GREEN, TFT_BLACK);  // Now show the clock time
  tft.setTextDatum(BC_DATUM);
  if (show24HR == true) {
    shownHR = my24Hour;
  } else {
    shownHR = myHour;
  }
  if (colonBlink == false) {
    sprintf(buffer, " %2u:%02u ", shownHR, myMinute);  // kill the leading 0 on the hours
  } else {
    sprintf(buffer, " %2u %02u ", shownHR, myMinute);  // kill the leading 0 on the hours
  }
  colonBlink = !colonBlink;                   // toggle the value for next pass
  tft.drawString(buffer, xpos, ypos, FONT7);  // Overwrite the text to clear it
  tft.setTextPadding(0);                      // Reset padding width to none

  if (myDay != lastDay) {  // if the day changes, we update it with new month/date
    String monthStuff;
    struct tm *timeinfo;                   // just to get the current day of the year
    timeinfo = localtime(&localTime);      // shove it in the array
    dayOfTheYear = timeinfo->tm_yday + 1;  // tm_yday is 0-365
    // Serial.print("Day of the year: ");
    // Serial.println(dayOfTheYear);

    tft.setTextDatum(BC_DATUM);  // sets up to display Month, day, year under clock
    tft.fillRect(1, 64, 238, 32, TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);  // now we display the current month/day
    monthStuff = monthNames[myMonth - 1] + " " + String(myDay) + ", " + String(myYear);
    tft.setFreeFont(FS12);                       // Select font
    tft.drawString(monthStuff, 119, 92, GFXFF);  // display the day of the week

    updateDisplayData();  // go show the info
  }
  //
  if (mySecond == 15) {
    drawWiFiQuality();  // go update the wifi strength graphic once a minute
  }

  //handleGRID();  // just a grid to find spacing if you need it

  lastSecond = mySecond;  // save it for next time
  lastMinute = myMinute;  // save this for the next pass
  lastHour = my24Hour;    // save for the next pass
  lastDay = myDay;        // save the day...
  lastYear = myYear;      // save the last year we checked
  lastMonth = myMonth;    // save it
}

void updateDisplayData() {
  computeDateDiff(eventYear, eventMonth, eventDay);  // find the time lapsed
  drawCounter();                                     // go draw the countdown stuff
  defMonth = eventMonth;
  defDay = eventDay;
  defYear = eventYear;
}

void drawCounter() {
  uint8_t xpos = 154;
  uint8_t ypos = 170;
  char buffer[16];                            // holds the formated numbers
  String msg;                                 // holds the SDcard text lines
  String oneLiner;                            // the single line from the file
  tft.fillRect(1, 108, 239, 194, TFT_BLACK);  // this erases the whole data area
  tft.setTextDatum(BC_DATUM);                 // right justified
  switch (g_dateStatus) {
    case -1:                                   // date already happened
      tft.setTextColor(TFT_GREEN, TFT_BLACK);  // we are retired!
      tft.drawString(eventPerson + "'s " + eventType + " Tracker", 119, 136, FONT2);
      break;
    case 0:  // it happened today
      break;
    case 1:                                  // its a countdown to the future
      tft.setTextColor(TFT_RED, TFT_BLACK);  // we are waiting!
      tft.drawString(eventPerson + "'s " + eventType + " Countdown", 119, 136, FONT2);
      break;
  }
  if (g_dateStatus != 0) {                          // we show numbers on anything but actual date
    tft.setTextColor(TFT_WHITE, TFT_BLACK);         //
    tft.setTextDatum(BR_DATUM);                     // right justified
    tft.drawString("YEARS:", xpos, ypos, 4);        // labels
    tft.drawString("MONTHS:", xpos, ypos + 24, 4);  //
    tft.drawString("DAYS:", xpos, ypos + 48, 4);    //
    tft.setTextDatum(MC_DATUM);
    xpos = 188;
    if (g_dateStatus == -1) {
      tft.setTextColor(TFT_GREEN, TFT_BLACK);  // we are retired!

    } else {
      tft.setTextColor(TFT_RED, TFT_BLACK);  // RED cause it's counting down
    }
    tft.setTextDatum(BR_DATUM);                  // formatted bottom right
    sprintf(buffer, "%2d", g_diffYears);         // kill the leading 0 on the years
    tft.drawString(buffer, xpos, ypos, 4);       // labels
    sprintf(buffer, "%2d", g_diffMonths);        // kill the leading 0 on the months
    tft.drawString(buffer, xpos, ypos + 24, 4);  //
    sprintf(buffer, "%2d", g_diffDays);          // kill the leading 0 on the days
    tft.drawString(buffer, xpos, ypos + 48, 4);  //

    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextFont(FONT2);  // Use built-in font 2
    switch (g_dateStatus) {  // -1 date in the past, 0 day of event, 1 date has yet to come
      case -1:               // date is in the past
        if (sdCardAfter == "BLFunny" || sdCardAfter == "BLSerious") {
          ypos = 222;                                                                            // bottom area of the display
          oneLiner = "/" + sdCardAfter + ".txt";                                                 // build the filename to read from
          msg = readLineFromFile(oneLiner.c_str(), dayOfTheYear);                                // and (funny ones) NOTYET.txt
          drawJustifiedText(6, ypos, eventType + " Bucket List", JUSTIFY_CENTER, 236, 2, 2, 4);  // and draw the remark for us
          tft.fillRect(80, ypos + 20, 80, 2, TFT_GREEN);                                         // this erases the whole data area
          drawJustifiedText(6, ypos + 24, msg, JUSTIFY_CENTER, 236, 2, 2, 3);                    // and draw the remark for us
        } else {
          ypos = 228;                                                     // bottom area of the display
          oneLiner = "/" + sdCardAfter + ".txt";                          // build the filename to read from
          msg = readLineFromFile(oneLiner.c_str(), dayOfTheYear);         // and this will be retired/jokes or PUNS
          drawJustifiedText(6, ypos, msg, JUSTIFY_CENTER, 236, 2, 2, 4);  // and draw the remark for us
        }
        break;
      case 1:  // date is in the future
        if (sdCardPrior == "BLFunny" || sdCardPrior == "BLSerious") {
          ypos = 222;                                                                            // bottom area of the display
          oneLiner = "/" + sdCardPrior + ".txt";                                                 // build the filename to read from
          msg = readLineFromFile(oneLiner.c_str(), dayOfTheYear);                                // and (funny ones) NOTYET.txt
          drawJustifiedText(6, ypos, eventType + " Bucket List", JUSTIFY_CENTER, 236, 2, 2, 4);  // and draw the remark for us
          tft.fillRect(80, ypos + 20, 80, 2, TFT_GREEN);                                         // this erases the whole data area
          drawJustifiedText(6, ypos + 24, msg, JUSTIFY_CENTER, 236, 2, 2, 3);                    // and draw the remark for us
        } else {
          ypos = 228;                                                     // bottom area of the display
          oneLiner = "/" + sdCardPrior + ".txt";                          // build the filename to read from
          msg = readLineFromFile(oneLiner.c_str(), dayOfTheYear);         // and this will be retired/jokes or PUNS
          drawJustifiedText(6, ypos, msg, JUSTIFY_CENTER, 236, 2, 2, 4);  // and draw the remark for us
        }
        break;
    }
  } else {  // today is the magical day whatever it is happens
    drawJpegFromSD("/theDayOf.jpg", 35, 109);
  }
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_SKYBLUE, TFT_BLACK);  //
  tft.drawString(String(VERSION) + " @2026 Wabbit Wanch Design", 119, 312, FONT2);
}
//
// ── Helper: days in a given month ────────────────────────────────────────────
static int daysInMonth(int month, int year) {
  static const int dom[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
  if (month == 2 && ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0))
    return 29;
  return dom[month - 1];
}

void computeDateDiff(int targetYear, int targetMonth, int targetDay) {
  // ── 1. Grab current local time ────────────────────────────────────────────
  time_t now = timeZoneRule.toLocal(utc, &tcr);
  tmElements_t tm;
  breakTime(now, tm);

  int curYear = tm.Year + 1970;
  int curMonth = tm.Month;
  int curDay = tm.Day;

  // ── 2. Determine status via pure calendar compare ─────────────────────────
  if (targetYear > curYear) g_dateStatus = 1;
  else if (targetYear < curYear) g_dateStatus = -1;
  else if (targetMonth > curMonth) g_dateStatus = 1;
  else if (targetMonth < curMonth) g_dateStatus = -1;
  else if (targetDay > curDay) g_dateStatus = 1;
  else if (targetDay < curDay) g_dateStatus = -1;
  else g_dateStatus = 0;

  // ── 3. Order so "from" is always the earlier date ─────────────────────────
  int fromY, fromM, fromD, toY, toM, toD;
  if (g_dateStatus >= 0) {
    fromY = curYear;
    fromM = curMonth;
    fromD = curDay;
    toY = targetYear;
    toM = targetMonth;
    toD = targetDay;
  } else {
    fromY = targetYear;
    fromM = targetMonth;
    fromD = targetDay;
    toY = curYear;
    toM = curMonth;
    toD = curDay;
  }

  // ── 4. Calendar difference (years / months / days) ───────────────────────
  g_diffYears = toY - fromY;
  g_diffMonths = toM - fromM;
  g_diffDays = toD - fromD;

  if (g_diffDays < 0) {
    g_diffMonths--;
    g_diffDays += daysInMonth(fromM, fromY);
  }
  if (g_diffMonths < 0) {
    g_diffYears--;
    g_diffMonths += 12;
  }
}

/**
 * Draw a justified, word-wrapped string using TFT_eSPI built-in fonts.
 *
 * @param x           Left boundary (no text ever draws left of this)
 * @param y           Y coordinate (top of the first line)
 * @param str         The string to draw (Arduino String, supports \n)
 * @param justify     JUSTIFY_LEFT, JUSTIFY_CENTER, or JUSTIFY_RIGHT
 * @param rightEdge   Right boundary in pixels (no text ever draws right of this)
 * @param lineSpacing Extra pixels added between lines (0 = font height only)
 * @param font        TFT_eSPI built-in font number (1, 2, 4, 6, 7, or 8)
 * @param maxLines    Maximum lines to draw (0 = no limit)
 *
 * @return            Total pixel height of all lines drawn.
 */
void drawJustifiedText(int32_t x, int32_t y, const String &str,
                       TextJustify justify, int32_t rightEdge,
                       int16_t lineSpacing = 2, uint8_t font = 2,
                       uint8_t maxLines = 0) {
  if (str.length() == 0) return;

  uint8_t originalDatum = tft.getTextDatum();  // save caller's datum
  tft.setTextDatum(TL_DATUM);                  // force top-left for our math
  //
  // fieldWidth is the actual pixel width of the text field
  int32_t fieldWidth = rightEdge - x;
  if (fieldWidth <= 0) return;

  int16_t fontHeight = tft.fontHeight(font);
  int16_t lineStride = fontHeight + lineSpacing;

  const char *p = str.c_str();
  int32_t drawY = y;
  int linesDrawn = 0;
  int totalHeight = 0;

  while (*p != '\0') {

    if (maxLines > 0 && linesDrawn >= maxLines) break;

    char lineBuf[512];
    lineBuf[0] = '\0';
    int lineLen = 0;

    // ----------------------------------------------------------------
    // Build the next display line, one word at a time.
    // A word is never split. If a single word is wider than fieldWidth
    // it is placed on its own line and drawn as-is (overflow visible).
    // ----------------------------------------------------------------
    while (*p != '\0' && *p != '\n') {

      // Skip leading spaces at the start of a new line
      if (lineLen == 0) {
        while (*p == ' ') p++;
        if (*p == '\0' || *p == '\n') break;
      }

      // Locate the end of the next word
      const char *wordStart = p;
      while (*p != '\0' && *p != ' ' && *p != '\n') p++;
      int wordLen = p - wordStart;

      // Build the candidate: current line + space (if needed) + new word
      char candidate[512];
      if (lineLen == 0) {
        snprintf(candidate, sizeof(candidate), "%.*s", wordLen, wordStart);
      } else {
        snprintf(candidate, sizeof(candidate), "%s %.*s", lineBuf, wordLen, wordStart);
      }

      int32_t candidateWidth = tft.textWidth(candidate, font);

      if (lineLen == 0 || candidateWidth <= fieldWidth) {
        // Fits (or it's the first word — never split a word, always
        // accept it even if it alone exceeds fieldWidth)
        strncpy(lineBuf, candidate, sizeof(lineBuf) - 1);
        lineBuf[sizeof(lineBuf) - 1] = '\0';
        lineLen = strlen(lineBuf);

        if (*p == ' ') p++;  // consume the trailing space
      } else {
        // Word doesn't fit — leave p pointing at wordStart and flush
        p = wordStart;
        break;
      }
    }

    // Consume a \n if that's what ended the line
    if (*p == '\n') p++;

    // Draw the line with correct justification inside [x, rightEdge]
    if (lineLen > 0) {
      int32_t strWidth = tft.textWidth(lineBuf, font);
      int32_t drawX = x;  // default: left

      switch (justify) {
        case JUSTIFY_LEFT:
          drawX = x;
          break;
        case JUSTIFY_CENTER:
          drawX = x + (fieldWidth - strWidth) / 2;
          break;
        case JUSTIFY_RIGHT:
          drawX = rightEdge - strWidth;
          break;
      }

      tft.drawString(lineBuf, drawX, drawY, font);
    }

    drawY += lineStride;
    totalHeight += lineStride;
    linesDrawn++;
  }
  tft.setTextDatum(originalDatum);  // restore on the way out
  //return totalHeight;
}
// -------- WIFI Signal Strength ---------
//
void drawWiFiQuality() {
  const byte numBars = 5;                      // set the number of total bars to display
  const byte barWidth = 3;                     // set bar width, height in pixels
  const byte barHeight = 20;                   // should be multiple of numBars, or to indicate zero value
  const byte barSpace = 1;                     // set number of pixels between bars
  const uint16_t barXPosBase = SCREEN_W - 25;  // set the baseline X-pos for drawing the bars
  const byte barYPosBase = 20;                 // set the baseline Y-pos for drawing the bars
  const uint16_t barColor = TFT_YELLOW;
  const uint16_t barBackColor = TFT_DARKGREY;

  int8_t quality = getWifiQuality();

  for (int8_t i = 0; i < numBars; i++) {  // current bar loop
    byte barSpacer = i * barSpace;
    byte tempBarHeight = (barHeight / numBars) * (i + 1);
    for (int8_t j = 0; j < tempBarHeight; j++) {  // draw bar height loop
      for (byte ii = 0; ii < barWidth; ii++) {    // draw bar width loop
        byte nextBarThreshold = (i + 1) * (100 / numBars);
        byte currentBarThreshold = i * (100 / numBars);
        byte currentBarIncrements = (barHeight / numBars) * (i + 1);
        float rangePerBar = (100 / numBars);
        float currentBarStrength;
        if ((quality > currentBarThreshold) && (quality < nextBarThreshold)) {
          currentBarStrength = ((quality - currentBarThreshold) / rangePerBar) * currentBarIncrements;
        } else if (quality >= nextBarThreshold) {
          currentBarStrength = currentBarIncrements;
        } else {
          currentBarStrength = 0;
        }
        if (j < currentBarStrength) {
          tft.drawPixel((barXPosBase + barSpacer + ii) + (barWidth * i), barYPosBase - j, barColor);
        } else {
          tft.drawPixel((barXPosBase + barSpacer + ii) + (barWidth * i), barYPosBase - j, barBackColor);
        }
      }
    }
  }
}
// converts the dBm to a range between 0 and 100%
int8_t getWifiQuality() {
  int32_t dbm = WiFi.RSSI();
  if (dbm <= -100) {
    return 0;
  } else if (dbm >= -50) {
    return 100;
  } else {
    return 2 * (dbm + 100);
  }
}

/*-------- NTP code ----------*/
const int NTP_PACKET_SIZE = 48;      // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE];  //buffer to hold incoming & outgoing packets

time_t getNtpTime() {
  IPAddress timeServerIP;  // time.nist.gov NTP server address

  while (Udp.parsePacket() > 0)
    ;  // discard any previously received packets
  //  Serial.print(F("Transmit NTP Request "));
  //get a random server from the pool
  WiFi.hostByName(ntpServerName, timeServerIP);
  //  Serial.println(timeServerIP);

  sendNTPpacket(timeServerIP);
  uint32_t beginWait = millis();
  while ((millis() - beginWait) < 1500) {
    int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 = (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      return secsSince1900 - 2208988800UL;
    }
  }
  return 0;  // return 0 if unable to get the time
}

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress &address) {
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;  // LI, Version, Mode
  packetBuffer[1] = 0;           // Stratum, or type of clock
  packetBuffer[2] = 6;           // Polling Interval
  packetBuffer[3] = 0xEC;        // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket(address, 123);  //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}

void handleGRID() {
  for (uint32_t myX = 0; myX < SCREEN_W; myX = myX + 32) {
    tft.drawFastVLine(myX, 0, SCREEN_H - 3, TFT_RED);  // Draw a horizontal red line from (10, 20) to (110, 20)
  }
  for (uint32_t myY = 0; myY < SCREEN_H; myY = myY + 32) {
    tft.drawFastHLine(0, myY, SCREEN_W - 3, TFT_RED);  // Draw a horizontal red line from (10, 20) to (110, 20)
  }
}

String readLineFromFile(const char *filename, int targetLine) {
  File file = SD.open(filename);
  if (!file) {
    return "If at first you don't succeed, read the instructions...";
  }

  int currentLine = 1;
  String lineContent = "";

  while (file.available()) {
    char c = file.read();

    if (c == '\n') {
      if (currentLine == targetLine) {
        file.close();
        lineContent.trim();  // Remove \r if present (Windows line endings)
        return lineContent;
      }
      currentLine++;
      lineContent = "";
    } else {
      lineContent += c;
    }
  }

  // Handle last line if it doesn't end with \n
  if (currentLine == targetLine && lineContent.length() > 0) {
    file.close();
    lineContent.trim();
    return lineContent;
  }

  file.close();
  return "If at first you don't succeed, read the instructions...";
}

bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap) {
  // Stop rendering if we've gone past the screen edge
  if (y >= tft.height()) return false;
  tft.pushImage(x, y, w, h, bitmap);
  return true;
}

// ─────────────────────────────────────────────
// Loads a JPEG from SD and draws it at (posX, posY)
//
// filename : full path on SD, e.g. "/images/cat.jpg"
// posX     : top-left X position on display
// posY     : top-left Y position on display
// NOTE: JPEG must be baseline (non-progressive), 24-bit RGB
// Convert with: magick input.jpg -interlace None -type TrueColor output.jpg
// ─────────────────────────────────────────────
void drawJpegFromSD(const char *filename, int16_t posX, int16_t posY) {
  if (!SD.exists(filename)) {
    Serial.printf("[JPEG] File not found: %s\n", filename);
    return;
  }

  // Tell the decoder where to render (offsets the callback coordinates)
  TJpgDec.setJpgScale(1);      // 1 = full size, 2 = half, 4 = quarter, 8 = eighth
  TJpgDec.setSwapBytes(true);  // Required for ESP32 (little-endian → RGB565)
  TJpgDec.setCallback(tft_output);

  // Draw — the decoder opens the file, decodes, and fires the callback
  TJpgDec.drawSdJpg(posX, posY, filename);
}

void handle_OnConnect() {
  // Pass in your current live values here
  server.send(200, "text/html", SendHTML());
}

void handle_WifiReset() {  // this will wipe the WIFI settings
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(TC_DATUM);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.drawString("Wifi reset in 5 seconds", SCREEN_W / 2, SCREEN_H / 2, 2);
  delay(5000);
  redirectHome();
  delay(1000);
  WiFiManager wifiManager;
  wifiManager.resetSettings();  // kill the config for the WIFI
  ESP.restart();                // then kick back into the portal mode
}

void redirectHome() {
  // Send them back to the Root Directory
  server.sendHeader("Location", String("/"), true);
  server.sendHeader("Cache-Control", "no-cache, no-store");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "-1");
  server.send(302, "text/plain", "");
  server.client().stop();
}

void handle_UpDateSettings() {
  if (server.hasArg("action")) {
    if (server.arg("action") == "update") {
      if (server.hasArg("eventPerson")) eventPerson = server.arg("eventPerson");
      if (server.hasArg("eventType")) eventType = server.arg("eventType");
      if (server.hasArg("show24HR")) show24HR = (server.arg("show24HR") == "1");
      else show24HR = false;  // unchecked boxes send nothing

      if (server.hasArg("eventDate")) {
        String d = server.arg("eventDate");  // "YYYY-MM-DD"
        if (d.length() == 10) {
          eventYear = d.substring(0, 4).toInt();
          eventMonth = d.substring(5, 7).toInt();
          eventDay = d.substring(8, 10).toInt();
        }
      }
      if (server.hasArg("msgNamePrior")) {
        sdCardPrior = server.arg("msgNamePrior");
      }
      if (server.hasArg("msgNameAfter")) {
        sdCardAfter = server.arg("msgNameAfter");
      }
      updateDisplayData();  // go update the info
    }
  }
  writeSettings();  // save the new settings for a reboot
  // Re-render with updated values
  handle_OnConnect();
}

String SendHTML() {

  String ptr = "<!DOCTYPE html> <html>\n";
  ptr += "<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n";
  ptr += "<title>Wabbit Wanch</title>\n";
  ptr += "<style>\n";
  ptr += "  body { font-family: Arial, sans-serif; margin: 0; padding: 10px; background: #1a1a2e; color: #eee; }\n";
  ptr += "  h2 { text-align: center; color: #ffffff; margin: 8px 0; font-size: 1.2em; }\n";
  ptr += "  .date-display { text-align: center; color: #aaa; font-size: 0.85em; margin-bottom: 12px; }\n";
  ptr += "  .card { background: #16213e; border-radius: 10px; padding: 12px; margin-bottom: 10px; }\n";
  ptr += "  label { display: block; font-size: 0.8em; color: #a8a8b3; margin-bottom: 3px; }\n";
  ptr += "  input[type=text], input[type=date] { width: 100%; box-sizing: border-box; padding: 8px;\n";
  ptr += "    background: #0f3460; border: 1px solid #00c853; border-radius: 6px;\n";
  ptr += "    color: #eee; font-size: 0.95em; }\n";
  ptr += "  .field-group { margin-bottom: 10px; }\n";
  ptr += "  .checkbox-row { display: flex; align-items: center; justify-content: space-between;\n";
  ptr += "    background: #0f3460; border-radius: 8px; padding: 8px 10px; margin-bottom: 8px; }\n";
  ptr += "  .checkbox-row span { font-size: 0.9em; }\n";
  ptr += "  input[type=checkbox] { width: 20px; height: 20px; accent-color: #e94560; cursor: pointer; }\n";
  ptr += "  .btn { display: block; width: 100%; padding: 11px; border: none; border-radius: 8px;\n";
  ptr += "    font-size: 1em; font-weight: bold; cursor: pointer; margin-bottom: 8px; }\n";
  ptr += "  .btn-update { background: #0f3460; color: #00c853; border: 2px solid #00c853; }\n";
  ptr += "  .btn-wifi   { background: #e94560; color: white; }\n";
  ptr += "  .footer { text-align: center; font-size: 0.75em; color: #555; margin-top: 6px; }\n";
  ptr += "</style></head>\n";

  // ── Body ────────────────────────────────────────────────────────────────────
  ptr += "<body>\n";

  ptr += "<div style=\"position:relative;\">\n";
  ptr += "<form action=\"/wifireset\" method=\"GET\" style=\"position:absolute; top:0; left:0;\">\n";
  ptr += "<button type=\"submit\" style=\"background:none; border:none; font-size:1.5em; cursor:pointer;\">&#128246;</button>\n";
  ptr += "</form>\n";
  ptr += "<h2>&#128048; Event Settings &#128048;</h2>\n";
  ptr += "</div>\n";

  // Current date display
  char dateBuf[32];
  // Month names array baked into the HTML via a simple lookup
  const char *months[] = { "January", "February", "March", "April", "May", "June",
                           "July", "August", "September", "October", "November", "December" };
  String monthName = (myMonth >= 1 && myMonth <= 12) ? months[myMonth - 1] : "???";
  sprintf(dateBuf, "%s %d, %d", monthName.c_str(), myDay, myYear);
  ptr += "<div class=\"date-display\">" + String(dateBuf) + "</div>\n";

  ptr += "<form action=\"/update\" method=\"GET\">\n";  // ← open BEFORE the card divs

  // ── Text fields card ────────────────────────────────────────────────────────
  ptr += "<div class=\"card\">\n";

  ptr += "<div class=\"field-group\">\n";
  ptr += "<label for=\"eventPerson\">Event Person</label>\n";
  ptr += "<input type=\"text\" id=\"eventPerson\" name=\"eventPerson\" maxlength=\"32\" value=\"" + eventPerson + "\">\n";
  ptr += "</div>\n";

  ptr += "<div class=\"field-group\">\n";
  ptr += "<label for=\"eventType\">Event Type</label>\n";
  ptr += "<input type=\"text\" id=\"eventType\" name=\"eventType\" maxlength=\"32\" value=\"" + eventType + "\">\n";
  ptr += "</div>\n";

  ptr += "<div class=\"field-group\">\n";
  ptr += "<label for=\"msgNamePrior\">Prior Event Quote:</label>\n";
  ptr += "<select name=\"msgNamePrior\" id=\"msgNamePrior\" style=\"width:100%; box-sizing:border-box; padding:8px; background:#0f3460; border:2px solid #00c853; border-radius:6px; color:#eee; font-size:0.95em;\">\n";
  ptr += "<option value=\"BLFunny\"" + String(sdCardPrior == "BLFunny" ? " selected" : "") + ">BLFunny</option>\n";
  ptr += "<option value=\"BLSerious\"" + String(sdCardPrior == "BLSerious" ? " selected" : "") + ">BLSerious</option>\n";
  ptr += "<option value=\"Jokes\"" + String(sdCardPrior == "Jokes" ? " selected" : "") + ">Jokes</option>\n";
  ptr += "<option value=\"Puns\"" + String(sdCardPrior == "Puns" ? " selected" : "") + ">Puns</option>\n";
  ptr += "<option value=\"Retired\"" + String(sdCardPrior == "Retired" ? " selected" : "") + ">Retired</option>\n";
  ptr += "</select>\n";
  ptr += "</div>\n";

  // Date field with default values pre-filled as YYYY-MM-DD
  char defaultDate[22];
  sprintf(defaultDate, "%04d-%02d-%02d", defYear, defMonth, defDay);
  ptr += "<div class=\"field-group\">\n";
  ptr += "<label for=\"eventDate\">Event Date</label>\n";
  ptr += "<input type=\"date\" id=\"eventDate\" name=\"eventDate\" value=\"" + String(defaultDate) + "\">\n";
  ptr += "</div>\n";

  ptr += "<div class=\"field-group\">\n";
  ptr += "<label for=\"msgNameAfter\">After Event Quote:</label>\n";
  ptr += "<select name=\"msgNameAfter\" id=\"msgNameAfter\" style=\"width:100%; box-sizing:border-box; padding:8px; background:#0f3460; border:2px solid #00c853; border-radius:6px; color:#eee; font-size:0.95em;\">\n";
  ptr += "<option value=\"BLFunny\"" + String(sdCardAfter == "BLFunny" ? " selected" : "") + ">BLFunny</option>\n";
  ptr += "<option value=\"BLSerious\"" + String(sdCardAfter == "BLSerious" ? " selected" : "") + ">BLSerious</option>\n";
  ptr += "<option value=\"Jokes\"" + String(sdCardAfter == "Jokes" ? " selected" : "") + ">Jokes</option>\n";
  ptr += "<option value=\"Puns\"" + String(sdCardAfter == "Puns" ? " selected" : "") + ">Puns</option>\n";
  ptr += "<option value=\"Retired\"" + String(sdCardAfter == "Retired" ? " selected" : "") + ">Retired</option>\n";
  ptr += "</select>\n";
  ptr += "</div>\n";

  ptr += "</div>\n";  // end card

  // ── Checkboxes card ─────────────────────────────────────────────────────────
  ptr += "<div class=\"card\">\n";

  ptr += "<div class=\"checkbox-row\">\n";
  ptr += "<span>24-Hour Clock</span>\n";
  ptr += "<input type=\"checkbox\" id=\"show24HR\" name=\"show24HR\" value=\"1\"";
  if (show24HR) ptr += " checked";
  ptr += ">\n";
  ptr += "</div>\n";

  ptr += "</div>\n";  // end card

  // ── Buttons ─────────────────────────────────────────────────────────────────
  // ptr += "<form action=\"/update\" method=\"GET\">\n";
  ptr += "<button type=\"submit\" name=\"action\" value=\"update\" class=\"btn btn-update\">&#128190; Update Settings</button>\n";
  ptr += "</form>\n";

  ptr += "<form action=\"https://github.com/wabbitguy\" method=\"GET\" target=\"_blank\">\n";
  ptr += "<button type=\"submit\" class=\"btn btn-github\">&#128279; Wabbit Wanch Design GitHub</button>\n";
  ptr += "</form>\n";

  // ptr += "<br><br>";
  // ptr += "<form action=\"/wifireset\" method=\"GET\">\n";
  // ptr += "<button type=\"submit\" class=\"btn btn-wifi\">&#128246; WiFi Reset</button>\n";
  // ptr += "</form>\n";
  // ── Footer ──────────────────────────────────────────────────────────────────
  ptr += "<div class=\"footer\">&copy; 2026 Wabbit Wanch Design</div>\n";

  ptr += "</body></html>\n";
  return ptr;
}

void writeSettings() {
  // Save decoded message to SPIFFS file for playback on power up.
  File f = LittleFS.open(config, "w");
  if (!f) {
    Serial.println("File open failed!");
  } else {
    //Serial.println("Saving settings now...");
    f.println("eventPerson=" + eventPerson);
    f.println("eventType=" + eventType);
    f.println("eventYear=" + String(eventYear));
    f.println("eventMonth=" + String(eventMonth));
    f.println("eventDay=" + String(eventDay));
    f.println("sdCardPrior=" + sdCardPrior);
    f.println("sdCardAfter=" + sdCardAfter);
    f.println("show24HR=" + String(show24HR));
    f.close();
  }
  //readSettings();
}

void readSettings() {
  if (LittleFS.exists(config) == false) {
    //Serial.println("Settings File does not yet exist");
    writeSettings();
    return;
  }
  File fr = LittleFS.open(config, "r");
  String line;
  while (fr.available()) {
    line = fr.readStringUntil('\n');
    if (line.indexOf("eventPerson=") >= 0) {
      eventPerson = line.substring(line.lastIndexOf("eventPerson=") + 12);
      eventPerson.trim();
      //Serial.println("eventPerson: " + eventPerson);
    }
    if (line.indexOf("eventType=") >= 0) {
      eventType = line.substring(line.lastIndexOf("eventType=") + 10);
      eventType.trim();
      //Serial.println("eventType: " + eventType);
    }
    if (line.indexOf("eventYear=") >= 0) {
      eventYear = line.substring(line.lastIndexOf("eventYear=") + 10).toInt();
      //Serial.println("eventYear: " + String(eventYear));
    }
    if (line.indexOf("eventMonth=") >= 0) {
      eventMonth = line.substring(line.lastIndexOf("eventMonth=") + 11).toInt();
      //Serial.println("eventMonth: " + String(eventMonth));
    }
    if (line.indexOf("sdCardPrior=") >= 0) {
      sdCardPrior = line.substring(line.lastIndexOf("sdCardPrior=") + 12);
      sdCardPrior.trim();
      //Serial.println("sdCardPrior: " + sdCardPrior);
    }
    if (line.indexOf("sdCardAfter=") >= 0) {
      sdCardAfter = line.substring(line.lastIndexOf("sdCardAfter=") + 12);
      sdCardAfter.trim();
      //Serial.println("sdCardAfter: " + sdCardAfter);
    }
    if (line.indexOf("show24HR=") >= 0) {
      show24HR = line.substring(line.lastIndexOf("show24HR=") + 9).toInt();
      //Serial.println("show24HR: " + String(show24HR));
    }
  }
  fr.close();
}

void setRGB(bool redLevel, bool greenLevel, bool blueLevel) {
  digitalWrite(RED_PIN, !redLevel);
  digitalWrite(GREEN_PIN, !greenLevel);
  digitalWrite(BLUE_PIN, !blueLevel);
}