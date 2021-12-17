#ifndef OV2640_H_
#define OV2640_H_

#include <Arduino.h>
#include <pgmspace.h>
#include <cstdio>
#include "esp_log.h"
#include "esp_attr.h"
#include "esp_camera.h"

extern camera_config_t esp32cam_aithinker_config;

class cameraModule
{
public:
    cameraModule(){
        fb = nullptr;
    };
    ~cameraModule()= default;;
    esp_err_t init(camera_config_t config);
    void run();
    size_t getSize();
    uint8_t *getfb();
    int getWidth();
    int getHeight();
    framesize_t getFrameSize();
    pixformat_t getPixelFormat();

    void setFrameSize(framesize_t size);
    void setPixelFormat(pixformat_t format);

private:
    void runIfNeeded(); // grab a frame if we don't already have one

    // camera_framesize_t _frame_size;
    // camera_pixelformat_t _pixel_format;
    camera_config_t _cam_config{};

    camera_fb_t *fb;
};

#endif //OV2640_H_
