// Miniz zlib: Discard parts that we do not use:
#define MINIZ_NO_ARCHIVE_APIS
#define MINIZ_NO_STDIO
#define MINIZ_NO_TIME
#include "miniz.c"
#include <Config.h>
#include <SPI.h>
#include <GxEPD.h>
// Please check what is the right one for your Display: https://github.com/ZinggJM/GxEPD#supported-spi-e-paper-panels-from-good-display
#include <GxGDEW042T2/GxGDEW042T2.cpp>
#include <GxIO/GxIO_SPI/GxIO_SPI.cpp>
#include <GxIO/GxIO.cpp>
#include <WiFiClient.h>

#ifdef ESP32
  #include <WiFi.h>
  #include <ESPmDNS.h>
  #include <WebServer.h>
#elif ESP8266
  #include <ESP8266WiFi.h>
  #include <ESP8266mDNS.h>
  #include <ESP8266WebServer.h>
#endif

// FONT used for title / message body
//Converting fonts with ümlauts: ./fontconvert *.ttf 18 32 252
#include <Fonts/FreeMonoBold12pt7b.h>
#include <Fonts/FreeMonoBold24pt7b.h>

bool debugMode = false;
uint64_t USEC = 1000000;
unsigned int secondsToDeepsleep = 0;

const char* domainName = "calendar"; // mDNS
String message;
// Makes a div id="m" containing response message to dissapear after 3 seconds
String javascriptFadeMessage = "<script>setTimeout(function(){document.getElementById('m').innerHTML='';},3000);</script>";

// TCP server at port 80 will respond to HTTP requests
#ifdef ESP32
  WebServer server(80);
  #elif ESP8266
  ESP8266WebServer server(80);
#endif

// Note if there is a reset after decompression is probably that the decompressed size overflows this buffer limits, just update it:
#define COMPRESSION_BUFFER 20000
#define DECOMPRESSION_BUFFER 69000

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

void handle_http_not_found() {
  server.send(404, "text/plain", "Not Found");
}

void handle_http_root() {

  String headers = "<head><link rel=\"stylesheet\" href=\"https://maxcdn.bootstrapcdn.com/bootstrap/4.0.0/css/bootstrap.min.css\">";
  headers += "<meta name='viewport' content='width=device-width,initial-scale=1'></head>";
  String html = "<body><main role='main'><div class='container-fluid'><div class='row'>";
  html += "<div class='col-md-12'><h4>" + String(domainName) + ".local</h4>";
  html += "<form id='f' action='/web-image' target='frame' method='POST'>";
  html += "<div class='row'><div class='col-sm-12'>";

  html += "<select name='zoom' style='width:7em' class='form-control float-right'>";
  html += "<option value='2'>2</option>";
  html += "<option value='1.7'>1.7</option>";
  html += "<option value='1.5'>1.5</option>";
  html += "<option value='1.4'>1.4</option>";
  html += "<option value='1.3'>1.3</option>";
  html += "<option value='1.2'>1.2</option>";
  html += "<option value='1.1'>1.1 10% zoomed</option>";
  html += "<option value='' selected>Zoom</option>";
  html += "<option value='1'>1 no zoom</option>";
  html += "<option value='.9'>.9 10% smaller</option>";
  html += "<option value='.85'>.85</option>";
  html += "<option value='.8'>.8</option>";
  html += "<option value='.7'>.7</option>";
  html += "<option value='.6'>.6</option>";
  html += "<option value='.5'>.5 half size</option></select>&nbsp;";
  
  html += "<select name='brightness' style='width:8em' class='form-control float-right'>";
  html += "<option value='150'>+5</option>";
  html += "<option value='140'>+4</option>";
  html += "<option value='130'>+3</option>";
  html += "<option value='120'>+2</option>";
  html += "<option value='110'>+1</option>";
  html += "<option value='' selected>Brightness</option>";
  html += "<option value='90'>-1</option>";
  html += "</select>";

  html += "</div></div>";
  html += "<input placeholder='http://' id='url' name='url' type='url' class='form-control' value='"+calendarUrl+"'>";
  html += "<div class='row'><div class='col-sm-12 form-group'>";
  html += "<input type='submit' value='Website screenshot' class='btn btn-mini btn-dark'>&nbsp;";
  html += "<input type='button' onclick='document.getElementById(\"url\").value=\"\"' value='Clean url' class='btn btn-mini btn-default'></div>";
  html += "</div></form>";
  
  html += "<form id='f2' action='/display-write' target='frame' method='POST'>";
  html += "<label for='title'>Title:</label><input id='title' name='title' class='form-control'><br>";
  html += "<textarea placeholder='Content' name='text' rows=4 class='form-control'></textarea>";
  html += "<input type='submit' value='Send to display' class='btn btn-success'></form>";
  html += "<a class='btn btn-default' role='button' target='frame' href='/display-clean'>Clean screen</a><br>";
  html += "<iframe name='frame'></iframe>";
  html += "<a href='/deep-sleep' target='frame'>Deep sleep</a><br>";
  html += "</div></div></div></main>";
  html += "</body>";

  server.send(200, "text/html", headers + html);
}

