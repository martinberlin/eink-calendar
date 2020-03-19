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
// JPEG decoder library
#include <JPEGDecoder.h>
#include <TFT_eSPI.h>

TFT_eSPI tft = TFT_eSPI();
// FONT used for title / message body
// Converting fonts with ümlauts: ./fontconvert *.ttf 18 32 252
#include <Fonts/FreeMonoBold12pt7b.h>
#include <Fonts/FreeMonoBold24pt7b.h>

bool debugMode = false;

unsigned int secondsToDeepsleep = 0;
uint64_t USEC = 1000000;
WiFiClient client; // wifi client object


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


void handleWebToDisplay() {
  int millisIni = millis();
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
  if (debugMode) {
    Serial.println(request);
  }

  client.connect(host, 80);
  client.print(request); //send the http request to the server
  client.flush();

  unsigned long timeout = millis();
  while (client.available() == 0) {
    if (millis() - timeout > 5000) {
      Serial.println(">>> Client Timeout !");
      client.stop();
      return;
    }
  }
  // Read image and dump it on the TFT 
  
// Start reading bits
while (client.available()) {
    yield();
  }     
}


void loop() {

  // Note: Enable deepsleep only as last step when all the rest is working as you expect
#ifdef DEEPSLEEP_ENABLED
  if (secondsToDeepsleep>SLEEP_AFTER_SECONDS) {
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
    // There is no WiFi or can't connect. After getting this to work leave this at least in 600 seconds so it will retry in 10 minutes so 
    //                                    if your WiFi is temporarily down the system will not drain your battery in a loop trying to connect.
    int secs = 10;
    
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