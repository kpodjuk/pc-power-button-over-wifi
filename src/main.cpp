#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ArduinoOTA.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <FS.h>
#include <WebSocketsServer.h>
#include "ArduinoJson.h"
#include "main.h"
#include <arduino-timer.h>

void setup()
{
  // powerButtonPin HIGH - button not pressed, LOW - pressed
  pinMode(powerButtonPin, OUTPUT);
  // powerLightPin LOW - PC ON, HIGH - PC OFF
  pinMode(powerLightPin, INPUT);

  digitalWrite(powerButtonPin, HIGH); // HIGH - counts as not pressing the button

  Serial.begin(115200); // Start the Serial communication to send messages to the computer
  delay(10);
  Serial.println("\r\n");

  startWiFi(); // Start a Wi-Fi access point, and try to connect to some given access points. Then wait for either an AP or STA connection

  startOTA();    // Start the OTA service
  startSPIFFS(); // Start the SPIFFS and list all contents

#ifndef NO_WEBCLIENT
  startWebSocket(); // Start a WebSocket server
#endif
  startMDNS(); // Start the mDNS responder

  startServer(); // Start a HTTP server with a file read handler and an upload handler
}

void loop()
{
  timer.tick();

#ifndef NO_WEBCLIENT
  webSocket.loop(); // constantly check for websocket events
  refreshStatusIfNeeded();
#endif
#ifdef RAPORT_WIFI_STATUS
  raportStatusOnSerial();
#endif
  server.handleClient(); // run the server
  ArduinoOTA.handle();   // listen for OTA events
}

void startWiFi()
{
  WiFi.begin("Orange_Swiatlowod_Gora", "mlekogrzybowe"); // add Wi-Fi networks you want to connect to

  Serial.println("Connecting");

  unsigned long startTime = millis();
  bool connectionFailed = false;

  while (WiFi.status() != WL_CONNECTED && !connectionFailed)
  { // Wait for the Wi-Fi to connect
    delay(250);
    Serial.print('.');
    connectionFailed = (millis() - startTime < delayBeforeAP) ? false : true;
  }

  if (connectionFailed)
  {
    WiFi.softAP(ssid, password); // Start the access point
    Serial.print("Access Point \"");
    Serial.print(ssid);
    Serial.println("\" started\r\n");
  }
  else
  {
    Serial.println("\r\n");

    Serial.println("\r\n");

    WiFi.setAutoReconnect(true);
    WiFi.persistent(true);
  }
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

  server.on("/api", HTTP_GET, []()
            {
    if (server.argName(0) == "p")
    {
      String desiredAction = server.arg(0);
      String answer;
      if (desiredAction == "on")
      {
        Serial.println("\e[0;35m/api: Turning on!\e[0m");
        answer = "/api: Turning on if required!";
        server.send(200, "text/plain", answer);
        turnOn();
      }
      else if (desiredAction == "off")
      {
        Serial.println("\e[0;35m/api: Turning off!\e[0m");
        server.send(200, "text/plain", "/api: Turning off!");
        turnOff();
      }
      else if (desiredAction == "toggle")
      {
        Serial.println("\e[0;35m/api: Toggling!\e[0m");
        server.send(200, "text/plain", "/api: Toggling!");
        pressPowerButton();
      }
      else
      {
        Serial.println("\e[0;31m/api: Wrong param value!\e[0m");
        server.send(200, "text/plain", "/api: Wrong param value!");
      }
    } 
    else
    {
      // if no param = send current PC status
      Serial.println("\e[0;35m/api: Sending PC status!\e[0m");
      char answerCharArr[25];
      sprintf(answerCharArr, "/api: PC status: %s", 
      digitalRead(powerLightPin) == 0 ? "Online" : "Offline");
      server.send(200, "text/plain", answerCharArr);
    } });

  server.onNotFound(handleNotFound); // if someone requests any other file or page, go to function 'handleNotFound'
                                     // and check if the file exists

  server.begin(); // start the HTTP server
  Serial.println("HTTP server started.");
}

void handleNotFound()
{
// if the requested file or page doesn't exist, return a 404 not found error
#ifdef NO_WEBCLIENT
  server.send(404, "text/plain", "404: WebClient not available in this build, use /api?p=");
#else
  if (!handleFileRead(server.uri()))
  { // check if the file exists in the flash memory (SPIFFS), if so, send it
    server.send(404, "text/plain", "404: File Not Found");
  }
#endif
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
    Serial.printf("[%u] got WS text: %s\n", num, payload);

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
      Serial.println("Json tells me to: pressPowerButton()");
      pressPowerButton();
    }
    else if (jsonDoc["connected"] == true)
    {
      // Serial.println("Json tells me to: sendStatus()");
      sendStatus();
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

  jsonDoc["turnedOn"] = !digitalRead(powerLightPin);

  String statusString;
  serializeJson(jsonDoc, statusString);

  Serial.print("Sending status:");

  Serial.print(statusString);

  // It's sent to every client connected, not the one who requested it
  // no harm in that tho
  webSocket.broadcastTXT(statusString);
}

