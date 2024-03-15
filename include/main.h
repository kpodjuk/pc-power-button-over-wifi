#include <string.h>
#include "arduino-timer.h"

//  Pin definitions
const int powerButtonPin = D4; // green wire
const int powerLightPin = D2;  // blue  wire
const long interval = 30000;   // interval when rapoerting wifi status
const long delayWhenMakingSure = 40000;
const uint64_t delayBeforeAP = 5 * 60 * 1000;

#define JSON_MAXLENGTH 200

auto timer = timer_create_default();

// ### COLORS - Defines that help with colorizing serial output ###
#define Black "\e[0;30m"
#define Red "\e[0;31m"
#define Green "\e[0;32m"
#define Yellow "\e[0;33m"
#define Blue "\e[0;34m"
#define Purple "\e[0;35m"
#define Cyan "\e[0;36m"
#define White "\e[0;37m"
#define EndColor "\e[0"
// ### COLORS ###

typedef enum
{
    CURRENT, // do nothing
    OFF,     // make sure it's off
    ON,      // make sure it's on
} desiredPcStatus_t;

desiredPcStatus_t desiredPcStatus;

ESP8266WiFiMulti wifiMulti;                 // Create an instance of the ESP8266WiFiMulti class, called 'wifiMulti'
StaticJsonDocument<JSON_MAXLENGTH> jsonDoc; // JSON document received with websocket
ESP8266WebServer server(80);                // Create a webserver object that listens for HTTP request on port 80
WebSocketsServer webSocket(81);             // create a websocket server on port 81
File fsUploadFile;                          // a File variable to temporarily store the received file
const char *ssid = "PC shutdown";           // The name of the Wi-Fi network that will be created
const char *password = "";                  // The password required to connect to it, leave blank for an open network
const char *OTAName = "ESP8266";            // A name and a password for the OTA service
const char *OTAPassword = "esp8266";
const char *mdnsName = "esp8266"; // Domain name for the mDNS responder
unsigned long previousMillis;
bool previousPcStatus;

// ************ Function definitions ************
bool makeSure(void *);

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
void checkPowerLightPin();
void refreshStatusIfNeeded();
void raportStatusOnSerial();
// WL_NO_SHIELD        = 255,   // for compatibility with WiFi Shield library
// WL_IDLE_STATUS      = 0,
// WL_NO_SSID_AVAIL    = 1,
// WL_SCAN_COMPLETED   = 2,
// WL_CONNECTED        = 3,
// WL_CONNECT_FAILED   = 4,
// WL_CONNECTION_LOST  = 5,
// WL_WRONG_PASSWORD   = 6,
// WL_DISCONNECTED     = 7
