#include "main.h"

/** Web server class */
WebServer server(80);

// ESP32 has two cores: APPlication core and PROcess core (the one that runs ESP32 SDK stack)
#define APP_CPU 1
#define PRO_CPU 0

/** Forward dedclaration of the task handling browser requests */
void webTask(void *pvParameters);
void camCallback(void *pvParameters);
[[noreturn]] void streamCallback(void *pvParameters);

/** Task handle of the web task */

/** Web request function forward declarations */
void handle_jpg_stream();
void handle_jpg();
void handleNotFound();

// ===== rtos task handles =========================
// Streaming is implemented with 3 tasks:
TaskHandle_t webTaskHandler;
TaskHandle_t camTaskHandler;     // handles getting picture frames from the camera and storing them locally
TaskHandle_t streamTaskHandler;  // actually streaming frames to all connected clients

/**
 * Initialize the web stream server by starting the handler task
 */
void initWebStream() {
    // Create the task for the web server
    xTaskCreate(webTask, "WEB", 4096, nullptr, 1, &webTaskHandler);

    if (webTaskHandler == nullptr) {
        Serial.println("Create Webstream task failed");
    } else {
        Serial.println("Webstream task up and running");
    }
}

const int FPS = 30;
// We will handle web client requests at a certain speed according to FPS settings
const int WSINTERVAL = 1000 / FPS;

// frameSync semaphore is used to prevent streaming buffer as it is replaced with the next frame
SemaphoreHandle_t frameSync = nullptr;

// Queue stores currently connected clients to whom we are streaming
QueueHandle_t streamingClients;

// Commonly used variables:
volatile size_t camSize;    // size of the current frame, byte
volatile char *camBuf;      // pointer to the current frame


/**
 * The task that handles web server connections
 * Starts the web server
 * Handles requests in an endless loop
 * until a stop request is received because OTA
 * starts
 */
void webTask(void *pvParameters) {
    // Creating frame synchronization semaphore and initializing it
    frameSync = xSemaphoreCreateBinary();
    xSemaphoreGive(frameSync);

    streamingClients = xQueueCreate(10, sizeof(WiFiClient *));

    //  Creating RTOS task for grabbing frames from the camera
    xTaskCreatePinnedToCore(
            camCallback,        // callback
            "cam",        // name
            4096,         // stacj size
            nullptr,         // parameters
            2,            // priority
            &camTaskHandler,        // RTOS task handle
            APP_CPU);     // core

    //  Creating task to push the stream to all connected clients
    xTaskCreatePinnedToCore(
            streamCallback,
            "strmCB",
            4 * 1024,
            nullptr, //(void*) handler,
            2,
            &streamTaskHandler,
            APP_CPU);

    // Set the function to handle stream requests
    server.on("/", HTTP_GET, handle_jpg_stream);
    // Set the function to handle single picture requests
    server.on("/jpg", HTTP_GET, handle_jpg);
    // Set the function to handle other requests
    server.onNotFound(handleNotFound);
    // Start the web server
    server.begin();

    const TickType_t xFrequency = pdMS_TO_TICKS(WSINTERVAL);
    TickType_t xLastWakeTime = xTaskGetTickCount();
    while (true) {
        // Check if the server has clients
        server.handleClient();
        taskYIELD();
        vTaskDelayUntil(&xLastWakeTime, xFrequency)
    }
}

char *allocateMemory(char *aPtr, size_t aSize) {

    //  Since current buffer is too smal, free it
    if (aPtr != nullptr) free(aPtr);


    size_t freeHeap = ESP.getFreeHeap();
    char *ptr = nullptr;

    // If memory requested is more than 2/3 of the currently free heap, try PSRAM immediately
    if (aSize > freeHeap * 2 / 3) {
        if (psramFound() && ESP.getFreePsram() > aSize) {
            ptr = (char *) ps_malloc(aSize);
        }
    } else {
        //  Enough free heap - let's try allocating fast RAM as a buffer
        ptr = (char *) malloc(aSize);

        //  If allocation on the heap failed, let's give PSRAM one more chance:
        if (ptr == nullptr && psramFound() && ESP.getFreePsram() > aSize) {
            ptr = (char *) ps_malloc(aSize);
        }
    }

    // Finally, if the memory pointer is nullptr, we were not able to allocate any memory, and that is a terminal condition.
    if (ptr == nullptr) {
        ESP.restart();
    }
    return ptr;
}

void camCallback(void *pvParameters) {

    TickType_t xLastWakeTime;

    //  A running interval associated with currently desired frame rate
    const TickType_t xFrequency = pdMS_TO_TICKS(1000 / FPS);

    // Mutex for the critical section of switching the active frames around
    portMUX_TYPE xSemaphore = portMUX_INITIALIZER_UNLOCKED;

    //  Pointers to the 2 frames, their respective sizes and index of the current frame
    char *frameBuffers[2] = {nullptr, nullptr};
    size_t frameSizes[2] = {0, 0};
    int frameIndex = 0;

    //=== loop() section  ===================
    xLastWakeTime = xTaskGetTickCount();

    for (;;) {

        //  Grab a frame from the camera and query its size
        cam.run();
        size_t frameSize = cam.getSize();

        //  If frame size is more that we have previously allocated - request  125% of the current frame space
        if (frameSize > frameSizes[frameIndex]) {
            frameSizes[frameIndex] = frameSize * 4 / 3;
            frameBuffers[frameIndex] = allocateMemory(frameBuffers[frameIndex], frameSizes[frameIndex]);
        }

        //  Copy current frame into local buffer
        char *b = (char *) cam.getfb();
        memcpy(frameBuffers[frameIndex], b, frameSize);

        //  Let other tasks run and wait until the end of the current frame rate interval (if any time left)
        taskYIELD();
        vTaskDelayUntil(&xLastWakeTime, xFrequency)

        //  Only switch frames around if no frame is currently being streamed to a client
        //  Wait on a semaphore until client operation completes
        xSemaphoreTake(frameSync, portMAX_DELAY);

        //  Do not allow interrupts while switching the current frame
        portENTER_CRITICAL(&xSemaphore);
        camBuf = frameBuffers[frameIndex];
        camSize = frameSize;
        frameIndex++;
        frameIndex &= 1;  // this should produce 1, 0, 1, 0, 1 ... sequence
        portEXIT_CRITICAL(&xSemaphore);

        //  Let anyone waiting for a frame know that the frame is ready
        xSemaphoreGive(frameSync);

        //  Technically only needed once: let the streaming task know that we have at least one frame
        //  and it could start sending frames to the clients, if any
        xTaskNotifyGive(streamTaskHandler);

        //  Immediately let other (streaming) tasks run
        taskYIELD();

        //  If streaming task has suspended itself (no active clients to stream to)
        //  there is no need to grab frames from the camera. We can save some juice
        //  by suspedning the tasks
        if (eTaskGetState(streamTaskHandler) == eSuspended) {
            vTaskSuspend(nullptr);  // passing nullptr means "suspend yourself"
        }
    }
}

