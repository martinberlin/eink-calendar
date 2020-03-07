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
#endif

#include <GxIO/GxIO_SPI/GxIO_SPI.cpp>
#include <GxIO/GxIO.cpp>
uint8_t selectedScreen = 1;
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


void handleWebToDisplay(char screenUrl[], String bearer) {
  int millisIni = millis();
  // Determine schema (http vs https), Host and /Route
  char *path;
  bool secure = true;
  char * host;
 
  Serial.println(screenUrl);

  if(!parsePathInformation(strdup(screenUrl), &path, &secure)){
      Serial.println("Parsing error with given url");
      return;
  }
  host = hostFrom(screenUrl);
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
  
  int displayWidth = display.width();
  int displayHeight= display.height();// Not used now
  uint8_t buffer[displayWidth]; // pixel buffer, size for r,g,b
  long bytesRead = 34; // summing the whole BMP info headers
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

void playMp3(char * mp3file) {
  audioLogger = &Serial;
  file = new AudioFileSourceSPIFFS(mp3file);
  id3 = new AudioFileSourceID3(file);
  id3->RegisterMetadataCB(MDCallback, (void*)"ID3TAG");
  //out = new AudioOutputI2SNoDAC();
  out = new AudioOutputI2S();
  out->SetPinout(IIS_BCK, IIS_WS, IIS_DOUT);
  mp3 = new AudioGeneratorMP3();
  mp3->begin(id3, out);
}

void button_handle(uint8_t gpio)
{
    switch (gpio) {
#if BUTTON_1
    case BUTTON_1: {
        Serial.printf("Clicked: %d \n", BUTTON_1);
        if (selectedScreen != 1) {
           handleWebToDisplay(screen1, bearer1);
           selectedScreen = 1;
        }

        esp_sleep_enable_ext0_wakeup((gpio_num_t)BUTTON_1, LOW);
        esp_sleep_enable_ext1_wakeup(((uint64_t)(((uint64_t)1) << BUTTON_1)), ESP_EXT1_WAKEUP_ALL_LOW);
        Serial.println("Going to sleep now");
        delay(2000);
        esp_deep_sleep_start();
    }
    break;
#endif

#if BUTTON_2
    case BUTTON_2: {
        Serial.printf("Clicked: %d \n", BUTTON_2);
        if (selectedScreen != 2) {
           handleWebToDisplay(screen2, bearer2);
           selectedScreen = 2;
        }
    }
    break;
#endif

#if BUTTON_3
    case BUTTON_3: {
       
       Serial.printf("Clicked: %d\n", BUTTON_3);
      if (playAudio) {
        Serial.println("Play audio");
        playMp3("/pno-cs.mp3");
      } else {
        mp3->stop();
        Serial.println("Stop audio");
      }
       playAudio = !playAudio;
      //Extra Screen?
        /* if (selectedScreen != 3) {
           handleWebToDisplay(screen3, bearer3);
           selectedScreen = 3;
        } */
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

  if (playAudio) {
    if (mp3->isRunning()) {
      mp3->loop();
      //if (!mp3->loop()) mp3->stop(); // Hangs everything
    }
  }
  
  button_loop();
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
    button_init();
    Serial.printf("ONLINE: %s\n", IpAddress2String(WiFi.localIP()));
    //handleWebToDisplay(screen1, bearer1);
  } else {
    Serial.println("Restarting...");ESP.restart();
  }
  
}

void setup() {
  Serial.begin(115200);
  
  if (!SPIFFS.begin()) {
    Serial.println("FILESYSTEM is not initialized");
    Serial.println("Please use: pio run --target uploadfs");
    delay(5000);ESP.restart();
  }

  // Turn on the Amplifier:
  pinMode(AMP_POWER_CTRL, OUTPUT);
  digitalWrite(AMP_POWER_CTRL, HIGH);
  
  display.init();
  display.setRotation(eink_rotation); // Rotates display N times clockwise
  display.setFont(&FreeMonoBold12pt7b);
  display.setTextColor(GxEPD_BLACK);

  //playMp3("/pno-cs.mp3");
  //playMp3("/monster-slap.mp3"); Does not work at all

  postSetup(); // WiFi and extra initialization


}