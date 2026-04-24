# RetireMate
Retirement countdown, day of graphic, tracker after event date

A friend suggested a retirement countdown application and I thought it would be a great idea for a CYD (cheap yellow display). With some changes you could make it for anniversaries, birthdays or whatever. A small microSD card is required to hold the messages and a single graphic (theDayOf.jpg).

The code is compiled with the Arduino IDE 2.3.8 (April 2026). The settings.h file has a list of all the libaries that are required to compile the code.

<h3>CYD Board</h3>

There are at least three of 2.8" CYD boards, the one I am using is based on the ST7789. An earlier one was based on a ILI9431. Thus if you haven't bought the board yet, ensure you get one with the ST7789 driver for the TFT. The code base here has the User_Setup.h file for the TFT_eSPI library so you don't need to create/edit one yourself.

<img width="256" height="450" alt="IMG_7958" src="https://github.com/user-attachments/assets/e60c8d34-7fe3-47de-8e30-6d696157856f" /><br>

For programming the CYD, I used the Micro USB connector. The USB C connector didn't seem to do anything. According to things I've read there are some missing resistors that require a USB A to USB C cable, which I used and the port still does nothing. Keep in mind the CYD I used is the ST7789 driver whereas most referenced one prior uses the ILI9341 driver.

<h3>Using RetireMate</h3>

The only code change to make is in the TimeZone setting in the settings.h file. Use it to match your timezone. The default values RetireMate uses can be configured from any web browser (it was designed with an iPhone sized editing page), thus the code doesn't need any editing unless you really want to.

To start take the 6 files in the SDCard_Items folder and copy those to a FAT32 formatted microSD card. You don't need more than a 2GB sdCARD. I used a 4GB one that was in my spare parts box. Serious overkill.

<h4>For Arduino IDE settings use:</h4>

<img width="368" height="450" alt="ESP32_CYD_Settings" src="https://github.com/user-attachments/assets/2816a4c7-2465-4255-ba76-cb3c86cf8a86" /><br>

After uploading the code, the board with go into a typical Captive Portal. Use your phone wifi to look for "RetireMate". When it shows up it's the usual "pick" your wifi router from the list, type in the password, save and the board will reboot and come up with the default values.

Ensure after the reboot the time is correct for your timezone.

<h3>Personalizing RetireMate</h3>

Use a web browser and type in RetireMate.local to bring up the web page where you can set all the values you want.

<img width="208" height="450" alt="IMG_7953" src="https://github.com/user-attachments/assets/a3beb688-e63b-4178-b2a9-cbc021eae4c4" /><br>

First off the persons name, the type of event (like retirement). These will be shown on the display as "Person's Event Countdown". Below that will be the years, months and days til the event occurs.

<h4>Next is Prior Event Quote selections:</h4>

<ul>
  <li>BLFunny - Bucket List of Goofy Entries</li>
  <li>BLSerious - Bucket List of Serious Goals</li>
  <li>Jokes - Exactly That</li>
  <li>PUNS - You find them punny or not</li>
  <li>Retired - Wit and Wisdom after retirement (and some humour)</li>
</ul>

You can have whatever you want leading up to the day of the event, and then a completely different set after the event.

Date of Event - you can make it pretty much any day you want. Well I haven't tested it outside the 20th century...

Clock display can be 12hr or 24hr format.

Update the settings and the job is mostly done...

<h3>Personal Useage or Gift</h3>

If you're going to use this unit for yourself, you're done. Each day there will be a new "one liner" from the file type you've chosen. There are 366 one liners in each file, one for every day of the year.

Should you want to gift this to someone, you can set it up for them in advance. When you have it displaying what you want, you'll notice a little WIFI icon at the top right of the display. If you click on this icon the wifi will erase its previously saved connection point and go back into captive portal. It will wait for 5 seconds before doing this should you want to abort and pull out the USB cable that's powering the display.

<h3>Customization Txt Files</h3>

RetireMate allows a signifcant amount of customization if you want to work with a specific topic/event. In the firmware there are hardcoded filenames of five text files on the sdCard. The filenames are BLFunny.txt, BLSerious.txt, Jokes.txt, PUNS.txt, and Retired.txt. While you can customize filenames it's not a straight forward thing to do because both the HTML section and the parser have to be changed to recognize different filenames. None the less, it can be done.

The .txt files themselves are plain text files with a UNIX style LF (linefeed marking the end of each line). Thus not the standard Windows CRLF (carriage return/linefeed). Thus make sure your text editor just uses a linefeed to end each line. I used BBEdit on the Mac to create these files.

The line length depends on the file. The BL files use much shorter line lengths. The PUNS, JOKES and Retired lines can handle around 120 characters (about 4 lines). If you get too long of a line, it'll be clipped.

<h3>Customization Graphic</h3>

For the actual day of the "event", there will be no text shown, instead the firmware will display a file called theDayOf.jpg. Note the upper and lower case in that filename because it's very specific. The JPG itself needs to be 170 x 170 pixels MAXIMUM size, and NO progressive stuff so don't use any compression settings in the JPG. You can easily set the day of the event to the current day to see if your graphic will display correctly.

