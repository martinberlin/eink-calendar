#include <Config.h>
#ifdef ESP32
  #include <WiFi.h>
  #include <ESPmDNS.h>
#elif ESP8266
  #include <ESP8266WiFi.h>
  #include <ESP8266mDNS.h>
  #include <DNSServer.h>
#endif
#include <WiFiClient.h>
#include <SPI.h>
#include <GxEPD.h>

// Get the right interface for the display
#ifdef GDEW042T2
  #include <GxGDEW042T2/GxGDEW042T2.h>
  #elif GDEW075T8
  #include <GxGDEW075T8/GxGDEW075T8.h>
  #elif GDEW075T7
  #include <GxGDEW075T7/GxGDEW075T7.h> 
  #elif GDEW0213I5F
  #include <GxGDEW0213I5F/GxGDEW0213I5F.h>
  #elif GDE0213B1
  #include <GxGDE0213B1/GxGDE0213B1.h>
  #elif GDEH0213B72
  #include <GxGDEH0213B72/GxGDEH0213B72.h>
  #elif GDEH0213B73
  #include <GxGDEH0213B73/GxGDEH0213B73.h>
  #elif GDEW0213Z16
  #include <GxGDEW0213Z16/GxGDEW0213Z16.h>
  #elif GDEH029A1
  #include <GxGDEH029A1/GxGDEH029A1.h>
  #elif GDEW029T5
  #include <GxGDEW029T5/GxGDEW029T5.h>
  #elif GDEW029Z10
  #include <GxGDEW029Z10/GxGDEW029Z10.h>
  #elif GDEW026T0
  #include <GxGDEW026T0/GxGDEW026T0.h>
  #elif GDEW027C44
  #include <GxGDEW027C44/GxGDEW027C44.h>
  #elif GDEW027W3
  #include <GxGDEW027W3/GxGDEW027W3.h>
  #elif GDEW0371W7
  #include <GxGDEW0371W7/GxGDEW0371W7.h>
  #elif GDEW042Z15
  #include <GxGDEW042Z15/GxGDEW042Z15.h>
  #elif GDEW0583T7
  #include <GxGDEW0583T7/GxGDEW0583T7.h>
  #elif GDEW075Z09
  #include <GxGDEW075Z09/GxGDEW075Z09.h>
  #elif GDEW075Z08
  #include <GxGDEW075Z08/GxGDEW075Z08.h>
#endif

#include <GxIO/GxIO_SPI/GxIO_SPI.cpp>
#include <GxIO/GxIO.cpp>

// FONT used for title / message body
//Converting fonts with ümlauts: ./fontconvert *.ttf 18 32 252
#include <Fonts/FreeMonoBold12pt7b.h>
#include <Fonts/FreeMonoBold24pt7b.h>

bool debugMode = false;

unsigned int secondsToDeepsleep = 0;
uint64_t USEC = 1000000;

// SPI interface GPIOs defined in Config.h  
GxIO_Class io(SPI, EINK_CS, EINK_DC, EINK_RST);
// (GxIO& io, uint8_t rst = D4, uint8_t busy = D2);
GxEPD_Class display(io, EINK_RST, EINK_BUSY );

WiFiClient client; // wifi client object


// Determine path and schema (http vs https)
char *path;
bool secure = true;
char * host;

char * hostFrom(char url[]) {
  char * pch;
  pch = strtok (url,"/");
  __uint8_t p = 0;
  while (pch)
  {
    if (p==1) break;
    pch = strtok (NULL, "/");
    p++;
  }
  return pch;
}

// Displays message doing a partial update
void displayMessage(String message, int height) {
  Serial.println("DISPLAY prints: "+message);
  display.setTextColor(GxEPD_WHITE);
  display.fillRect(0,0,display.width(),height,GxEPD_BLACK);
  display.setCursor(2, 25);
  display.print(message);
  display.updateWindow(0,0,display.width(),height, true); // Attempt partial update
  display.update(); // -> Since could not make partial updateWindow work
}

void displayClean() {
  display.fillScreen(GxEPD_WHITE);
  display.update();
}

uint16_t read16()
{
  // BMP data is stored little-endian, same as Arduino.
  uint16_t result;
  ((uint8_t *)&result)[0] = client.read(); // LSB
  ((uint8_t *)&result)[1] = client.read(); // MSB
  //Serial.print(result, HEX);
  return result;
}

