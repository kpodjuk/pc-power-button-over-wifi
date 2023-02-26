#include <string.h>

// ************ Global vars ************
const int powerButtonPin = D4;
const int poweredOnDetectionPin = D2;
#define JSON_MAXLENGTH 200
ESP8266WiFiMulti wifiMulti;                 // Create an instance of the ESP8266WiFiMulti class, called 'wifiMulti'
StaticJsonDocument<JSON_MAXLENGTH> jsonDoc; // JSON document received with websocket
ESP8266WebServer server(80);                // Create a webserver object that listens for HTTP request on port 80
WebSocketsServer webSocket(81);             // create a websocket server on port 81
File fsUploadFile;                          // a File variable to temporarily store the received file
const char *ssid = "NodeMCU1";              // The name of the Wi-Fi network that will be created
const char *password = "";                  // The password required to connect to it, leave blank for an open network
const char *OTAName = "ESP8266";            // A name and a password for the OTA service
const char *OTAPassword = "esp8266";
const char *mdnsName = "esp8266"; // Domain name for the mDNS responder
unsigned long previousMillis;
const long interval = 1000;

// ************ Function definitions ************
void startWiFi();      // Start a Wi-Fi access point, and try to connect to some given access points. Then wait for either an AP or STA connection
void startOTA();       // Start the OTA service
void startSPIFFS();    // Start the SPIFFS and list all contents
void startWebSocket(); // Start a WebSocket server
void startMDNS();      // Start the mDNS responder
void startServer();    // Start a HTTP server with a file read handler and an upload handler
String formatBytes(size_t bytes);
void webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t lenght);
bool handleFileRead(String path);
void sendStatus();
void handleNotFound();
String getContentType(String filename);
void pressPowerButton();
void turnOff();
void turnOn();
void checkButtonPin();
