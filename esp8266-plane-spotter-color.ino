/**The MIT License (MIT)

Copyright (c) 2015 by Daniel Eichhorn

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

See more at http://blog.squix.ch
*/

#include <Arduino.h>

//SPIFFS stuff
#include <FS.h>
#include <SPI.h>

// Wifi Libraries
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>


// Easy Wifi Setup
#include <WiFiManager.h>

// Go to settings to change important parameters
#include "settings.h"

// Project libraries
#include "WifiLocator.h"
#include "PlaneSpotter.h"
#include "ILI9341.h"
#include "artwork.h"
#include "AdsbExchangeClient.h"
#include "GeoMap.h"

// Initialize the TFT
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC);
WifiLocator locator;
AdsbExchangeClient adsbClient;
GeoMap geoMap(MAP_QUEST_API_KEY, 320, 200);
PlaneSpotter planeSpotter(&tft, &geoMap);


Coordinates mapCenter;

// Check http://www.virtualradarserver.co.uk/Documentation/Formats/AircraftList.aspx
// to craft this query to your needs
// lat=47.424341887&lng=8.56877803&fDstL=0&fDstU=10&fAltL=0&fAltL=1500&fAltU=10000
//const String QUERY_STRING = "fDstL=0&fDstU=20&fAltL=0&fAltL=1000&fAltU=10000";
// airport zürich is on 1410ft => hide landed airplanes
const String QUERY_STRING = "fDstL=0&fDstU=20&fAltL=1500";

void downloadCallback(String filename, uint32_t bytesDownloaded, uint32_t bytesTotal);
ProgressCallback _downloadCallback = downloadCallback;


void setup() {

  // Start serial communication
  Serial.begin(115200);

  // The LED pin needs to set HIGH
  // Use this pin to save energy
  pinMode(LED_PIN, D8);
  digitalWrite(LED_PIN, HIGH);

  // Init TFT
  tft.begin();
  tft.setRotation(3);  // landscape
  tft.cp437();
  tft.fillScreen(TFT_BLACK);

  // Init file system
  if (!SPIFFS.begin()) { Serial.println("initialisation failed!"); return;}

  // copy files from code to SPIFFS
  planeSpotter.copyProgmemToSpiffs(splash, splash_len, "/plane.jpg");
  planeSpotter.copyProgmemToSpiffs(plane, plane_len, "/plane.jpg");
  planeSpotter.drawSPIFFSJpeg("/splash.jpg", 30, 75);

  
  
  WiFiManager wifiManager;
  // Uncomment for testing wifi manager
  //wifiManager.resetSettings();
  
  //wifiManager.setAPCallback(configModeCallback);

  //or use this for auto generated name ESP + ChipID
  wifiManager.autoConnect();

  // Set center of the map by using a WiFi fingerprint
  // Hardcode the values if it doesn't work or you want another location
  locator.updateLocation();
  mapCenter.lat = locator.getLat().toFloat();
  mapCenter.lon = locator.getLon().toFloat();
  
  geoMap.downloadMap(mapCenter, MAP_SCALE, _downloadCallback);
  
}


void loop() {

  adsbClient.updateVisibleAircraft(QUERY_STRING + "&lat=" + String(mapCenter.lat) + "&lng=" + String(mapCenter.lon));
  
  planeSpotter.drawSPIFFSJpeg(geoMap.getMapName(), 0, 0);

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  Aircraft closestAircraft = adsbClient.getClosestAircraft(mapCenter.lat, mapCenter.lon);
  for (int i = 0; i < adsbClient.getNumberOfAircrafts(); i++) {
    Aircraft aircraft = adsbClient.getAircraft(i);
    planeSpotter.drawPlane(aircraft, aircraft.call == closestAircraft.call);
    AircraftHistory history = adsbClient.getAircraftHistory(i);
    planeSpotter.drawAircraftHistory(history);
  }
  planeSpotter.drawInfoBox(closestAircraft);

  // Draw center of map
  CoordinatesPixel p = geoMap.convertToPixel(mapCenter);
  tft.fillCircle(p.x, p.y, 2, TFT_BLUE); 

  delay(2000);

}



void downloadCallback(String filename, uint32_t bytesDownloaded, uint32_t bytesTotal) {
  Serial.println(String(bytesDownloaded) + " / " + String(bytesTotal));
  int width = 320 - 2 * 10;
  int progress = width * bytesDownloaded / bytesTotal;
  tft.fillRect(10, 220, progress, 5, TFT_WHITE);
  planeSpotter.drawSPIFFSJpeg("/plane.jpg", 15 + progress, 220 - 15);
}


