#include <Config.h>
#include <Wire.h>
char version[13] = "CALE v1.0.1";

#ifdef ESP32
  char firmware[40] = " Framework:arduino ESP32";
  #include <HTTPClient.h>
  #include <WiFi.h>
  #include <ESPmDNS.h>
#elif ESP8266
  char firmware[40] = " framework:arduino ESP8266";
  #include <ESP8266HTTPClient.h>
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
  #elif defined(GDEW075T8)
  #include <GxGDEW075T8/GxGDEW075T8.h>
  #elif defined(GDEW075T7)
  #include <GxGDEW075T7/GxGDEW075T7.h> 
  #elif defined(GDEP015OC1)
  #include <GxGDEP015OC1/GxGDEP015OC1.h>
  #elif defined(GDEW0154Z04)
  #include <GxGDEW0154Z04/GxGDEW0154Z04.h> // Controller IL0376F
  #elif defined(GDEW0154Z17)
  #include <GxGDEW0154Z17/GxGDEW0154Z17.h> // Controller IL0373
  #elif defined(GDEW0213I5F)
  #include <GxGDEW0213I5F/GxGDEW0213I5F.h>
  #elif defined(GDE0213B1)
  #include <GxGDE0213B1/GxGDE0213B1.h>
  #elif defined(GDEH0213B73)
  #include <GxGDEH0213B73/GxGDEH0213B73.h>
  #elif defined(GDEH0213B72)
  #include <GxGDEH0213B72/GxGDEH0213B72.h>
  #elif defined(GDEW0213Z16)
  #include <GxGDEW0213Z16/GxGDEW0213Z16.h>
  #elif defined(GDEH029A1)
  #include <GxGDEH029A1/GxGDEH029A1.h>
  #elif defined(GDEW029T5)
  #include <GxGDEW029T5/GxGDEW029T5.h>
  #elif defined(GDEW029Z10)
  #include <GxGDEW029Z10/GxGDEW029Z10.h>
  #elif defined(GDEW026T0)
  #include <GxGDEW026T0/GxGDEW026T0.h>
  #elif defined(GDEW027C44)
  #include <GxGDEW027C44/GxGDEW027C44.h>
  #elif defined(GDEW027W3)
  #include <GxGDEW027W3/GxGDEW027W3.h>
  #elif defined(GDEW0371W7)
  #include <GxGDEW0371W7/GxGDEW0371W7.h>
  #elif defined(GDEW042Z15)
  #include <GxGDEW042Z15/GxGDEW042Z15.h>
  #elif defined(GDEW0583T7)
  #include <GxGDEW0583T7/GxGDEW0583T7.h>
  #elif defined(GDEW075Z09)
  #include <GxGDEW075Z09/GxGDEW075Z09.h>
  #elif defined(GDEW075Z08)
  #include <GxGDEW075Z08/GxGDEW075Z08.h>
#endif

#include <GxIO/GxIO_SPI/GxIO_SPI.cpp>
#include <GxIO/GxIO.cpp>

// FONT used for title / message body
//Converting fonts with ümlauts: ./fontconvert *.ttf 18 32 252
#include <Fonts/FreeMonoBold12pt7b.h>
#include <Fonts/FreeMonoBold24pt7b.h>

bool debugMode = true;

unsigned int secondsToDeepsleep = 0;
uint64_t USEC = 1000000;
char newline[2] = "\n";

// SPI interface GPIOs defined in Config.h  
GxIO_Class io(SPI, EINK_CS, EINK_DC, EINK_RST);
// (GxIO& io, uint8_t rst = D4, uint8_t busy = D2);
GxEPD_Class display(io, EINK_RST, EINK_BUSY );

WiFiClient client; // wifi client object

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

uint32_t skip(WiFiClient& client, int32_t bytes)
{
  int32_t remain = bytes;
  uint32_t start = millis();
  while ((client.connected() || client.available()) && (remain > 0))
  {
    if (client.available())
    {
      client.read();
      remain--;
    }
    else delay(1);
    if (millis() - start > 2000) break; // don't hang forever
  }
  return bytes - remain;
}