const char JHEADER[] = "HTTP/1.1 200 OK\r\n" \
                       "Content-disposition: inline; filename=capture.jpg\r\n" \
                       "Content-type: image/jpeg\r\n\r\n";
const size_t jhdLen = strlen(JHEADER);

/**
 * Handle single picture requests
 * Gets the latest picture from the camera
 * and sends it to the web client
 */
void handle_jpg() {
    WiFiClient client = server.client();

    if (!client.connected()) return;
    cam.run();
    client.write(JHEADER, jhdLen);
    client.write((char *) cam.getfb(), cam.getSize());
}

/**
 * Handle any other request from the web client
 */
void handleNotFound() {
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

// ==== STREAMING ======================================================
const char HEADER[] = "HTTP/1.1 200 OK\r\n" \
                      "Access-Control-Allow-Origin: *\r\n" \
                      "Content-Type: multipart/x-mixed-replace; boundary=123456789000000000000987654321\r\n";
const char BOUNDARY[] = "\r\n--123456789000000000000987654321\r\n";
const char CTNTTYPE[] = "Content-Type: image/jpeg\r\nContent-Length: ";
const size_t hdrLen = strlen(HEADER);
const size_t bdrLen = strlen(BOUNDARY);
const size_t cntLen = strlen(CTNTTYPE);

[[noreturn]] void streamCallback(void *pvParameters) {
    char buf[16];
    TickType_t xLastWakeTime;
    TickType_t xFrequency;

    //  Wait until the first frame is captured and there is something to send
    //  to clients
    ulTaskNotifyTake(pdTRUE,          /* Clear the notification value before exiting. */
                     portMAX_DELAY); /* Block indefinitely. */

    xLastWakeTime = xTaskGetTickCount();
    for (;;) {
        // Default assumption we are running according to the FPS
        xFrequency = pdMS_TO_TICKS(1000 / FPS);

        //  Only bother to send anything if there is someone watching
        UBaseType_t activeClients = uxQueueMessagesWaiting(streamingClients);
        if (activeClients) {
            // Adjust the period to the number of connected clients
            xFrequency /= activeClients;

            //  Since we are sending the same frame to everyone,
            //  pop a client from the the front of the queue
            WiFiClient *client;
            xQueueReceive(streamingClients, (void *) &client, 0);

            //  Check if this client is still connected.

            if (!client->connected()) {
                //  delete this client reference if s/he has disconnected
                //  and don't put it back on the queue anymore. Bye!
                delete client;
            } else {

                //  Ok. This is an actively connected client.
                //  Let's grab a semaphore to prevent frame changes while we
                //  are serving this frame
                xSemaphoreTake(frameSync, portMAX_DELAY);

                client->write(CTNTTYPE, cntLen);
                sprintf(buf, "%d\r\n\r\n", camSize);
                client->write(buf, strlen(buf));
                client->write((char *) camBuf, (size_t) camSize);
                client->write(BOUNDARY, bdrLen);

                // Since this client is still connected, push it to the end
                // of the queue for further processing
                xQueueSend(streamingClients, (void *) &client, 0);

                //  The frame has been served. Release the semaphore and let other tasks run.
                //  If there is a frame switch ready, it will happen now in between frames
                xSemaphoreGive(frameSync);
                taskYIELD();
            }
        } else {
            //  Since there are no connected clients, there is no reason to waste battery running
            vTaskSuspend(nullptr);
        }
        //  Let other tasks run after serving every client
        taskYIELD();
        vTaskDelayUntil(&xLastWakeTime, xFrequency)
    }
}

/**
 * Handle web stream requests
 * Gives a first response to prepare the streaming
 * Then runs in a loop to update the web content
 * every time a new frame is available
 */
void handle_jpg_stream() {
    //  Can only acommodate 10 clients. The limit is a default for WiFi connections
    if (!uxQueueSpacesAvailable(streamingClients)) return;


    //  Create a new WiFi Client object to keep track of this one
    WiFiClient* client = new WiFiClient();
    *client = server.client();

    //  Immediately send this client a header
    client->write(HEADER, hdrLen);
    client->write(BOUNDARY, bdrLen);

    // Push the client to the streaming queue
    xQueueSend(streamingClients, (void *) &client, 0);

    // Wake up streaming tasks, if they were previously suspended:
    if (eTaskGetState(camTaskHandler) == eSuspended) vTaskResume(camTaskHandler);
    if (eTaskGetState(streamTaskHandler) == eSuspended) vTaskResume(streamTaskHandler);
}

