#include "main.h"

/** Web server class */
WebServer server(80);

/** Forward dedclaration of the task handling browser requests */
void webTask(void *pvParameters);

/** Task handle of the web task */
TaskHandle_t webTaskHandler;

/** Flag to stop the web server */
boolean stopWeb = false;

/** Web request function forward declarations */
void handle_jpg_stream();
void handle_jpg();
void handleNotFound();

/**
 * Initialize the web stream server by starting the handler task
 */
void initWebStream()
{
	// Create the task for the web server
	xTaskCreate(webTask, "WEB", 4096, nullptr, 1, &webTaskHandler);

	if (webTaskHandler == nullptr)
	{
		Serial.println("Create Webstream task failed");
	}
	else
	{
		Serial.println("Webstream task up and running");
	}
}

/**
 * The task that handles web server connections
 * Starts the web server
 * Handles requests in an endless loop
 * until a stop request is received because OTA
 * starts
 */
void webTask(void *pvParameters)
{
	// Set the function to handle stream requests
	server.on("/", HTTP_GET, handle_jpg_stream);
	// Set the function to handle single picture requests
	server.on("/jpg", HTTP_GET, handle_jpg);
	// Set the function to handle other requests
	server.onNotFound(handleNotFound);
	// Start the web server
	server.begin();

	while (true)
	{
			// Check if the server has clients
			server.handleClient();
		}
		if (stopWeb)
		{
			// User requested web server stop
			server.close();
			// Delete this task
			vTaskDelete(nullptr);
		}
		delay(100);
}

/**
 * Handle web stream requests
 * Gives a first response to prepare the streaming
 * Then runs in a loop to update the web content
 * every time a new frame is available
 */
void handle_jpg_stream()
{
	WiFiClient thisClient = server.client();
	String response = "HTTP/1.1 200 OK\r\n";
	response += "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n\r\n";
	server.sendContent(response);

	while (true)
	{
		cam.run();
		if (!thisClient.connected())
		{
			break;
		}
		response = "--frame\r\n";
		response += "Content-Type: image/jpeg\r\n\r\n";
		server.sendContent(response);

		thisClient.write((char *)cam.getfb(), cam.getSize());
		server.sendContent("\r\n");
		delay(25);
	}
}

/**
 * Handle single picture requests
 * Gets the latest picture from the camera
 * and sends it to the web client
 */
void handle_jpg()
{
	WiFiClient thisClient = server.client();

	cam.run();
	if (!thisClient.connected())
	{
		return;
	}
	String response = "HTTP/1.1 200 OK\r\n";
	response += "Content-disposition: inline; filename=capture.jpg\r\n";
	response += "Content-type: image/jpeg\r\n\r\n";
	server.sendContent(response);
	thisClient.write((char *)cam.getfb(), cam.getSize());
}

/**
 * Handle any other request from the web client
 */
void handleNotFound()
{
	IPAddress ip = WiFi.localIP();
	String message = "Stream Link: rtsp://";
	message += ip.toString();
	message += ":8554/mjpeg/1\n";
	message += "Browser Stream Link: http://";
	message += ip.toString();
	message += "\n";
	message += "Browser Single Picture Link: http//";
	message += ip.toString();
	message += "/jpg\n";
	message += "\n";
	server.send(200, "text/plain", message);
}