void handleDeepSleep() {
  server.send(200, "text/html", "Going to deep-sleep. Reset to wake up");
  delay(1);
  ESP.deepSleep(20e6);
}


void handleDisplayClean() {
  display.fillScreen(GxEPD_WHITE);
  display.update();
  server.send(200, "text/html", "Clearing display");
}

void handleDisplayWrite() {
  display.fillScreen(GxEPD_WHITE);

  // Analizo el POST iterando cada value
  if (server.args() > 0) {
    for (byte i = 0; i < server.args(); i++) {

      if (server.argName(i) == "title") {
        display.setFont(&FreeMonoBold24pt7b);
        display.setCursor(10, 43);
        display.print(server.arg(i));
      }
      if (server.argName(i) == "text") {
        display.setFont(&FreeMonoBold12pt7b);
        display.setCursor(10, 75);
        display.print(server.arg(i));
      }
    }
  }
  display.update();
  server.send(200, "text/html", "Text sent to display");
}

uint32_t readBuffer16(uint8_t * outBuffer, long *bytePointer)
{
  uint32_t result;
  long pointer = *bytePointer;
  ((uint8_t *)&result)[0] = outBuffer[pointer]; pointer++;
  ((uint8_t *)&result)[1] = outBuffer[pointer]; pointer++; // MSB
  *bytePointer = pointer;
  return result;
}

uint32_t readBuffer32(uint8_t * outBuffer, long *bytePointer)
{
  uint32_t result;
  long pointer = *bytePointer;
  ((uint8_t *)&result)[0] = outBuffer[pointer]; pointer++;
  ((uint8_t *)&result)[1] = outBuffer[pointer]; pointer++;
  ((uint8_t *)&result)[2] = outBuffer[pointer]; pointer++;
  ((uint8_t *)&result)[3] = outBuffer[pointer]; pointer++; // MSB
  *bytePointer = pointer;
  return result;
}