uint32_t read32()
{
  // BMP data is stored little-endian, same as Arduino.
  uint32_t result;
  ((uint8_t *)&result)[0] = client.read(); // LSB
  ((uint8_t *)&result)[1] = client.read();
  ((uint8_t *)&result)[2] = client.read();
  ((uint8_t *)&result)[3] = client.read(); // MSB
  return result;
}

bool parsePathInformation(char *url, char **path, bool *secure){
  if(url==NULL){
    return false;
  }
  for(unsigned i = 0; i < strlen(url); i++){
    switch (url[i])
    {
    case ':':
      if (i < 7) // If we find : past the 7th position, we know it's probably a port number
      { // This is to handle "http" or "https" in front of URL
        if (secure!=NULL&&i == 4)
        {
          *secure = false;
        }
        else if(secure!=NULL)
        {
          *secure = true;
        }
        i = i + 2; // Move our cursor out of the schema into the domain
        continue;
      }
      break;
    case '/': // We know if we skipped the schema, than the first / will be the start of the path
      *path = &url[i];
      return true;
    }
  }
  return false;
}


void handleWebToDisplay() {
  int millisIni = millis();
  // Copy the screenUrl[] in a new char:
  char *url_copy = strdup(screenUrl);

  if(!parsePathInformation(url_copy, &path, &secure)){ // Let me know if you need the host too!
      Serial.println("Parsing error with given screenUrl");
      return;
  }
  // Read host
  host = hostFrom(screenUrl);

  String request;
  request  = "GET " + String(path) + " HTTP/1.1\r\n";
  request += "Host: " + String(host) + "\r\n";
  if (bearer != "") {
    request += "Authorization: Bearer "+bearer+ "\r\n";
  }
  request += "Connection: close\r\n";
  request += "\r\n";
  
  Serial.println("REQUEST: "+request);

  client.connect(host, 80);
  client.print(request); //send the http request to the server
  client.flush();
  display.fillScreen(GxEPD_WHITE);
  
  unsigned long timeout = millis();
  while (client.available() == 0) {
    if (millis() - timeout > 5000) {
      Serial.println(">>> Client Timeout !");
      client.stop();
      return;
    }
  }
  
  int displayWidth = display.width();
  int displayHeight= display.height();// Not used now
  uint8_t buffer[displayWidth]; // pixel buffer, size for r,g,b
  long bytesRead = 32; // summing the whole BMP info headers
  long count = 0;
  uint8_t lastByte = 0x00;

// Start reading bits
while (client.available()) {
  count++;
  uint8_t clientByte = client.read();
  uint16_t bmp;
  ((uint8_t *)&bmp)[0] = lastByte; // LSB
  ((uint8_t *)&bmp)[1] = clientByte; // MSB
  if (debugMode) {
    Serial.print(bmp,HEX);Serial.print(" ");
    if (0 == count % 16) {
      Serial.println();
    }
    delay(1);
  }
  lastByte = clientByte;
  
  if (bmp == 0x4D42) { // BMP signature
    int millisBmp = millis();
    uint32_t fileSize = read32();
    read32(); // creatorBytes
    uint32_t imageOffset = read32(); // Start of image data
    uint32_t headerSize = read32();
    uint32_t width  = read32();
    uint32_t height = read32();
    uint16_t planes = read16();
    uint16_t depth = read16(); // bits per pixel
    uint32_t format = read32();
    
      Serial.print("->BMP starts here. File size: "); Serial.println(fileSize);
      Serial.print("Image Offset: "); Serial.println(imageOffset);
      Serial.print("Header size: "); Serial.println(headerSize);
      Serial.print("Width * Height: "); Serial.print(String(width) + " x " + String(height));
      Serial.print(" / Bit Depth: "); Serial.println(depth);
      Serial.print("Planes: "); Serial.println(planes);Serial.print("Format: "); Serial.println(format);
    
    if ((planes == 1) && (format == 0 || format == 3)) { // uncompressed is handled
      // Attempt to move pointer where image starts
      client.readBytes(buffer, imageOffset-bytesRead); 
      size_t buffidx = sizeof(buffer); // force buffer load
      
      for (uint16_t row = 0; row < height; row++) // for each line
      {
        //delay(1); // May help to avoid Wdt reset
        uint8_t bits = 0;
        for (uint16_t col = 0; col < width; col++) // for each pixel
        {
          yield();
          // Time to read more pixel data?
          if (buffidx >= sizeof(buffer))
          {
            client.readBytes(buffer, sizeof(buffer));
            buffidx = 0; // Set index to beginning
            //Serial.printf("ReadBuffer Row: %d bytesRead: %d\n",row,bytesRead);
          }
          switch (depth)
          {
            case 1: // one bit per pixel b/w format
              {
                if (0 == col % 8)
                {
                  bits = buffer[buffidx++];
                  bytesRead++;
                }
                uint16_t bw_color = bits & 0x80 ? GxEPD_WHITE : GxEPD_BLACK;
                display.drawPixel(col, displayHeight-row, bw_color);
                bits <<= 1;
              }
              break;
              
            case 4: // was a hard work to get here
              {
                if (0 == col % 2) {
                  bits = buffer[buffidx++];
                  bytesRead++;
                }   
                bits <<= 1;           
                bits <<= 1;
                uint16_t bw_color = bits > 0x80 ? GxEPD_WHITE : GxEPD_BLACK;
                display.drawPixel(col, displayHeight-row, bw_color);
                bits <<= 1;
                bits <<= 1;
              }
              break;
              
             case 24: // standard BMP format
              {
                uint16_t b = buffer[buffidx++];
                uint16_t g = buffer[buffidx++];
                uint16_t r = buffer[buffidx++];
                uint16_t bw_color = ((r + g + b) / 3 > 0xFF  / 2) ? GxEPD_WHITE : GxEPD_BLACK;
                display.drawPixel(col, displayHeight-row, bw_color);
                bytesRead = bytesRead +3;
              }
          }
        } // end pixel
      } // end line
      int millisEnd = millis();

      Serial.printf("Bytes read: %lu BMP headers detected: %d ms. BMP total fetch: %d ms.  Total download: %d ms\n",bytesRead,millisBmp-millisIni, millisEnd-millisBmp, millisEnd-millisIni);

       display.update();
       Serial.printf("display.update() render: %lu ms.\n", millis()-millisEnd);
       client.stop();
       break;
       
    } else {
      display.setCursor(10, 43);
      display.print("Compressed BMP files are not handled. Unsupported image format (depth:"+String(depth)+")");
      display.update();
      
    }
  }
  
  }     
}

