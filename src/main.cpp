#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ArduinoOTA.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <FS.h>
#include <WebSocketsServer.h>
#include "ArduinoJson.h"
#include "main.h"

// ************ Function definitions ************
void sendStatus();

// ************ Global vars ************
unsigned long start, end = 0;
unsigned long prevMillis = millis();
const int powerButtonPin = D4;

#define JSON_MAXLENGTH 200

ESP8266WiFiMulti wifiMulti; // Create an instance of the ESP8266WiFiMulti class, called 'wifiMulti'

StaticJsonDocument<JSON_MAXLENGTH> jsonDoc; // JSON document received with websocket

ESP8266WebServer server(80);    // Create a webserver object that listens for HTTP request on port 80
WebSocketsServer webSocket(81); // create a websocket server on port 81

File fsUploadFile; // a File variable to temporarily store the received file

const char *ssid = "NodeMCU1"; // The name of the Wi-Fi network that will be created
const char *password = "";     // The password required to connect to it, leave blank for an open network

const char *OTAName = "ESP8266"; // A name and a password for the OTA service
const char *OTAPassword = "esp8266";

const char *mdnsName = "esp8266"; // Domain name for the mDNS responder

void setup()
{
  // prepare power button pin
  pinMode(powerButtonPin, OUTPUT);
  digitalWrite(powerButtonPin, HIGH); // HIGH - counts as not pressing the button

  Serial.begin(115200); // Start the Serial communication to send messages to the computer
  delay(10);
  Serial.println("\r\n");

  startWiFi(); // Start a Wi-Fi access point, and try to connect to some given access points. Then wait for either an AP or STA connection

  startOTA(); // Start the OTA service

  startSPIFFS(); // Start the SPIFFS and list all contents

  startWebSocket(); // Start a WebSocket server

  startMDNS(); // Start the mDNS responder

  startServer(); // Start a HTTP server with a file read handler and an upload handler
}

void loop()
{
  webSocket.loop();      // constantly check for websocket events
  server.handleClient(); // run the server
  ArduinoOTA.handle();   // listen for OTA events
}

void startWiFi()
{                              // Start a Wi-Fi access point, and try to connect to some given access points. Then wait for either an AP or STA connection
  WiFi.softAP(ssid, password); // Start the access point
  Serial.print("Access Point \"");
  Serial.print(ssid);
  Serial.println("\" started\r\n");

  // will it connect to multiple networks or only one?
  wifiMulti.addAP("Orange_Swiatlowod_Gora", "mlekogrzybowe"); // add Wi-Fi networks you want to connect to
  // wifiMulti.addAP("Orange_Swiatlowod_E8A0", "mlekogrzybowe);

  Serial.println("Connecting");
  while (wifiMulti.run() != WL_CONNECTED && WiFi.softAPgetStationNum() < 1)
  { // Wait for the Wi-Fi to connect
    delay(250);
    Serial.print('.');
  }
  Serial.println("\r\n");
  if (WiFi.softAPgetStationNum() == 0)
  { // If the ESP is connected to an AP
    Serial.print("Connected to ");
    Serial.println(WiFi.SSID()); // Tell us what network we're connected to
    Serial.print("IP address:\t");
    Serial.print(WiFi.localIP()); // Send the IP address of the ESP8266 to the computer
  }
  else
  { // If a station is connected to the ESP SoftAP
    Serial.print("Station connected to ESP8266 AP");
  }
  Serial.println("\r\n");
}

