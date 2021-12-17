#include <Arduino.h>

// WiFi stuff
#include <WiFi.h>
#include <WebServer.h>
#include <WiFiClient.h>

// Camera stuff
#include "cameraModule.h"

// Camera class
extern cameraModule cam;

// Web server stuff
void initWebStream();