void pressPowerButton()
{
  const uint16_t delayBetweenPresses = 200;

  // Pin LOW == you pressed power button
  digitalWrite(powerButtonPin, LOW);
  Serial.printf("\e[0;33mpowerButtonPin=%i\e[0m\n", LOW);
  delay(delayBetweenPresses); // too small of a delay and it might not work
  digitalWrite(powerButtonPin, HIGH);
  Serial.printf("\e[0;33mpowerButtonPin=%i\e[0m\n", HIGH);

#ifndef NO_WEBCLIENT
  sendStatus();
#endif
}

// *arg param is required by timer library
bool makeSure(void *arg)
{
  // time has passed...
  // desired status  passed through desiredPcStatus variable
  bool actualPcStatus = !digitalRead(powerLightPin);
  Serial.printf(Purple "desiredPcStatus=%s\n", desiredPcStatus == ON ? "Online" : "Offline");
  Serial.printf("actualPcStatus=%s\n" EndColor, actualPcStatus == true ? "Online" : "Offline");

  // if you pressed button and there's no change in PC status, retry button press after x time
  // is desired status achieved? after this time? if not, repeat press
  if (desiredPcStatus == ON)
  {
    // delay passed, check if PC is really ON, if not repeat button press
    if (actualPcStatus == true)
    {
      // good!
      Serial.println(Green "makeSure(): Not repeating, everything is allright" EndColor);
    }
    else
    {
      Serial.println(Red "makeSure(): Pc doesn't seenm to be listening, pressing button again" EndColor);
      pressPowerButton();
    }
  }
  else if (desiredPcStatus == OFF)
  {
    // delay passed, check if PC is really OFF, if not repeat button press
    if (actualPcStatus == false)
    {
      // good!
      Serial.println(Green "makeSure(): PC Should be off and is off: Not repeating, everything is allright" EndColor);
    }
    else
    {
      Serial.println(Red "makeSure(): PC Should be off and is on: pressing button again!" EndColor);
      pressPowerButton();
    }
  }
  // desiredPcStatus = CURRENT;
  return true; // if you want to repeat function after another delay return false
}

void turnOff()
{
  if (digitalRead(powerLightPin) == LOW)
  {
    Serial.println(Yellow "PC is currently ON, have to press button!" EndColor);
    pressPowerButton();
    desiredPcStatus = OFF;
    Serial.print(Purple "I will make sure it's really OFF in: ");
    Serial.printf("%ims\n" EndColor, delayWhenMakingSure);
    timer.in(delayWhenMakingSure, makeSure);
  }
  else
  {
    Serial.println(Yellow "PC already OFF, nothing to do" EndColor);
  }
}

void turnOn()
{
  if (digitalRead(powerLightPin) == HIGH)
  {
    Serial.println(Yellow "PC is currently OFF, have to press button!" EndColor);
    pressPowerButton();
    desiredPcStatus = ON;
    Serial.print("I will make sure it's really ON in: ");
    Serial.printf("%i\nms" EndColor, delayWhenMakingSure);
    timer.in(delayWhenMakingSure, makeSure);
  }
  else
  {
    Serial.println(Yellow "PC already ON, nothing to do" EndColor);
  }
}

void refreshStatusIfNeeded()
{
  bool currentPcStatus = digitalRead(powerLightPin);

  if (currentPcStatus != previousPcStatus)
  {
    sendStatus();
  }

  previousPcStatus = digitalRead(powerLightPin);
}

void raportStatusOnSerial()
{

  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= interval)
  {
    previousMillis = currentMillis;

    Serial.printf("\e[0;34m ############### Info ############### \e[0m \n");

    Serial.printf("Signal strength: %i dBm\n", WiFi.RSSI());

    Serial.print("SSID: ");
    Serial.println(WiFi.SSID());

    Serial.print("localIP: ");
    Serial.println(WiFi.localIP());

    Serial.print("Status: ");

    char color[20] = "\e[0;31m"; // red by default
    switch (WiFi.status())
    {
    case WL_IDLE_STATUS:
    {
      Serial.printf("%sWL_IDLE_STATUS \e[0m\n", color);
      break;
    }
    case WL_NO_SSID_AVAIL:
    {
      Serial.printf("%sWL_NO_SSID_AVAIL \e[0m\n", color);
      break;
    }
    case WL_SCAN_COMPLETED:
    {
      Serial.printf("%sWL_SCAN_COMPLETED \e[0m\n", color);
      break;
    }
    case WL_CONNECTED:
    {
      strcpy(color, "\e[0;32m"); // change to green when connected
      Serial.printf("%sWL_CONNECTED \e[0m\n", color);
      break;
    }
    case WL_CONNECT_FAILED:
    {
      Serial.printf("%sWL_CONNECT_FAILED \e[0m\n", color);
      break;
    }
    case WL_CONNECTION_LOST:
    {
      Serial.printf("%sWL_CONNECTION_LOST \e[0m\n", color);
      break;
    }
    case WL_DISCONNECTED:
    {
      Serial.printf("%sWL_DISCONNECTED \e[0m\n", color);
      break;
    }
    }
  }
}