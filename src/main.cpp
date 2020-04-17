#include <Config.h>
#include <string>
// Please note this version is only ESP32
#include <WiFi.h>
#include <ESPmDNS.h>
// Bluetooth arsenal:
#include <ArduinoJson.h>
#include <Preferences.h>
#include <nvs.h>
#include <nvs_flash.h>
Preferences preferences;
#include <WiFiClient.h>
#include <HTTPClient.h>

#include <GxEPD.h>
#include "BluetoothSerial.h"
#include <TinyPICO.h>
#ifdef TINYPICO
  TinyPICO tp = TinyPICO();
#endif
float batteryVoltage = 0;

// SerialBT class
BluetoothSerial SerialBT;
StaticJsonDocument<900> jsonBuffer;
// DEBUG_MODE is compiled now and cannot be changed on runtime (Check lib/Config)
String screenUrl;
String bearer;
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
// Partial update coordinates
uint16_t partialupdate_y = 0; 
uint16_t partialupdate_x = 0;
// Copied verbatim from gxEPD example (See platformio.ini)
static const uint16_t input_buffer_pixels = 640; // may affect performance
static const uint16_t max_palette_pixels = 256; // for depth <= 8

uint8_t input_buffer[3 * input_buffer_pixels]; // up to depth 24
uint8_t mono_palette_buffer[max_palette_pixels / 8]; // palette buffer for depth <= 8 b/w
uint8_t color_palette_buffer[max_palette_pixels / 8]; // palette buffer for depth <= 8 c/w
#include <GxIO/GxIO_SPI/GxIO_SPI.cpp>
#include <GxIO/GxIO.cpp>

// FONT used for title / message body
//Converting fonts with ümlauts: ./fontconvert *.ttf 18 32 252
#include <Fonts/FreeMono9pt7b.h>
#include <Fonts/FreeMonoBold12pt7b.h>

unsigned int secondsToDeepsleep = 0;
uint64_t USEC = 1000000;

// SPI interface GPIOs defined in Config.h  
GxIO_Class io(SPI, EINK_CS, EINK_DC, EINK_RST);
// (GxIO& io, uint8_t rst = D4, uint8_t busy = D2);
GxEPD_Class display(io, EINK_RST, EINK_BUSY );
HTTPClient http;   // Service times mini request


char apName[] = "CALE-xxxxxxxxxxxx";
bool usePrimAP = true;
/** Flag if stored AP credentials are available */
bool hasCredentials = false;
/** Connection status */
volatile bool isConnected = false;
bool connStatusChanged = false;
uint8_t lostConnectionCount = 1;
/** SSIDs/Password of local WiFi networks */
String wifi_ssid1;
String wifi_pass1;
String wifi_ssid2 = "";
String wifi_pass2 = "";

void deleteWifiCredentials() {
	Serial.println("Clearing saved WiFi credentials");
	preferences.begin("WiFiCred", false);
	preferences.clear();
	preferences.end();
}

// Displays message doing a partial update
void displayMessage(String message, int height) {
  Serial.println("DISPLAY prints: "+message);
  display.setTextColor(GxEPD_BLACK);
  display.setCursor(2, height);
  display.print(message);
  display.update(); // -> Since could not make partial updateWindow work
}

void displayClean() {
  display.fillScreen(GxEPD_WHITE);
  display.update();
}

/** Callback for connection loss */
void lostCon(system_event_id_t event) {
	isConnected = false;
	connStatusChanged = true;

    Serial.printf("WiFi lost connection try %d to connect again\n", lostConnectionCount);
	// Avoid trying to connect forever if the user made a typo in password
	if (lostConnectionCount>4) {
		deleteWifiCredentials();
		ESP.restart();
	} else if (lostConnectionCount>1) {
		Serial.printf("Lost connection %d times", lostConnectionCount);
	}
	lostConnectionCount++;
  WiFi.begin(wifi_ssid1.c_str(), wifi_pass1.c_str());
}

/**
 * Create unique device name from MAC address
 **/
void createName() {
	uint8_t baseMac[6];
	// Get MAC address for WiFi station
	esp_read_mac(baseMac, ESP_MAC_WIFI_STA);
	// Write unique name into apName
	sprintf(apName, "CALE-%02X%02X%02X%02X%02X%02X", baseMac[0], baseMac[1], baseMac[2], baseMac[3], baseMac[4], baseMac[5]);
}

