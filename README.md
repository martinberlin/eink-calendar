![CALE Logo](/screenshot/cale-logo.svg)

# CALE E-ink calendar

### A very easy and straight-forward E-Ink calendar

The original version and hackaday project is moved to the [legacy branch](https://github.com/martinberlin/eink-calendar/tree/legacy)

**cale_ble** Bluetooth research branch

**cale_ble_v2** Check this branch for the latest Bluetooth version where we [used char instead of String to generate the WiFi Client request](https://github.com/martinberlin/eink-calendar/commit/5586b062370ac2390ecd5474a56634984e49480f)
and so far seems to be a better candidate than the first try. 

**The Bluetooth version of CALE does this 3 things only:**

1. In case of no WiFi config, opens Bluetooth and waits for configuration, please send it [installing CALE Android app](https://github.com/martinberlin/cale-app/tree/master/releases)
2. Will connect to [cale.es](http://cale.es) and grab a dynamic rendered BMP, create an account if you want to have a ready made web-service, that renders BMPs ready for your displays
3. Will go to sleep the amount of seconds defined in Config and return to point 1

Please read the short instructions on [configuring the Firmware over Bluetooth](https://cale.es/firmware-bluetooth) to understand how it works.

### Our approach to make an easy E-Ink calendar

- A screenshot to BMP endpoint that prints a webpage with the contents you need displayed on Eink (This does for you CALE)
- The Firmware driving the Eink display will wake up every morning or every 2 hours and reads this screenshot. Then it will stay in deep sleep mode, consuming less than 1 miliamper from the battery, until it wakes up again and repeats the loop. 

The goal is to build a dynamic calendar that is easy to install and has the lowest consumption as possible.
We could reach a minimum consumption of 0.08 mA/hr using ESP32 [Tinypico](https://www.tinypico.com) please check the branch: 
**cale_tinypico**

Where we implemented the TinyPICO helper library to shut down the DotStar Led and the data lines to reduce the consumption to the minimum. Currently this was is lowest consumption record we could achieve with an ESP32.

For the rest of contents please refer to the master branch since it takes a long time to update the README files across branches and does not make too much sense.

### Support CALE

There are commercial solutions alike and they start up to 560€ for a Eink syncronized calendar (Check getjoan.com)
If you use this commercially in your office we want to ask you about a small donation and to send us a short history of how it's working so we can give support. Please also file a Github issue in case you find a bug with detailed instructions so we can reproduce it in our end. 
