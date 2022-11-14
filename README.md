![CALE Logo](/screenshot/cale-logo.svg)

# CALE E-ink calendar

This is based on esp32-arduino framework. There is an ESP-IDF framework version in [Cale-idf repository](https://github.com/martinberlin/cale-idf).

IMPORTANT: Since 2 years we are developing our own epaper component for ESP32 called [CalEPD](https://github.com/martinberlin/CalEPD). Because of our limited time and resources, this repository is falling behind for new updates, so we recommend that you try our new component.

We know some developers are reluctant to move to a new framework. But we find ESP-IDF the best solution to build and flash our ESP32's and we are confident that you will find **CalEPD** a faster component, way easier and cleaner, than GxEPD.

### VSCODE and Platformio ★

In the repository [cale-platformio](https://github.com/martinberlin/cale-platformio) you can have a quick start skeleton to use CalEPD and Adafruit-GFX components, along with optional FocalTech touch I2C. Please be aware that there are some corrections to do by hand until we figure out what is the best way to do it. Read those in the WiKi and please give a **★ to the cale-platformio** repository if you find it useful


### A very easy and straight-forward E-Ink calendar

The Android app that sends the configuration to the ESP32 Firmware using Bluetooth is now oficially the default way to set up your display. But we will always keep the possibility to have also a version that uses a [hardcoded C configuration](https://github.com/martinberlin/eink-calendar/tree/cale) since there are many use cases where the user does not have Android, or just wants to have a configuration like this per se, keeping in mind that ESP8266 has no bluetooth.

The original version and hackaday project is moved to the [legacy branch](https://github.com/martinberlin/eink-calendar/tree/legacy)

#### Branch reference

**cale** Use this branch is you need to use ESP8266 as the display controller (No bluetooth, config is hardcoded in Config.h)

**cale_tft** Version only as a working demo for TFT displays

**cale_t5** T5 & T5S are small 2.13 or 2.7 inches ESP32 small Epaper displays from TTGO

**cale_ble** Bluetooth research branch (beta)

**cale_spi_config** Version uses a GxEPD fork that enables you to define in platformio.ini what pins you want to use for SPI communication

Any other branch is internal for us and will probably be deleted in the future. 

### What the Firmware actually does

Only 4 things: 

1. In case of no WiFi config, opens Bluetooth and waits for configuration, please send it [installing CALE Android app](https://play.google.com/store/apps/details?id=io.cordova.cale)
2. Will connect to [cale.es](http://cale.es) and query Service times (That responds with a boolean response: 0 or 1) 
3. If the response is 1, then it will download a Screen bitmap image and render it in your E-ink, if not goes directly to point 4 and sleeps till next wake-up
4. Will go to sleep the amount of seconds defined in Config and return to point 2. Only if wakes up and does not find a WiFi will return to 1

Please read the short instructions on [configuring the Firmware over Bluetooth](https://cale.es/firmware-bluetooth) to understand how it works and get alternative links to download the Android app. If you don't use Android is no problem, just use the **cale branch** with [hardcoded C configuration](https://github.com/martinberlin/eink-calendar/tree/cale). 

### Our approach to make an easy E-Ink calendar

- A screenshot to BMP endpoint that prints a webpage with the contents of your Screen (This does for you CALE)
- The Firmware driving the Eink display will wake up every hour and reads this screenshot only if it's in the Service times you define in CALE (Ex. Mon->Fri from 6 to 18 hours). Then it will stay in deep sleep mode, consuming less than 1 miliamper from the battery, until it wakes up again and repeats the loop. 

The goal is to build a dynamic calendar that is easy to install and has the lowest consumption as possible.
We could reach a minimum consumption of 0.08 mA/hr using ESP32 [Tinypico](https://www.tinypico.com) please uncomment the Config.h if you use one:

    // #define TINYPICO

We highly recomment to use TinyPICO or a similar ESP32 board that allows having a deepsleep consumption of 0.1 mA or less if you want your battery powered display to work for long without being recharged.
We don't provide any lifetime estimations since everyone can decide how often it refreshes and what battery to use, but we expect that even refreshing many times a day, will stand at least 1 month working with a fully charged 2000 mA/hr 3.7 Lion battery.
Our goal is to build a product that can do it's job and stay out of the way, that needs no mainteinance and has a large battery lifespan.

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


Amount of seconds that the ESP32 will deepsleep:

    uint64_t DEEPSLEEP_SECONDS = 3600*1;

### ESP-IDF version

[Cale Firmware for ESP-IDF](https://github.com/martinberlin/cale-idf) No bluetooth config with our own app so far.
But there is a [WiFi provisioning example using ESP-RainMaker](https://github.com/martinberlin/cale-idf/tree/feature/42-wifi-provisioning) (needs PSRAM). This is a version thought for Espressif IDF Framework users more geared to developers that want to make this Firmware their own or even implement only our epaper component and make their own program.
In the latest months most of our development efforts land in the Espressif IDF framework version of the Firmware.

### Support CALE

There are commercial solutions alike and they start up to 560€ for a Eink syncronized calendar (Check getjoan.com)
If you like this Firmware please consider becoming a sponsor where you can donate as little as 2 u$ per month. Just click on:
**❤ Sponsor**  on the top right

♢ For cryptocurrency users is also possible to help this project transferring Ethereum:

    0x65B7EF685E5B493603740310A84268c6D59f58B5

We are thankful for the support and contributions so far!
For CALE project I really need to give support but also most importantly to get new epapers so I can test them and add new features. 