uint32_t read(WiFiClient& client, uint8_t* buffer, int32_t bytes)
{
  int32_t remain = bytes;
  uint32_t start = millis();
  while ((client.connected() || client.available()) && (remain > 0))
  {
    if (client.available())
    {
      int16_t v = client.read();
      *buffer++ = uint8_t(v);
      remain--;
    }
    else delay(1);
    if (millis() - start > 2000) break; // don't hang forever
  }
  return bytes - remain;
}

uint16_t read16bmp(WiFiClient& client)
{
  // BMP data is stored little-endian, same as Arduino.
  uint16_t result;
  ((uint8_t *)&result)[0] = client.read(); // LSB
  ((uint8_t *)&result)[1] = client.read(); // MSB
  if (debugMode) {
    Serial.print(result,HEX); Serial.print(" ");
  } 
  return result;
}

uint16_t read16(WiFiClient& client)
{
  // BMP data is stored little-endian, same as Arduino.
  uint16_t result;
  ((uint8_t *)&result)[0] = client.read(); // LSB
  ((uint8_t *)&result)[1] = client.read(); // MSB
  return result;
}

uint32_t read32(WiFiClient& client)
{
  // BMP data is stored little-endian, same as Arduino.
  uint32_t result;
  ((uint8_t *)&result)[0] = client.read(); // LSB
  ((uint8_t *)&result)[1] = client.read();
  ((uint8_t *)&result)[2] = client.read();
  ((uint8_t *)&result)[3] = client.read(); // MSB
  return result;
}

bool parsePathInformation(char *url, char **path, char *host, unsigned *host_len, bool *secure){
  if(url==NULL){
    return false;
  }
  char *host_start = NULL;
  bool path_resolved = false;
  unsigned hostname_length = 0;
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
        host_start = &url[i+1];
        hostname_length = 0;
        continue;
      }
      break;
    case '/': // We know if we skipped the schema, than the first / will be the start of the path
      *path = &url[i];
      path_resolved = true;
      break;
    }
    if(path_resolved){
      break;
    }
    ++hostname_length;
  }
  if(host_start!=NULL&&*path!=NULL&&*host_len>hostname_length){
    memcpy(host, host_start, hostname_length);
    host[hostname_length] = NULL;
    if(path_resolved){
      return true;
    }
  }
  return false;
}

// Copied verbatim from gxEPD example (See platformio.ini)
static const uint16_t input_buffer_pixels = 640; // may affect performance
static const uint16_t max_palette_pixels = 256; // for depth <= 8

uint8_t input_buffer[3 * input_buffer_pixels]; // up to depth 24
uint8_t mono_palette_buffer[max_palette_pixels / 8]; // palette buffer for depth <= 8 b/w
uint8_t color_palette_buffer[max_palette_pixels / 8]; // palette buffer for depth <= 8 c/w

void espSleep(uint64_t seconds){
      #ifdef ESP32
        Serial.printf("Going to sleep %llu seconds\n", seconds);
        esp_sleep_enable_timer_wakeup(seconds * USEC);
        esp_deep_sleep_start();
      #elif ESP8266
        Serial.println("Going to sleep. Waking up only if D0 is connected to RST");
        ESP.deepSleep(10800e6);  // 3600e6 = 1 hour in seconds / ESP.deepSleepMax()
      #endif
}