/**
 * initBTSerial
 * Initialize Bluetooth Serial
 * Start BLE server and service advertising
 * @return <code>bool</code>
 * 			true if success
 * 			false if error occured
 */
bool initBTSerial() {
		if (!SerialBT.begin(apName)) {
			Serial.println("Failed to start BTSerial");
			return false;
		}
    displayMessage("Bluetooth started\nOpen CALE Android app and connect with:\n"+String(apName)+"\n\nwww.CALE.es",20);
		Serial.println("BTSerial active. Device name: " + String(apName));
		return true;
}

/**
 * readBTSerial
 * read all data from BTSerial receive buffer
 * parse data for valid WiFi credentials
 */
void readBTSerial() {
	if (!SerialBT.available()) return;
	uint64_t startTimeOut = millis();
	String receivedData;
	int msgSize = 0;
	// Read RX buffer into String
	
	while (SerialBT.available() != 0) {
		receivedData += (char)SerialBT.read();
		msgSize++;
		// Check for timeout condition
		if ((millis()-startTimeOut) >= 5000) break;
	}
	SerialBT.flush();
	Serial.println("Received message " + receivedData + " over Bluetooth");

	// Decode the message only if it comes encoded (Like ESP32 WIFI Ble does)
	if (receivedData[0] != '{') {
		int keyIndex = 0;
		for (int index = 0; index < receivedData.length(); index ++) {
			receivedData[index] = (char) receivedData[index] ^ (char) apName[keyIndex];
			keyIndex++;
			if (keyIndex >= strlen(apName)) keyIndex = 0;
		}
		Serial.println("Decoded message: " + receivedData); 
	}
	
	/** Json object for incoming data */
	auto error = deserializeJson(jsonBuffer, receivedData);
	if (!error)
	{
		if (jsonBuffer.containsKey("wifi_ssid1") &&
			jsonBuffer.containsKey("wifi_pass1"))
		{
      Preferences preferences;
      preferences.begin("WiFiCred", false);
      bearer     = jsonBuffer["bearer"].as<String>();
      screenUrl  = jsonBuffer["screen_url"].as<String>();
			wifi_ssid1 = jsonBuffer["wifi_ssid1"].as<String>();
			wifi_pass1 = jsonBuffer["wifi_pass1"].as<String>();
      if (jsonBuffer.containsKey("wifi_ssid2")&&jsonBuffer.containsKey("wifi_pass2")) {
			  wifi_ssid2 = jsonBuffer["wifi_ssid2"].as<String>();
			  wifi_pass2 = jsonBuffer["wifi_pass2"].as<String>();
        preferences.putString("wifi_ssid2", wifi_ssid2);
			  preferences.putString("wifi_pass2", wifi_pass2);
      }
			preferences.putString("wifi_ssid1", wifi_ssid1);
			preferences.putString("wifi_pass1", wifi_pass1);
      preferences.putString("screen_url", screenUrl);
			preferences.putString("bearer"    , bearer);
			preferences.putBool("valid", true);
			preferences.end();

			Serial.println("Received over bluetooth:");
			Serial.println("primary SSID: "+wifi_ssid1+" password: "+wifi_pass1);
			connStatusChanged = true;
			hasCredentials = true;
			delay(500);
			Serial.println("Restarting with the preferences saved");
			ESP.restart();
		}
		else if (jsonBuffer.containsKey("erase"))
		{ // {"erase":"true"}
			Serial.println("Received erase command");
			Preferences preferences;
			preferences.begin("WiFiCred", false);
			preferences.clear();
			preferences.end();
			connStatusChanged = true;
			hasCredentials = false;
			wifi_ssid1 = "";
			wifi_pass1 = "";

			int err;
			err=nvs_flash_init();
			Serial.println("nvs_flash_init: " + err);
			err=nvs_flash_erase();
			Serial.println("nvs_flash_erase: " + err);
		}
		 else if (jsonBuffer.containsKey("reset")) {
			WiFi.disconnect();
			esp_restart();
		}
	} else {
		Serial.println("Received invalid JSON");
	}
	jsonBuffer.clear();
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

uint16_t read16bmp(WiFiClient& client)
{
  // BMP data is stored little-endian, same as Arduino.
  uint16_t result;
  ((uint8_t *)&result)[0] = client.read(); // LSB
  ((uint8_t *)&result)[1] = client.read(); // MSB
  #ifdef DEBUG_MODE
    Serial.print(result,HEX); Serial.print(" ");
  #endif
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

bool parsePathInformation(String screen_uri, char **path, char *host, unsigned *host_len, bool *secure){
  if(screen_uri==""){
    Serial.println("screen_uri is empty at parsePathInformation()");
    return false;
  }
  char *host_start = NULL;
  bool path_resolved = false;
  unsigned hostname_length = 0;
  
  char *url = (char *)screen_uri.c_str();
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

void drawBitmapFrom_HTTP_ToBuffer(bool with_color)
{
  WiFiClient client; // Wifi client object BMP request
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
char request[300];
int rsize = sizeof(request);
strlcpy(request, "POST "        , rsize);
strlcat(request, path           , rsize);
strlcat(request, " HTTP/1.1\r\n", rsize);
strlcat(request, "Host: "       , rsize);
strlcat(request, host           , rsize);
strlcat(request, "\r\n"         , rsize);

if (bearer != "") {
  strlcat(request, "Authorization: Bearer ", rsize);
  strlcat(request, bearer.c_str(), rsize);
  strlcat(request, "\r\n"        , rsize);
}

#ifdef ENABLE_INTERNAL_IP_LOG
  String ip = WiFi.localIP().toString();
  uint8_t ipLenght = ip.length()+3;
  strlcat(request, "Content-Type: application/x-www-form-urlencoded\r\n", rsize);
  strlcat(request, "Content-Length: ", rsize);
  char cLength[4];
  itoa(ipLenght, cLength, 10);
  strlcat(request, cLength    , rsize);
  strlcat(request, "\r\n\r\n" , rsize);
  strlcat(request, "ip="      , rsize);
  strlcat(request, ip.c_str() , rsize);
  strlcat(request, "\r\n"     , rsize);
#endif

  strlcat(request, "\r\n"     , 2);
  #ifdef DEBUG_MODE
    Serial.println("- - - - - - - - - ");
    Serial.println(request);
    Serial.println("- - - - - - - - - ");
  #endif
  Serial.print("connecting to "); Serial.println(host);
  if (!client.connect(host, 80))
  {
    Serial.println("connection failed");
    return;
  }
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
  Serial.printf("Searching for 0x4D42 BMP. Free heap:%d\n", ESP.getFreeHeap());
  uint32_t format;
  uint16_t depth;
  uint32_t width;
  uint32_t height;

  while (true) {
    //yield();
  if (read16bmp(client) == 0x4D42) // BMP signature
  {
    int millisBmp = millis();
    uint32_t fileSize = read32(client);
    read32(client); // creatorBytes
    uint32_t imageOffset = read32(client); // Start of image data
    uint32_t headerSize = read32(client);
    width  = read32(client);
    height = read32(client);
    uint16_t planes = read16(client);
    depth = read16(client); // bits per pixel
    format = read32(client);
    uint32_t bytes_read = 7 * 4 + 3 * 2; // read so far
    Serial.printf("\n\nFile size: %d\n",fileSize); 
    #ifdef DEBUG_MODE
    Serial.print("Image Offset: "); Serial.println(imageOffset);
    Serial.print("Header size: "); Serial.println(headerSize);
    Serial.printf("Planes:%d ", planes);
    Serial.printf("Bit Depth:%d ",depth);
    #endif
    
    Serial.printf("Format:%d (Valid is 0 or 3)\n", format);
    Serial.printf("Resolution:%d x %d\n",width,height);
    
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
    } else {
      Serial.println("BMP not supported");
    }

    millisEnd = millis();
    Serial.printf("Bytes read: %lu BMP headers detected: %d ms. BMP total fetch: %d ms.  Total download: %d ms\n",
    bytes_read,millisBmp-millisIni, millisEnd-millisBmp, millisEnd-millisIni);

    // Battery partial update only on boards that support reading battery voltage
    // Remove all this TINYPICO part if you don't want the battery level to be displayed with a partial update
    #ifdef TINYPICO
      display.setFont(&FreeMono9pt7b);
      display.setCursor(partialupdate_x, partialupdate_y);
      display.print("  "+String(batteryVoltage)+"v");
      // NOTE: Even setting the x to the display.width() - 22, did not margin to the right
      Serial.printf("Partial update. x: %d  y: %d\n", partialupdate_x, partialupdate_y);
      display.updateWindow(partialupdate_x, partialupdate_y, 20, 10, true);
    #endif

    break;
    }
  }
  millisEnd = millis();
  
  if (!valid)
  {
      char formatS[2];
      char depthS[2];
      char widthS[5];
      char heightS[5];
      itoa(format, formatS, 10);
      itoa(depth, depthS, 10);
      itoa(width, widthS, 10);
      itoa(height, heightS, 10);
      display.setFont(&FreeMono9pt7b);
      display.setCursor(0, 20);
      display.print("Unsupported image.\nFormat:"+String(formatS)+
      " -> valid:0 or 3\nDepth:"+depthS+"\nResolution:"+widthS+" x "+heightS);
  } 
  display.update();
  Serial.printf("display.update() render: %lu ms.\n", millis()-millisEnd);
  Serial.printf("freeHeap after display render: %d\n", ESP.getFreeHeap());

  #ifdef DEEPSLEEP_ENABLED
    if (isConnected && secondsToDeepsleep>SLEEP_AFTER_SECONDS+14) {
          Serial.printf("Going to sleep %llu seconds\n", DEEPSLEEP_SECONDS);
          delay(500);
          esp_sleep_enable_timer_wakeup(DEEPSLEEP_SECONDS * USEC);
          esp_deep_sleep_start();
    }
  #endif
}

/**
	 scanWiFi
	 Scans for available networks 
	 and decides if a switch between
	 allowed networks makes sense
	 @return <code>bool</code>
  True if at least one allowed network was found
*/
bool scanWiFi() {
	/** RSSI for primary network */
	int8_t rssiPrim = -1;
	/** RSSI for secondary network */
	int8_t rssiSec = -1;
	/** Result of this function */
	bool result = false;

	Serial.println("Start scanning for networks");

	WiFi.disconnect(true);
	WiFi.enableSTA(true);
	WiFi.mode(WIFI_STA);

	// Scan for AP:   async , show_hidden WiFis, passive, max_mms
	int apNum = WiFi.scanNetworks(false,false,false,1000);
	if (apNum == 0) {
		Serial.println("Found no networks.");
		return false;
	}
	
	byte foundAP = 0;
	bool foundPrim = false;

	for (int index=0; index<apNum; index++) {
		String ssid = WiFi.SSID(index);
		Serial.println("Found AP: " + ssid + " RSSI: " + WiFi.RSSI(index));
		if (!strcmp((const char*) &ssid[0], (const char*) &wifi_ssid1[0])) {
			Serial.println("Found primary AP");
			foundAP++;
			foundPrim = true;
			rssiPrim = WiFi.RSSI(index);
		}
		if (!strcmp((const char*) &ssid[0], (const char*) &wifi_ssid2[0])) {
			Serial.println("Found secondary AP");
			foundAP++;
			rssiSec = WiFi.RSSI(index);
		}
	}

	switch (foundAP) {
		case 0:
			result = false;
			break;
		case 1:
			if (foundPrim) {
				usePrimAP = true;
			} else {
				usePrimAP = false;
			}
			result = true;
			break;
		default:
			Serial.printf("RSSI Prim: %d Sec: %d\n", rssiPrim, rssiSec);
			if (rssiPrim > rssiSec) {
				usePrimAP = true; // RSSI of primary network is better
			} else {
				usePrimAP = false; // RSSI of secondary network is better
			}
			result = true;
			break;
	}
	return result;
}

/** Callback for receiving IP address from AP */
void gotIP(system_event_id_t event) {
  SerialBT.disconnect();
  SerialBT.end();
  Serial.printf("SerialBT.end() freeHeap: %d\n", ESP.getFreeHeap());
  String payload = "1";
#ifdef ENABLE_SERVICE_TIMES
  http.begin(screenUrl+"/st");
  int httpCode = http.GET();
  Serial.printf("Service times Response status:%d\n", httpCode);
  
  if (httpCode > 0) { //Check for the returning code
      payload = http.getString();
      } else {
      Serial.println("Error on HTTP request");
    }
  http.end();
#endif
  // Read bitmap from web service: (bool with_color)
  if (payload == "1") {
    drawBitmapFrom_HTTP_ToBuffer(EINK_HAS_COLOR);
  } else {
    Serial.println("Not in service time");
  }
  if (isConnected) return;

  isConnected = true;
	connStatusChanged = true;

	if (!MDNS.begin(apName)) {
		Serial.println("Error setting up MDNS responder!");
    }
  MDNS.addService("http", "tcp", 80);
}

/**
 * Start connection to AP
 */
void connectWiFi() {
	// Setup callback function for successful connection
	WiFi.onEvent(gotIP, SYSTEM_EVENT_STA_GOT_IP);
	// Setup callback function for lost connection
	WiFi.onEvent(lostCon, SYSTEM_EVENT_STA_DISCONNECTED);

 	Serial.println("Start connection to ");
	if (usePrimAP) {
		Serial.println(wifi_ssid1);
		WiFi.begin(wifi_ssid1.c_str(), wifi_pass1.c_str());
	} else {
    if (wifi_ssid2 != "") {
      Serial.println(wifi_ssid2);
      WiFi.begin(wifi_ssid2.c_str(), wifi_pass2.c_str());
    } else {
      Serial.println("wifi_ssid2 is not defined, please send it per Bluetooth to use 2 APs");
    }
	}
}

void loop() {
  if (!isConnected) {
    readBTSerial();
  }
}

void resetPreferences() {
   preferences.clear();
}

void setup() {
  Serial.begin(115200);
  #ifdef DEBUG_MODE
    display.init(115200);
    Serial.printf("setup() freeHeap after display.init() %d\n", ESP.getFreeHeap());
  #else
    display.init();
  #endif
  // Calculate bottom position for partial update. You may need to edit this to adjust to your display
    if (eink_rotation == 0 || eink_rotation == 2) {
      partialupdate_x = display.width()-20;
      partialupdate_y = display.height()-25; // used for 7.5" 800x480
    } else {
      partialupdate_x = display.height()-20;
      partialupdate_y = display.width()-20;
    }
  #ifdef TINYPICO
    tp.DotStar_SetPower(false);
    batteryVoltage = tp.GetBatteryVoltage();
    Serial.printf("TINYPICO version. Battery: %.6f v showing it on partial update\n", batteryVoltage);
  #endif

  createName();
   
  display.setRotation(eink_rotation); // Rotates display N times clockwise
  if (display.width()>250) {
    display.setFont(&FreeMonoBold12pt7b);
  } else {
    display.setFont(&FreeMono9pt7b);
  }
  display.setTextColor(GxEPD_BLACK);
  
	preferences.begin("WiFiCred", false);

  // Get the counter value, if the key does not exist, return a default value of 0
  // Note: Key name is limited to 15 chars.
  unsigned int counter = preferences.getUInt("counter", 0);

  // Increase counter by 1
  counter++;
  preferences.putUInt("counter", counter);
  Serial.printf("ESP32 has restarted %d times\n", counter);
  if (counter > RESTART_TIMES_BEFORE_CREDENTIALS_RESET) {
    Serial.println("Resetting Credentials");
    // Comment if you don't want to let the counter delete your credentials
    resetPreferences(); 
  }
  
	bool hasPref = preferences.getBool("valid", false);
	if (hasPref) {
		wifi_ssid1 = preferences.getString("wifi_ssid1","");
		wifi_pass1 = preferences.getString("wifi_pass1","");
    wifi_ssid2 = preferences.getString("wifi_ssid2","");
		wifi_pass2 = preferences.getString("wifi_pass2","");
    screenUrl  = preferences.getString("screen_url","");
		bearer     = preferences.getString("bearer","");
		if (wifi_ssid1.equals("") || wifi_pass1.equals("")) {
			Serial.println("initBTSerial() Found preferences but credentials are invalid");
      initBTSerial();
		} else {
			Serial.println("Read from preferences:");
			Serial.println("primary SSID: "+wifi_ssid1+" password: "+wifi_pass1);
      Serial.println("screen_url: "+screenUrl);
      Serial.println("bearer: "+bearer);
			hasCredentials = true;
		}
	}  else {
		Serial.println("Could not find preferences. Please send the WiFi config over Bluetooth with udpx Android application");
    // Start Bluetooth serial. This reduces Heap memory like crazy so start it only if preferences are not set!
    initBTSerial();
    Serial.printf("initBTSerial() freeHeap: %d\n", ESP.getFreeHeap());
	}
	preferences.end();

	if (hasCredentials) {
    #ifdef WIFI_TWO_APS
		// Enable this define WIFI_TWO_APS: If you want to set up 2 APs with ESP32WiFi BLE app
		if (!scanWiFi()) {
        int secs = 5;
			  Serial.printf("Going to sleep %d seconds and restarting\n", secs);
        esp_sleep_enable_timer_wakeup(secs * USEC);
        esp_deep_sleep_start();
		} else {
			connectWiFi();
		}
    #else
      connectWiFi();
    #endif
  }
  
}
