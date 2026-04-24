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
#include <WebServer.h>
#include <FS.h>
#include "LittleFS.h"

//Australia Eastern Time Zone (Sydney, Melbourne)
TimeChangeRule aEDT = { "AEDT", First, Sun, Oct, 2, 660 };  //UTC + 11 hours
TimeChangeRule aEST = { "AEST", First, Sun, Apr, 3, 600 };  //UTC + 10 hours
//Timezone timeZoneRule(aEDT, aEST);

//Central European Time (Frankfurt, Paris)
TimeChangeRule CEST = { "CEST", Last, Sun, Mar, 2, 120 };  //Central European Summer Time
TimeChangeRule CET = { "CET ", Last, Sun, Oct, 3, 60 };    //Central European Standard Time
//Timezone timeZoneRule(CEST, CET);

//United Kingdom (London, Belfast)
TimeChangeRule BST = { "BST", Last, Sun, Mar, 1, 60 };  //British Summer Time
TimeChangeRule GMT = { "GMT", Last, Sun, Oct, 2, 0 };   //Standard Time
//Timezone timeZoneRule(BST, GMT);

//US Eastern Time Zone (New York, Detroit)
TimeChangeRule usEDT = { "EDT", Second, Sun, Mar, 2, -240 };  //Eastern Daylight Time = UTC - 4 hours
TimeChangeRule usEST = { "EST", First, Sun, Nov, 2, -300 };   //Eastern Standard Time = UTC - 5 hours
//Timezone timeZoneRule(usEDT, usEST);

//US Central Time Zone (Chicago, Houston)
TimeChangeRule usCDT = { "CDT", Second, dowSunday, Mar, 2, -300 };
TimeChangeRule usCST = { "CST", First, dowSunday, Nov, 2, -360 };
//Timezone timeZoneRule(usCDT, usCST);

//US Mountain Time Zone (Denver, Salt Lake City)
TimeChangeRule usMDT = { "MDT", Second, dowSunday, Mar, 2, -360 };
TimeChangeRule usMST = { "MST", First, dowSunday, Nov, 2, -420 };
//Timezone timeZoneRule(usMDT, usMST);

//Arizona is US Mountain Time Zone but does not use DST
//Timezone timeZoneRule(usMST, usMST);

//US Pacific Time Zone (Las Vegas, Los Angeles)
TimeChangeRule usPDT = { "PDT", Second, dowSunday, Mar, 2, -420 };
TimeChangeRule usPST = { "PST", First, dowSunday, Nov, 2, -480 };
//Timezone timeZoneRule(usPDT, usPST);
//
// ---------------- TIME HELPER ----------------
//TimeZone Settings Details https://github.com/JChristensen/Timezone
//BC Canada Pacific Standard Time always
TimeChangeRule bcPDT = { "PDT", Second, dowSunday, Mar, 2, -420 };  // 7 hour offset - BC, Canada

//Edit the info in the (xxx,xxx) according To Your Timezone and Daylight Saving Time
// for example if in the US pacific northwest you'd use
// Timezone timeZoneRule(usPDT, usPST);
// Examples are provided above for various timezone settings
//
Timezone timeZoneRule(bcPDT, bcPDT);

//Pointer To The Time Change Rule, Use to Get The TZ Abbrev
TimeChangeRule *tcr;
time_t utc;

// -- Event Type and Date --
String eventPerson = "Bullwinkle";  // who the event is for
String eventType = "Retirement";    // type of event
uint16_t eventYear = 1964;
uint16_t eventMonth = 6;
uint16_t eventDay = 27;
String sdCardPrior = "BLFunny";// prior to the set date
String sdCardAfter = "Retired";// after the set date
//
// filenames for the messages are BLFunny.txt, BLSerious.txt
// Jokes.txt, PUNS.txt, Retired.txt
// each line is around 120 characters max, 366 lines minimum
//
// for a future retirement date, you can use a graphic, JPEG
// 170x170 pixels. No compression or smoothing
// store it on the SDcard as "theDayOf.jpg"
//