bool bmpBufferRead(uint8_t * outBuffer, long byteCount) {
  int displayWidth = display.width();
  int displayHeight= display.height();
  Serial.printf("EINKMODEL displayWidth: %d height: %d\n", displayWidth,displayHeight);
  uint8_t buffer[displayWidth]; // pixel buffer, size for r,g,b
  long bytesRead = 32;  // summing the whole BMP info headers
  long bytePointer = 2; // Start reading after BMP signature 0x424D

    uint32_t fileSize = readBuffer32(outBuffer, &bytePointer);
    readBuffer32(outBuffer, &bytePointer);                        // creatorBytes
    uint32_t imageOffset = readBuffer32(outBuffer, &bytePointer); // Start of image data
    uint32_t headerSize = readBuffer32(outBuffer, &bytePointer);
    uint32_t width  = readBuffer32(outBuffer, &bytePointer);
    uint32_t height = readBuffer32(outBuffer, &bytePointer);
    uint16_t planes = readBuffer16(outBuffer, &bytePointer);
    uint16_t depth = readBuffer16(outBuffer, &bytePointer);       // bits per pixel
    uint32_t format = readBuffer32(outBuffer, &bytePointer);

    Serial.printf("File size:%d\n",fileSize);
    Serial.printf("Image Offset:%d\n",imageOffset);
    Serial.printf("Header size:%d\n",headerSize);
    Serial.printf("Image Width:%d Height:%d Bit Depth:%d\n",width,height,depth);
    Serial.printf("Planes:%d Format:%d\n",planes,format); 
    
    if ((planes == 1) && (format == 0 || format == 3)) { // uncompressed is handled
      // Attempt to move pointer where image starts
      bytePointer = imageOffset;  
      size_t buffidx = sizeof(buffer); // force buffer load

      for (uint16_t row = 0; row < height; row++) // for each line
      {
        uint8_t bits;
        for (uint16_t col = 0; col < width; col++) // for each pixel
        {
          yield();
          // Time to read more pixel data?
          if (buffidx >= sizeof(buffer))
          {
            
            //TODO: Check if this assumption is correct, replaces old streaming read:
            //client.readBytes(buffer, sizeof(buffer));
             for(unsigned i = 0; i<sizeof(buffer); i++){
               buffer[i] = outBuffer[bytePointer];
               bytePointer++;
             }
            buffidx = 0; // Set index to beginning

            //Serial.printf("ReadBuffer Row: %d bytePointer: %d bytesRead: %d\n",row,bytePointer,bytesRead);
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
       // Uncomment to send answer, trying to make it faster not delivering this on calendar update
       //server.send(200, "text/html", "<div id='m'>Image sent to display</div>"+javascriptFadeMessage);
       display.update();
       Serial.printf("Read %lu bytes. Sending BMP pixels to display\n", bytesRead);
       return true;
       
    } else {
      server.send(200, "text/html", "<div id='m'>Unsupported image format (depth:"+String(depth)+")</div>"+javascriptFadeMessage);
      display.setCursor(10, 43);
      display.print("Compressed BMP files are not handled. Unsupported image format (depth:"+String(depth)+")");
      display.update();
      return false;
    }
} 

void handleWebToDisplay() {
  int milliIni = millis();
  String url = calendarUrl;
  String zoom = DEFAULT_ZOOM;
  String brightness = "100";
  if (server.args() > 0) {
    for (byte i = 0; i < server.args(); i++) {
      if (server.argName(i) == "url" && server.arg(i) != "") {
        url = server.arg(i);
      }
      if (server.argName(i) == "zoom" && server.arg(i) != "") {
        zoom = server.arg(i);
      }
      if (server.argName(i) == "brightness" && server.arg(i) != "") {
        brightness = server.arg(i);
      }
    }
  }
    if (url == "") {
      display.println("No Url received");
      display.update();
      return;
    }
  
  String image = screenshotPath+"?u=" + url + "&z=" + zoom + "&b=" + brightness +"&c=1&eink="+EINKMODEL;
  String request;
  request  = "GET " + image + " HTTP/1.1\r\n";
  request += "Host: " + String(screenshotHost) + "\r\n";
  request += "Connection: close\r\n";
  request += "\r\n";
  Serial.println(screenshotHost+image);

  const int httpPort = 80;
  client.connect(screenshotHost, httpPort);
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
  
 // NOTE: No need to discard headers anymore, unless they contain 0x4D42
long byteCount = 0; // Byte counter

// Start reading bits
uint8_t *inBuffer = new uint8_t[COMPRESSION_BUFFER];
inBuffer[byteCount] = 0x78; // zlib header[0]
byteCount++;
uint8_t lastByte = 0x00;
bool startFetch = false;
Serial.println("Zlib preview:");
Serial.print(inBuffer[0], HEX);Serial.println(" ");

  while (client.available()) {
    yield();
    uint8_t clientByte = client.read();
    if (clientByte == 0xDA && lastByte == 0x78) {
      startFetch = true;
    }
    if (startFetch) {
      inBuffer[byteCount] = clientByte;
      #ifdef ESP8266
      delay(1);
      #endif
      byteCount++;
    }
    lastByte = clientByte;
    }

    //Serial.printf("\nDone downloading compressed BMP. Length: %d\n", byteCount);
    
    int milliDecomp = millis();
  		uint8_t *outBuffer = new uint8_t[DECOMPRESSION_BUFFER];
			uLong uncomp_len;

    // status: { MZ_OK = 0, MZ_STREAM_END = 1, MZ_NEED_DICT = 2, MZ_ERRNO = -1, MZ_STREAM_ERROR = -2, 
    //           MZ_DATA_ERROR = -3, MZ_MEM_ERROR = -4, MZ_BUF_ERROR = -5, MZ_VERSION_ERROR = -6, MZ_PARAM_ERROR = -10000 };

    int cmp_status = uncompress(
      outBuffer, 
      &uncomp_len, 
      (const unsigned char*)inBuffer, 
      byteCount);

    int millisAfterDecomp = millis();
    delete(inBuffer);
    
    // Render BMP with outBuffer if this works. freeHeap: 204984 without bluetooth
    Serial.printf("uncompress_status: %d Compressed length: %lu freeHeap: %d\n", cmp_status, byteCount, ESP.getFreeHeap());
    bool isRendered = 0;
    if (cmp_status == 0) {
      isRendered = bmpBufferRead(outBuffer,uncomp_len);
    } else {
      Serial.printf("uncompress status: %d Decompression error\n", cmp_status);
    }
    Serial.printf("Eink isRendered: %d BENCHMARKS:\n", isRendered);
    Serial.printf("Download: %d ms Decompress: %d ms Rendering: %lu seconds\n", milliDecomp-milliIni, millisAfterDecomp-milliDecomp, (millis()-millisAfterDecomp)/1000 );
    delete(outBuffer);
}

void loop() {
  server.handleClient();

  // Note: Enable deepsleep only as last step when all the rest is working as you expect
#ifdef DEEPSLEEP_ENABLED
  if (secondsToDeepsleep>SLEEP_AFTER_SECONDS) {
      display.powerDown();
      #ifdef ESP32
        Serial.printf("Going to sleep %llu seconds\n", DEEPSLEEP_SECONDS);
        esp_sleep_enable_timer_wakeup(DEEPSLEEP_SECONDS * USEC);
        esp_deep_sleep_start();
      #elif ESP8266
        Serial.println("Going to sleep. Waking up only if D0 is connected to RST");
        ESP.deepSleep(DEEPSLEEP_SECONDS * 1000000);  // 3600e6 = 1 hour in seconds / ESP.deepSleepMax()
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
  while (WiFi.status() != WL_CONNECTED && connectTries<10) {
    Serial.print(" .");
    delay(500);
    connectTries++;
  }
Serial.println(WiFi.localIP());

  if (!MDNS.begin(domainName)) {
    Serial.println("Error setting up MDNS responder!");
    while (1) {
      delay(1000);
    }
  }
  Serial.printf("mDNS responder: %s.local\n",domainName);
  // Add service to MDNS-SD
  MDNS.addService("http", "tcp", 80);
  
  // Start HTTP server
  server.onNotFound(handle_http_not_found);
  // Routing
  server.on("/", handle_http_root);
  server.on("/display-write", handleDisplayWrite);
  server.on("/web-image", handleWebToDisplay);
  server.on("/display-clean", handleDisplayClean);
  server.on("/deep-sleep", handleDeepSleep);
  server.begin();
  if (WiFi.status() == WL_CONNECTED) {
    handleWebToDisplay();
  } else {
    //displayMessage("Please check your credentials in Config.h\nCould not connect to "+String(WIFI_SSID),80);
    Serial.printf("Please check your credentials in Config.h\nCould not connect to %s\n", WIFI_SSID);
    delay(1000);
    ESP.restart();
  }
}
