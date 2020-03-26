#include <Config.h>
#ifdef ESP32
  #include <WiFi.h>
  #include <ESPmDNS.h>
  // MP3 example
  #include "HTTPClient.h" // Needed because of ESP8266 dependencies, not used
  #include "SD.h"         // Needed because of ESP8266 dependencies, not used
  #include "SPIFFS.h"
  #include "AudioFileSourceSPIFFS.h"
  #include "AudioFileSourceID3.h"
  #include "AudioGeneratorMP3.h"
  #include "AudioOutputI2SNoDAC.h"
  #include "AudioOutputI2S.h"
#endif
#include <WiFiClient.h>
#include <SPI.h>
#include <GxEPD.h>
#include <Button2.h>


// MP3 Demo: To run, set your ESP32 build to 160MHz, and include a SPIFFS of 512KB or greater ( pio run --target uploadfs )
// pno_cs from https://ccrma.stanford.edu/~jos/pasp/Sound_Examples.html
AudioGeneratorMP3 *mp3;
AudioFileSourceSPIFFS *file;
AudioOutputI2S *out;
AudioFileSourceID3 *id3;
bool playAudio = false;

// Sounds, place yours in data: 
char voice_cale[] = "/cale.mp3";
char voice_one[] = "/1.mp3";
char voice_two[] = "/2.mp3";
char voice_three[] = "/3.mp3";

// Called when a metadata event occurs (i.e. an ID3 tag, an ICY block, etc.
void MDCallback(void *cbData, const char *type, bool isUnicode, const char *string)
{
  (void)cbData;
  Serial.printf("ID3 callback for: %s = '", type);

  if (isUnicode) {
    string += 2;
  }
  
  while (*string) {
    char a = *(string++);
    if (isUnicode) {
      string++;
    }
    Serial.printf("%c", a);
  }
  Serial.printf("'\n");
  Serial.flush();
}

// BUTTON Config
Button2 *pBtns = nullptr;
uint8_t g_btns[] = BUTTONS_MAP;
// Get the right interface for the display
#ifdef GDEW042T2
  #include <GxGDEW042T2/GxGDEW042T2.h>
  #elif defined(GDEW075T8)
  #include <GxGDEW075T8/GxGDEW075T8.h>
  #elif defined(GDEW075T7)
  #include <GxGDEW075T7/GxGDEW075T7.h> 
  #elif defined(GDEW0213I5F)
  #include <GxGDEW0213I5F/GxGDEW0213I5F.h>
  #elif defined(GDE0213B1)
  #include <GxGDE0213B1/GxGDE0213B1.h>
  #elif defined(GDEH0213B72)
  #include <GxGDEH0213B72/GxGDEH0213B72.h>
  #elif defined(GDEH0213B73)
  #include <GxGDEH0213B73/GxGDEH0213B73.h>
  #elif defined(GDEW0213Z16)
  #include <GxGDEW0213Z16/GxGDEW0213Z16.h>
  #elif defined(GDEH029A1)
  #include <GxGDEH029A1/GxGDEH029A1.h>
  #elif defined(GDEW029T5)
  #include <GxGDEW029T5/GxGDEW029T5.h>
  #elif defined(GDEW029Z10)
  #include <GxGDEW029Z10/GxGDEW029Z10.h>
  #elif defined(GDEW027W3)
  #include <GxGDEW027W3/GxGDEW027W3.h>
#endif

#include <GxIO/GxIO_SPI/GxIO_SPI.cpp>
#include <GxIO/GxIO.cpp>
uint8_t selectedScreen = 0;
// FONT used for title / message body
//Converting fonts with ümlauts: ./fontconvert *.ttf 18 32 252
#include <Fonts/FreeMonoBold12pt7b.h>
#include <Fonts/FreeMonoBold24pt7b.h>

unsigned int secondsToDeepsleep = 0;
uint64_t USEC = 1000000;

// SPI interface GPIOs defined in Config.h  
GxIO_Class io(SPI, EINK_CS, EINK_DC, EINK_RST);
// (GxIO& io, uint8_t rst = D4, uint8_t busy = D2);
GxEPD_Class display(io, EINK_RST, EINK_BUSY );

WiFiClient client; // wifi client object

void amplifierHigh() {
#if defined(AMP_POWER_CTRL)
  // Turn on the Amplifier:
  pinMode(AMP_POWER_CTRL, OUTPUT);
  digitalWrite(AMP_POWER_CTRL, HIGH);
#endif
}
void amplifierLow() {
#if defined(AMP_POWER_CTRL)
  digitalWrite(AMP_POWER_CTRL, LOW);
#endif
}