void drawBitmapFrom_HTTP_ToBuffer(bool with_color)
{
  int millisIni = millis();
  int millisEnd = 0;
  bool connection_ok = false;
  bool valid = false; // valid format to be handled
  bool flip = true; // bitmap is stored bottom-to-top
  
  char *path;
  char host[100];
  bool secure = true; // Default to secure
  unsigned hostlen = sizeof(host);
  if(!parsePathInformation(screenUrl, &path, host, &hostlen, &secure)){
    Serial.println("Parsing error!");
    Serial.println(host);
    return;
  }
  String request;
  request  = "POST " + String(path) + " HTTP/1.1\r\n";
  request += "Host: " + String(host) + "\r\n";
  if (bearer != "") {
    request += "Authorization: Bearer "+bearer+ "\r\n";
  }

#ifdef ENABLE_INTERNAL_IP_LOG
  String localIp = "ip="+WiFi.localIP().toString();
  request += "Content-Type: application/x-www-form-urlencoded\r\n";
  request += "Content-Length: "+ String(localIp.length())+"\r\n\r\n";
  request += localIp +"\r\n";
#endif
  request += "\r\n";
  if (debugMode) {
    Serial.println(request);
  }

  Serial.print("connecting to "); Serial.println(host);
  if (!client.connect(host, 80))
  {
    Serial.println("connection failed");
    return;
  }
  Serial.print("Requesting URL: ");
  Serial.println(String(host) + String(path));
  client.connect(host, 80);
  client.print(request); //send the http request to the server
  client.flush();
  display.fillScreen(GxEPD_WHITE);

  while (client.connected())
  {
    String line = client.readStringUntil('\n');
    if (!connection_ok)
    {
      connection_ok = line.startsWith("HTTP/1.1 200 OK");
      if (connection_ok) Serial.println(line);
    }
    if (line == "\r")
    {
      Serial.println("headers received");
      break;
    }
  }
  if (!connection_ok) {
    Serial.print("Client disconnected"); 
    return;
  }
  // Parse BMP header
  Serial.print("Searching for 0x4D42 start of BMP\n\n");
  while (true) {
  if (read16bmp(client) == 0x4D42) // BMP signature
  {
    int millisBmp = millis();
    uint32_t fileSize = read32(client);
    read32(client); // creatorBytes
    uint32_t imageOffset = read32(client); // Start of image data
    uint32_t headerSize = read32(client);
    uint32_t width  = read32(client);
    uint32_t height = read32(client);
    uint16_t planes = read16(client);
    uint16_t depth = read16(client); // bits per pixel
    uint32_t format = read32(client);
    uint32_t bytes_read = 7 * 4 + 3 * 2; // read so far
    Serial.printf("\n\nFile size: %d\n",fileSize); 
    Serial.print("Image Offset: "); Serial.println(imageOffset);
    Serial.print("Header size: "); Serial.println(headerSize);
    Serial.print("Bit Depth: "); Serial.println(depth);
    Serial.printf("Resolution: %d x %d\n",width,height);
    
    if ((planes == 1) && ((format == 0) || (format == 3))) // uncompressed is handled, 565 also
    {
      valid = true;
      // BMP rows are padded (if needed) to 4-byte boundary
      uint32_t rowSize = (width * depth / 8 + 3) & ~3;
      if (depth < 8) rowSize = ((width * depth + 8 - depth) / 8 + 3) & ~3;
      if (height < 0)
      {
        height = -height;
        flip = false;
      }
      uint16_t w = width;
      uint16_t h = height;
      if ((w - 1) >= display.width())  w = display.width();
      if ((h - 1) >= display.height()) h = display.height();
      
      uint8_t bitmask = 0xFF;
      uint8_t bitshift = 8 - depth;
      uint16_t red, green, blue;
      bool whitish, colored;
      if (depth == 1) with_color = false;
      if (depth <= 8)
      {
        if (depth < 8) bitmask >>= depth;
        //bytes_read += skip(client, 54 - bytes_read); //palette is always @ 54
        bytes_read += skip(client, imageOffset - (4 << depth) - bytes_read); // 54 for regular, diff for colorsimportant
        for (uint16_t pn = 0; pn < (1 << depth); pn++)
        {
          blue  = client.read();
          green = client.read();
          red   = client.read();
          client.read();
          bytes_read += 4;
          whitish = with_color ? ((red > 0x80) && (green > 0x80) && (blue > 0x80)) : ((red + green + blue) > 3 * 0x80); // whitish
          colored = (red > 0xF0) || ((green > 0xF0) && (blue > 0xF0)); // reddish or yellowish?
          if (0 == pn % 8) mono_palette_buffer[pn / 8] = 0;
          mono_palette_buffer[pn / 8] |= whitish << pn % 8;
          if (0 == pn % 8) color_palette_buffer[pn / 8] = 0;
          color_palette_buffer[pn / 8] |= colored << pn % 8;
          // DEBUG Colors
          //Serial.print("0x00"); Serial.print(red, HEX); Serial.print(green, HEX); Serial.print(blue, HEX);
          //Serial.print(" : "); Serial.print(whitish); Serial.print(", "); Serial.println(colored);
        }
      }
      display.fillScreen(GxEPD_WHITE);
      uint32_t rowPosition = flip ? imageOffset + (height - h) * rowSize : imageOffset;
      //Serial.print("skip "); Serial.println(rowPosition - bytes_read);
      bytes_read += skip(client, rowPosition - bytes_read);
      for (uint16_t row = 0; row < h; row++, rowPosition += rowSize) // for each line
      {
        if (!connection_ok || !(client.connected() || client.available())) break;
        delay(1); // yield() to avoid WDT
        uint32_t in_remain = rowSize;
        uint32_t in_idx = 0;
        uint32_t in_bytes = 0;
        uint8_t in_byte = 0; // for depth <= 8
        uint8_t in_bits = 0; // for depth <= 8
        uint16_t color = GxEPD_WHITE;
        for (uint16_t col = 0; col < w; col++) // for each pixel
        {
          yield();
          if (!connection_ok || !(client.connected() || client.available())) break;
          // Time to read more pixel data?
          if (in_idx >= in_bytes) // ok, exact match for 24bit also (size IS multiple of 3)
          {
            uint32_t get = in_remain > sizeof(input_buffer) ? sizeof(input_buffer) : in_remain;
            uint32_t got = read(client, input_buffer, get);
            while ((got < get) && connection_ok)
            {
              //Serial.print("got "); Serial.print(got); Serial.print(" < "); Serial.print(get); Serial.print(" @ "); Serial.println(bytes_read);
              uint32_t gotmore = read(client, input_buffer + got, get - got);
              got += gotmore;
              connection_ok = gotmore > 0;
            }
            in_bytes = got;
            in_remain -= got;
            bytes_read += got;
          }
          if (!connection_ok)
          {
            Serial.print("Error: got no more after "); Serial.print(bytes_read); Serial.println(" bytes read!");
            break;
          }
          switch (depth)
          {
            case 24:
              blue = input_buffer[in_idx++];
              green = input_buffer[in_idx++];
              red = input_buffer[in_idx++];
              whitish = with_color ? ((red > 0x80) && (green > 0x80) && (blue > 0x80)) : ((red + green + blue) > 3 * 0x80); // whitish
              colored = (red > 0xF0) || ((green > 0xF0) && (blue > 0xF0)); // reddish or yellowish?
              break;
            case 16:
              {
                uint8_t lsb = input_buffer[in_idx++];
                uint8_t msb = input_buffer[in_idx++];
                if (format == 0) // 555
                {
                  blue  = (lsb & 0x1F) << 3;
                  green = ((msb & 0x03) << 6) | ((lsb & 0xE0) >> 2);
                  red   = (msb & 0x7C) << 1;
                }
                else // 565
                {
                  blue  = (lsb & 0x1F) << 3;
                  green = ((msb & 0x07) << 5) | ((lsb & 0xE0) >> 3);
                  red   = (msb & 0xF8);
                }
                whitish = with_color ? ((red > 0x80) && (green > 0x80) && (blue > 0x80)) : ((red + green + blue) > 3 * 0x80); // whitish
                colored = (red > 0xF0) || ((green > 0xF0) && (blue > 0xF0)); // reddish or yellowish?
              }
              break;
            case 1:
            case 4:
            case 8:
              {
                if (0 == in_bits)
                {
                  in_byte = input_buffer[in_idx++];
                  in_bits = 8;
                }
                uint16_t pn = (in_byte >> bitshift) & bitmask;
                whitish = mono_palette_buffer[pn / 8] & (0x1 << pn % 8);
                colored = color_palette_buffer[pn / 8] & (0x1 << pn % 8);
                in_byte <<= depth;
                in_bits -= depth;
              }
              break;
          }
          if (whitish)
          {
            color = GxEPD_WHITE;
          }
          else if (colored && with_color)
          {
            color = GxEPD_RED;
          }
          else
          {
            color = GxEPD_BLACK;
          }
          uint16_t yrow = (flip ? h - row - 1 : row);
          display.drawPixel(col, yrow, color);
        } // end pixel
      } // end line
    } else {
      Serial.println("BMP not supported");
    }

    millisEnd = millis();
    Serial.printf("Bytes read: %lu BMP headers detected: %d ms. BMP total fetch: %d ms.  Total download: %d ms\n",
    bytes_read,millisBmp-millisIni, millisEnd-millisBmp, millisEnd-millisIni);
    break;
    }
  }
  millisEnd = millis();
  
  if (!valid)
  {
      display.setCursor(5, 20);
      display.print("Unsupported image format");
      display.setCursor(5, 40);
      display.print("Compressed bmp are not handled");
  } 
  display.update();
  Serial.printf("display.update() render: %lu ms.\n", millis()-millisEnd);

  espSleep(DEEPSLEEP_SECONDS);
}