void loop() {

  // Note: Enable deepsleep only as last step when all the rest is working as you expect
#ifdef DEEPSLEEP_ENABLED
  if (secondsToDeepsleep>SLEEP_AFTER_SECONDS) {
      display.powerDown();
      delay(10);
      #ifdef ESP32
        Serial.printf("Going to sleep %llu seconds\n", DEEPSLEEP_SECONDS);
        esp_sleep_enable_timer_wakeup(DEEPSLEEP_SECONDS * USEC);
        esp_deep_sleep_start();
      #elif ESP8266
        Serial.println("Going to sleep. Waking up only if D0 is connected to RST");
        ESP.deepSleep(10800e6);  // 3600e6 = 1 hour in seconds / ESP.deepSleepMax()
      #endif
  }
  secondsToDeepsleep++;
  delay(1000);
#endif

}

void setup() {
  Serial.begin(115200);

  display.init();
  display.setRotation(eink_rotation); // Rotates display N times clockwise
  display.setFont(&FreeMonoBold12pt7b);
  display.setTextColor(GxEPD_BLACK);
  uint8_t connectTries = 0;
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED && connectTries<6) {
    Serial.print(" .");
    delay(500);
    connectTries++;
  }


  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("ONLINE");
    Serial.println(WiFi.localIP());
    delay(999);
    handleWebToDisplay();
  } else {
    // There is no WiFi. Leave this at least in 600 seconds so it will retry in 10 minutes. As default half an hour:
    int secs = 1800;
    display.powerDown();

      #ifdef ESP32
        Serial.printf("Going to sleep %d seconds\n", secs);
        esp_sleep_enable_timer_wakeup(secs * USEC);
        esp_deep_sleep_start();
      #elif ESP8266
        Serial.println("Going to sleep. Waking up only if D0 is connected to RST");
        ESP.deepSleep(1800e6);
      #endif
      }
}