void displayInit() {
  #if defined(DEBUG_MODE)
    display.init(115200);
  #else
    display.init();
  #endif
  
  display.setRotation(eink_rotation); // Rotates display N times clockwise
  display.setFont(&FreeMonoBold12pt7b);
  display.setTextColor(GxEPD_BLACK);
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


uint32_t skip(WiFiClient& client, int32_t bytes)
{
  int32_t remain = bytes;
  uint32_t start = millis();
  while ((client.connected() || client.available()) && (remain > 0))
  {
    if (client.available())
    {
      int16_t v = client.read();
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

/**
 * Convert the internal IP to string
 */
String IpAddress2String(const IPAddress& ipAddress)
{
  return String(ipAddress[0]) + String(".") +\
  String(ipAddress[1]) + String(".") +\
  String(ipAddress[2]) + String(".") +\
  String(ipAddress[3]);
}

// Copied verbatim from gxEPD example (See platformio.ini)
static const uint16_t input_buffer_pixels = 640; // may affect performance
static const uint16_t max_palette_pixels = 256; // for depth <= 8

uint8_t input_buffer[3 * input_buffer_pixels]; // up to depth 24
uint8_t mono_palette_buffer[max_palette_pixels / 8]; // palette buffer for depth <= 8 b/w
uint8_t color_palette_buffer[max_palette_pixels / 8]; // palette buffer for depth <= 8 c/w

void handleWebToDisplay(char screenUrl[], String bearer, bool with_color) {
    int millisIni = millis();
  int millisEnd = 0;
  bool connection_ok = true;
  bool valid = false; // valid format to be handled
  bool flip = true; // bitmap is stored bottom-to-top
  uint32_t startTime = millis();
  // Determine schema (http vs https), Host and /Route
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
  String localIp = "ip="+IpAddress2String(WiFi.localIP());
  request += "Content-Type: application/x-www-form-urlencoded\r\n";
  request += "Content-Length: "+ String(localIp.length())+"\r\n\r\n";
  request += localIp +"\r\n";
#endif
  request += "\r\n";
  
  Serial.println("REQUEST: "+request);
  // Send the http request to the server
  client.connect(host, 80);
  client.print(request);
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
  
  long count = 0;
  uint8_t lastByte = 0x00;

// Start reading bits
while (client.available()) {
  count++;
  uint8_t clientByte = client.read();
  uint16_t bmp;
  ((uint8_t *)&bmp)[0] = lastByte; // LSB
  ((uint8_t *)&bmp)[1] = clientByte; // MSB
  
  #if defined(DEBUG_MODE)
    Serial.print(bmp,HEX);Serial.print(" ");
    if (0 == count % 16) {
      Serial.println();
    }
    delay(1);
  #endif

  lastByte = clientByte;
  
  if (bmp == 0x4D42) { // BMP signature
   int millisBmp = millis();
    uint32_t fileSize = read32(client);
    uint32_t creatorBytes = read32(client);
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
    Serial.printf("Planes: %d Format: %d\n",planes,format);
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
      bytes_read += skip(client, rowPosition - bytes_read);
    
      for (uint16_t row = 0; row < h; row++, rowPosition += rowSize) // for each line
      {
        //Serial.printf(" %d",row);
        if (!(client.connected() || client.available())) {
          Serial.println("Getting out: WiFiClient not more available");
          break;
        }
        //delay(1); // yield() to avoid WDT
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
}


void playMp3(char * mp3file) {

  int output_mode = AudioOutputI2S::INTERNAL_DAC;
#if defined(ENABLE_EXT_I2S)
  output_mode = AudioOutputI2S::EXTERNAL_I2S;
#endif

  amplifierHigh();
  audioLogger = &Serial;
  file = new AudioFileSourceSPIFFS(mp3file);
  id3 = new AudioFileSourceID3(file);
  id3->RegisterMetadataCB(MDCallback, (void*)"ID3TAG");
  //out = new AudioOutputI2SNoDAC();
  out = new AudioOutputI2S(0, output_mode);
  out->SetPinout(IIS_BCK, IIS_WS, IIS_DOUT);

  mp3 = new AudioGeneratorMP3();
  mp3->begin(id3, out);
}

void espressifSleep() {
  // If you want to use a button for "deepsleep" this may be a great place: 
        esp_sleep_enable_ext0_wakeup((gpio_num_t)BUTTON_1, LOW);
        esp_sleep_enable_ext1_wakeup(((uint64_t)(((uint64_t)1) << BUTTON_1)), ESP_EXT1_WAKEUP_ALL_LOW);
        Serial.println(" Going to sleep now, press first Button or Reset to wake up!");
        delay(1000);
        esp_deep_sleep_start(); 
}

void button_handle(uint8_t gpio)
{
    switch (gpio) {
#if BUTTON_1
    case BUTTON_1: {
      Serial.printf("Clicked: %d \n", BUTTON_1);
      #ifdef ENABLE_SOUNDS
      playMp3(voice_one);
      while (1) {
        if (mp3->isRunning()) {
            if (!mp3->loop())
                mp3->stop();
        } else {
            break;
        }
      }
      amplifierLow();
      #endif

      displayInit();
        if (selectedScreen != 1) {
          // 3rd param: with_color (boolean)  Still untested!
           handleWebToDisplay(screen1, bearer1, false);
           selectedScreen = 1;
        }
        // Just uncomment this espressifSleep() if you want the T5 to be awake all the time (But be aware it takes about 35mA/hour)
        espressifSleep();
    }
    break;
#endif

#if BUTTON_2
    case BUTTON_2: {
      #ifdef ENABLE_SOUNDS
      playMp3(voice_two);
      while (1) {
        if (mp3->isRunning()) {
            if (!mp3->loop())
                    mp3->stop();
            } else {
                break;
            }
      }
      amplifierLow();
      #endif

      displayInit();
        Serial.printf("Clicked: %d \n", BUTTON_2);
        if (selectedScreen != 2) {
           handleWebToDisplay(screen2, bearer2, false);
           selectedScreen = 2;
        }
        espressifSleep();
    }
    break;
#endif

#if BUTTON_3
    case BUTTON_3: {
      Serial.printf("Clicked: %d\n", BUTTON_3);
      #ifdef ENABLE_SOUNDS
      playMp3(voice_three);
      while (1) {
        if (mp3->isRunning()) {
            if (!mp3->loop())
                    mp3->stop();
            } else {
                break;
            }
      }
      amplifierLow();
      #endif
      
      //Extra Screen?
      displayInit();
        if (selectedScreen != 3) {
           handleWebToDisplay(screen3, bearer3, false);
           selectedScreen = 3;
        }
    }
    break;
#endif
    default:
        break;
    }
}
void button_callback(Button2 &b)
{
    for (int i = 0; i < sizeof(g_btns) / sizeof(g_btns[0]); ++i) {
        if (pBtns[i] == b) {
            Serial.printf("btn: %u press\n", pBtns[i].getAttachPin());
            button_handle(pBtns[i].getAttachPin());
        }
    }
}

void button_init()
{
    uint8_t args = sizeof(g_btns) / sizeof(g_btns[0]);
    pBtns = new Button2[args];
    for (int i = 0; i < args; ++i) {
        pBtns[i] = Button2(g_btns[i]);
        pBtns[i].setPressedHandler(button_callback);
    }
}

void button_loop()
{
    for (int i = 0; i < sizeof(g_btns) / sizeof(g_btns[0]); ++i) {
        pBtns[i].loop();
    }
}



void loop() {
  button_loop();

  // In case of using a button to stream internet radio:
  /* if (playAudio) {
      if (mp3->isRunning()) {
        if (!mp3->loop()) {
          mp3->stop();
          delete(mp3);
          }
      }
  } */
}



void postSetup() {
  uint8_t connectTries = 0;
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  while (WiFi.status() != WL_CONNECTED && connectTries<9) {
    Serial.print(".");
    delay(500);
    connectTries++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("ONLINE: ");
    Serial.println(WiFi.localIP());
    
    playMp3(voice_cale);
    while (1) {
      if (mp3->isRunning()) {
          if (!mp3->loop())
                  mp3->stop();
          } else {
              break;
          }
    }
    amplifierLow();
    displayInit();
    handleWebToDisplay(screen1, bearer1, false);
  } else {
    Serial.println("Could not connect, restarting...");ESP.restart();
  }
  
}

void setup() {
  Serial.begin(115200);
  
  if (!SPIFFS.begin()) {
    Serial.println("FILESYSTEM is not initialized");
    Serial.println("Please use: pio run --target uploadfs");
    delay(5000);ESP.restart();
  }

  button_init();
  postSetup(); // WiFi and extra initialization
}