void loop() {}

void setup() {
  int csize = sizeof(newline)+sizeof(version)+sizeof(firmware);
  char firmwareVersion[50];
  strlcpy(firmwareVersion, newline , csize);
  strlcat(firmwareVersion, version , csize);
  strlcat(firmwareVersion, firmware, csize);
  Serial.begin(115200);

  if (debugMode) {
    display.init(115200);
    Serial.println(firmwareVersion);
  } else {
    display.init();
  }
   
  display.setRotation(eink_rotation); // Rotates display N times clockwise
  display.setFont(&FreeMonoBold12pt7b);
  display.setTextColor(GxEPD_BLACK);

  
  uint8_t connectTries = 0;
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED && connectTries<9) {
    Serial.print(" .");
    delay(500);
    connectTries++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println(WiFi.localIP());
 
   bool isOnServiceTime = true;

    #ifdef ENABLE_SERVICE_TIMES
      HTTPClient http;   // Service times mini request
      String payload = "1";
      int urlSize = sizeof(screenUrl)+4;
      char stRequest[urlSize];
      strcpy(stRequest, screenUrl);
      strlcat(stRequest, "/st", urlSize);
      Serial.printf("Service times(ST) request: %s\n", stRequest);
      http.begin(stRequest);
      int httpCode = http.GET();
      Serial.printf("ST HTTP Response status code: %d\n", httpCode);
      
      if (httpCode > 0) { //Check for the returning code
          payload = http.getString();
          Serial.println("ST Response body: "+payload);
          if (payload == "0") {
            isOnServiceTime = false;
          }
          } else {
          Serial.printf("ST: Error on HTTP request. Status: %d\n", httpCode);
        }
      http.end();
    #endif
    
    // true if ENABLE_SERVICE_TIMES is disabled
  if (isOnServiceTime) {
   
      drawBitmapFrom_HTTP_ToBuffer(EINK_HAS_COLOR);

   } else {
      display.powerDown();
      espSleep(3600);
   }

  } else {
    // There is no WiFi or can't connect. After getting this to work leave this at least in 600 seconds so it will retry in 10 minutes so 
    //                                    if your WiFi is temporarily down the system will not drain your battery in a loop trying to connect.
    display.powerDown();
    espSleep(1800);
      }
}
