# CALE E-ink calendar

A very easy and straight-forward Eink calendar. Please visit [CALE T5 firmware page](https://cale.es/firmware-t5) to read more details about this branch.

### Build 

This Firmware reads MP3 Files stored on ESP32 Spiffs. Please don't forget to run:

    pio run --target uploadfs

In order to upload the /data directory to the Espressif chip spiffs partition. Feel free to update the MP3's to change the voices or make your own forks of this repository.

### Our approach to make an easy E-Ink calendar

- A screenshot to BMP endpoint that prints a webpage with the contents you need displayed on Eink
- The Firmware driving the Eink display will wake up every morning or every 2 hours and read this screenshot. Then it will stay in deep sleep mode, consuming 1 miliamper from the battery, until it wakes up again and repeats the loop. 
- [CALE.es](https://cale.es) will prepare for you a bitmap url so you can simply copy and paste it in Config.h configuration file


**Most important part of the configuration:**

    char screenUrl[] = "http://img.cale.es/bmp/USERNAME/SCREEN_ID";
    
    // Security setting, leave empty if your screen is publis
    String bearer = "";

Note that we don't recommend to use public screens since your calendar may contain private information like events, transfer or doctor appointments that you should not open to the world to see. So use always a security token.

This token is sent in the headers like:

Authorization: Bearer YOUR_TOKEN

And passed to cale.es that verifies that your user owns this screen and also that the token matches the one that is stored on our servers.

### Schematics

Please check http://cale.es for the up-to-date Schematic. The T5 is already all wired up so only uploading the Firmware is necessary.

### Build logs and detailed instructions

![CALE in Hackaday](https://hackaday.io/project/169086-cale-low-energy-eink-wallpaper) Please follow the project there to get updates and more detailed build instructions.

### Support CALE

There are commercial solutions alike and they start up to 500€ for a Eink syncronized calendar (Check getjoan.com)
If you use this commercially in your office we want to ask you about a small donation and to send us a short history of how it's working so we can give support. Please also file a Github issue in case you find a bug with detailed instructions so we can reproduce it in our end. 
