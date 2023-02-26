#include <string.h>

void startWiFi();      // Start a Wi-Fi access point, and try to connect to some given access points. Then wait for either an AP or STA connection
void startOTA();       // Start the OTA service
void startSPIFFS();    // Start the SPIFFS and list all contents
void startWebSocket(); // Start a WebSocket server
void startMDNS();      // Start the mDNS responder
void startServer();    // Start a HTTP server with a file read handler and an upload handler
String formatBytes(size_t bytes);
void webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t lenght);
bool handleFileRead(String path);
void handleNotFound();
String getContentType(String filename);