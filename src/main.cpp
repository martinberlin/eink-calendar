#include <Config.h>
#include <SPI.h>
#include <GxEPD.h>
#include <GxGDEW027W3/GxGDEW027W3.h>
#include <GxIO/GxIO_SPI/GxIO_SPI.cpp>
#include <GxIO/GxIO.cpp>
// FONT used for title / message body
//Converting fonts with ümlauts: ./fontconvert *.ttf 18 32 252
#include <Fonts/FreeMonoBold9pt7b.h>
#include <Fonts/FreeMonoBold18pt7b.h>

void loop(){}

void setup() {
  Serial.begin(115200);

   int mStart = millis();
   printf("Heap start:%d\n",ESP.getFreeHeap());
   // SPI interface GPIOs defined in Config.h  
GxIO_Class io(SPI, EINK_CS, EINK_DC, EINK_RST);
// (GxIO& io, uint8_t rst = D4, uint8_t busy = D2);
GxEPD_Class display(io, EINK_RST, EINK_BUSY );
   display.init();
   printf("Heap display instantiation:%d\n",ESP.getFreeHeap());
   display.setRotation(1);
   display.fillScreen(GxEPD_WHITE);  // GxEPD_BLACK  GxEPD_WHITE
   
   
   display.setFont(&FreeMonoBold9pt7b);
   display.setTextColor(GxEPD_BLACK);
   display.setCursor(20,20);

// Print all character from an Adafruit Font
  if (false) {
   for (int i = 40; i <= 126; i++) {
      display.write(i); // Needs to be >32 (first character definition)
   }
   }
   // Test fonts
   display.println("CALE-IDF");  // Todo: Add print and println
   display.setFont(&FreeMonoBold18pt7b);
   display.setCursor(10,40);
   display.println("Te quiero");
   display.setCursor(10,100);
   display.println("MACHO");
   display.update();
   Serial.printf("millis rendering: %d\n", millis()-mStart);
}