void startOTA()
{ // Start the OTA service
  ArduinoOTA.setHostname(OTAName);
  ArduinoOTA.setPassword(OTAPassword);

  ArduinoOTA.onStart([]()
                     { Serial.println("Start"); });
  ArduinoOTA.onEnd([]()
                   { Serial.println("\r\nEnd"); });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total)
                        { Serial.printf("Progress: %u%%\r", (progress / (total / 100))); });
  ArduinoOTA.onError([](ota_error_t error)
                     {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed"); });
  ArduinoOTA.begin();
  Serial.println("OTA ready\r\n");
}

void startSPIFFS()
{                 // Start the SPIFFS and list all contents
  SPIFFS.begin(); // Start the SPI Flash File System (SPIFFS)
  Serial.println("SPIFFS started. Contents:");
  {
    Dir dir = SPIFFS.openDir("/");
    while (dir.next())
    { // List the file system contents
      String fileName = dir.fileName();
      size_t fileSize = dir.fileSize();
      Serial.printf("\tFS File: %s, size: %s\r\n", fileName.c_str(), formatBytes(fileSize).c_str());
    }
    Serial.printf("\n");
  }
}

void startWebSocket()
{                                    // Start a WebSocket server
  webSocket.begin();                 // start the websocket server
  webSocket.onEvent(webSocketEvent); // if there's an incomming websocket message, go to function 'webSocketEvent'
  Serial.println("WebSocket server started.");
}

void startMDNS()
{                       // Start the mDNS responder
  MDNS.begin(mdnsName); // start the multicast domain name server
  Serial.print("mDNS responder started: http://");
  Serial.print(mdnsName);
  Serial.println(".local");
}

void startServer()
{                                           // Start a HTTP server with a file read handler and an upload handler
  server.on("/edit.html", HTTP_POST, []() { // If a POST request is sent to the /edit.html address,
    server.send(200, "text/plain", "");
  });

  server.onNotFound(handleNotFound); // if someone requests any other file or page, go to function 'handleNotFound'
                                     // and check if the file exists

  server.begin(); // start the HTTP server
  Serial.println("HTTP server started.");
}

void handleNotFound()
{ // if the requested file or page doesn't exist, return a 404 not found error
  if (!handleFileRead(server.uri()))
  { // check if the file exists in the flash memory (SPIFFS), if so, send it
    server.send(404, "text/plain", "404: File Not Found");
  }
}

bool handleFileRead(String path)
{ // send the right file to the client (if it exists)
  Serial.println("handleFileRead: " + path);
  if (path.endsWith("/"))
    path += "index.html";                    // If a folder is requested, send the index file
  String contentType = getContentType(path); // Get the MIME type
  String pathWithGz = path + ".gz";
  if (SPIFFS.exists(pathWithGz) || SPIFFS.exists(path))
  {                                                     // If the file exists, either as a compressed archive, or normal
    if (SPIFFS.exists(pathWithGz))                      // If there's a compressed version available
      path += ".gz";                                    // Use the compressed verion
    File file = SPIFFS.open(path, "r");                 // Open the file
    size_t sent = server.streamFile(file, contentType); // Send it to the client
    file.close();                                       // Close the file again
    Serial.println(String("\tSent file-> ") + path);
    return true;
  }
  Serial.println(String("\tFile Not Found: ") + path); // If the file doesn't exist, return false
  return false;
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t lenght)
{ // When a WebSocket message is received
  switch (type)
  {
  case WStype_DISCONNECTED: // if the websocket is disconnected
    Serial.printf("[%u] Disconnected!\n", num);
    break;
  case WStype_CONNECTED:
  { // if a new websocket connection is established
    IPAddress ip = webSocket.remoteIP(num);
    Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);
  }
  break;
  case WStype_TEXT: // if new text data is received
    Serial.printf("[%u] get Text: %s\n", num, payload);

    // String payload_str = String((char*) payload);

    // process received JSON
    DeserializationError error = deserializeJson(jsonDoc, payload);

    // Test if parsing succeeds.
    if (error)
    {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.f_str());
      return;
    }

    // Fetch values.
    // Most of the time, you can rely on the implicit casts.
    // In other case, you can do doc["time"].as<long>();

    if (jsonDoc["type"] == "POWER_BUTTON")
    {
      // Pin LOW == you pressed power button
      digitalWrite(powerButtonPin, LOW);
      Serial.println("powerButtonPin=LOW");
      delay(50);
      digitalWrite(powerButtonPin, HIGH);
      Serial.println("powerButtonPin=HIGH");
    }

    break;
  }
}

String formatBytes(size_t bytes)
{ // convert sizes in bytes to KB and MB
  if (bytes < 1024)
  {
    return String(bytes) + "B";
  }
  else if (bytes < (1024 * 1024))
  {
    return String(bytes / 1024.0) + "KB";
  }
  else if (bytes < (1024 * 1024 * 1024))
  {
    return String(bytes / 1024.0 / 1024.0) + "MB";
  }
  return String(0);
}

String getContentType(String filename)
{ // determine the filetype of a given filename, based on the extension
  if (filename.endsWith(".html"))
    return "text/html";
  else if (filename.endsWith(".css"))
    return "text/css";
  else if (filename.endsWith(".js"))
    return "application/javascript";
  else if (filename.endsWith(".ico"))
    return "image/x-icon";
  else if (filename.endsWith(".gz"))
    return "application/x-gzip";
  return "text/plain";
}

void sendStatus()
{
  // send current status to websocket client here (mode, settings for that mode)
  // jsonDoc["type"] = "STATUS_UPDATE";
  // jsonDoc["OPERATING_MODE"] = currentOperatingMode;

  // if (currentOperatingMode == SOLID_COLOR)
  // { // attach info about choosen setting if needed
  // jsonDoc["solidColor"] = solidColor;
  // }

  // String statusString;
  // serializeJson(jsonDoc, statusString);

  // JsonArray data = doc.createNestedArray("data");
  // data.add(48.756080);
  // data.add(2.302038);

  // It's sent to every client connected, not the one who requested it
  // no harm in that tho
  // webSocket.broadcastTXT(statusString);
}
