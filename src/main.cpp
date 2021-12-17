#include "main.h"

/** Put your WiFi credentials into this file **/
#include "wifikeys.h"

/** Camera class */
cameraModule cam;

/** 
 * Called once after reboot/powerup
 */
void setup()
{
	// Start the serial connection
	Serial.begin(115200);

	Serial.println("\n\n##################################");
	Serial.printf("Internal Total heap %d, internal Free Heap %d\n", ESP.getHeapSize(), ESP.getFreeHeap());
	Serial.printf("SPIRam Total heap %d, SPIRam Free Heap %d\n", ESP.getPsramSize(), ESP.getFreePsram());
	Serial.printf("ChipRevision %d, Cpu Freq %d, SDK Version %s\n", ESP.getChipRevision(), ESP.getCpuFreqMHz(), ESP.getSdkVersion());
	Serial.printf("Flash Size %d, Flash Speed %d\n", ESP.getFlashChipSize(), ESP.getFlashChipSpeed());
	Serial.println("##################################\n\n");

	// Initialize the ESP32 CAM, here we use the AIthinker ESP32 CAM
	delay(100);
	cam.init(esp32cam_aithinker_config);
	delay(100);

	// Connect the WiFi
	WiFiClass::mode(WIFI_STA);
	WiFi.begin(ssid, password);
	while (WiFiClass::status() != WL_CONNECTED)
	{
		delay(500);
		Serial.print(".");
	}

	// Print information how to contact the camera server
	IPAddress ip = WiFi.localIP();
	Serial.print("\nWiFi connected with IP ");
	Serial.println(ip);

	Serial.print("Browser Stream Link: http://");
	Serial.print(ip);
	Serial.println("\n");
	Serial.print("Browser Single Picture Link: http//");
	Serial.print(ip);
	Serial.println("/jpg\n");

	// Initialize the HTTP web stream server
	initWebStream();
}

void loop()
{
	delay(1);
}
