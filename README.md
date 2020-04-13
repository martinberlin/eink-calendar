![CALE Logo](/screenshot/cale-logo.svg)

# CALE E-ink calendar

### A very easy and straight-forward E-Ink calendar

The Android app that sends the configuration to the ESP32 Firmware using Bluetooth is now oficially the default way to set up your display. But we will always keep the possibility to have also a version that uses a [hardcoded C configuration](https://github.com/martinberlin/eink-calendar/tree/cale) since there are many use cases where the user does not have Android, or just wants to have a configuration like this per se, keeping in mind that ESP8266 has no bluetooth.

The original version and hackaday project is moved to the [legacy branch](https://github.com/martinberlin/eink-calendar/tree/legacy)

**cale_ble** Bluetooth research branch

### What the Firmware actually does

Only 4 things: 

1. In case of no WiFi config, opens Bluetooth and waits for configuration, please send it [installing CALE Android app](https://play.google.com/store/apps/details?id=io.cordova.cale)
2. Will connect to [cale.es](http://cale.es) and check receive a Service times response (0 or 1) 
3. If the response is 1, then it will download a Screen bitmap image and render it in your E-ink, if not goes directly to point 4 and sleeps till next wake-up
4. Will go to sleep the amount of seconds defined in Config and return to point 2. Only if wakes up and does not find a WiFi will return to 1

Please read the short instructions on [configuring the Firmware over Bluetooth](https://cale.es/firmware-bluetooth) to understand how it works and get alternative links to download the Android app. If you don't use Android is no problem, just use the **cale branch** with [hardcoded C configuration](https://github.com/martinberlin/eink-calendar/tree/cale). 

### Our approach to make an easy E-Ink calendar

- A screenshot to BMP endpoint that prints a webpage with the contents you need displayed on Eink (This does for you CALE)
- The Firmware driving the Eink display will wake up every hour and reads this screenshot only if it's in the Service times you define in CALE (Ex. Mon->Fri from 6 to 18 hours). Then it will stay in deep sleep mode, consuming less than 1 miliamper from the battery, until it wakes up again and repeats the loop. 

The goal is to build a dynamic calendar that is easy to install and has the lowest consumption as possible.
We could reach a minimum consumption of 0.08 mA/hr using ESP32 [Tinypico](https://www.tinypico.com) please check the branch: 
**cale_tinypico**

In that branch we implemented the TinyPICO helper library to shut down the DotStar Led and the data lines to reduce the consumption to the minimum. Currently this was is lowest consumption record we could achieve with an ESP32.

For the rest of contents please refer to the master branch since it takes a long time to update the README files across branches and does not make too much sense.

### C configuration

Just rename lib/Config/Config.h.dist to Config.h

First of all configure what GPIOs go out from ESP32 to the Eink display:

    // Example gpios Config for ESP32
    int8_t EINK_CS = 5;
    int8_t EINK_DC = 22;
    int8_t EINK_RST = 21; 
    int8_t EINK_BUSY = 4; 
    // Keep in mind that this 2 important Gpios for SPI are not configurable
    // Eink SPI DIN pin -> ESP32 MOSI  (23) 
    // Eink SPI CLK pin -> ESP32 CLOCK (18)

and then what Eink model you have uncommenting one of this lines:

    #define GDEW075T7     // Waveshare 7.5" v2 800*480 -> ex. this gxEPD class will be used
    //#define GDEW075T8   // Waveshare 7.5" v1 640*383
    //#define GDEW075Z09   // 7.5" b/w/r 640x383
    //#define GDEW075Z08   // 7.5" b/w/r 800x480
    //#define GDE0213B1
    //#define GDEW0154Z04 // Controller: IL0376F
    //#define GDEW0154Z17   // 1.54" b/w
    //#define GDE0213B1    // 2.13" b/w
    //#define GDEH0213B72  // 2.13" b/w new panel
    //#define GDEW0213Z16  // 2.13" b/w/r
    //#define GDEH029A1    // 2.9" b/w
    //#define GDEW029T5    // 2.9" b/w IL0373
    //#define GDEW029Z10   // 2.9" b/w/r
    //#define GDEW026T0    // 2.6" b/w
    //#define GDEW027C44   // 2.7" b/w/r
    //#define GDEW027W3    // 2.7" b/w
    //#define GDEW0371W7   // 3.7" b/w
    //#define GDEW042Z15   // 4.2" b/w/r

If your class is not here, before giving up, please check if it's not in [gxEPD library](https://github.com/ZinggJM/GxEPD) and add it following our model in main.cpp 

Deepsleep is one of the most important features to be able to have a display powered by 3.7v Lion batteries.
If you want to enable deepsleep to power your calendar with batteries, then uncomment the line:

    //#define DEEPSLEEP_ENABLED

Amount of seconds that the ESP32 will deepsleep:

    uint64_t DEEPSLEEP_SECONDS = 3600*1;

 This is useful in case you customize the firmware to do something more after rendering. Check loop() to see how is implemented:

    #define SLEEP_AFTER_SECONDS 20 

// When it reaches this number your credentials stored on Non-volatile storage on ESP32 processor are deleted
// Put this to a high number after you get it working correctly:
#define RESTART_TIMES_BEFORE_CREDENTIALS_RESET 500


### Support CALE

There are commercial solutions alike and they start up to 560€ for a Eink syncronized calendar (Check getjoan.com)
If you use this commercially in your office we want to ask you about a small donation and to send us a short history of how it's working so we can give support. Please also file a Github issue in case you find a bug with detailed instructions so we can reproduce it in our end. 
