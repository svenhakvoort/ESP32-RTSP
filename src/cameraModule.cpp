#include "cameraModule.h"

camera_config_t esp32cam_aithinker_config {

    .pin_pwdn = 32,
    .pin_reset = -1,

    .pin_xclk = 0,

    .pin_sscb_sda = 26,
    .pin_sscb_scl = 27,

    // Note: LED GPIO is apparently 4 not sure where that goes
    // per https://github.com/donny681/ESP32_CAMERA_QR/blob/e4ef44549876457cd841f33a0892c82a71f35358/main/led.c
    .pin_d7 = 35,
    .pin_d6 = 34,
    .pin_d5 = 39,
    .pin_d4 = 36,
    .pin_d3 = 21,
    .pin_d2 = 19,
    .pin_d1 = 18,
    .pin_d0 = 5,
    .pin_vsync = 25,
    .pin_href = 23,
    .pin_pclk = 22,
    .xclk_freq_hz = 20000000,
    .ledc_timer = LEDC_TIMER_1,
    .ledc_channel = LEDC_CHANNEL_1,
    .pixel_format = PIXFORMAT_JPEG,
    .frame_size = FRAMESIZE_SVGA,
    .jpeg_quality = 12,               //0-63 lower numbers are higher quality
    .fb_count = 2 // if more than one i2s runs in continous mode.  Use only with jpeg
};

void cameraModule::run()
{
    if(fb)
        //return the frame buffer back to the driver for reuse
        esp_camera_fb_return(fb);

    fb = esp_camera_fb_get();
}

void cameraModule::runIfNeeded()
{
    if(!fb)
        run();
}

int cameraModule::getWidth()
{
    runIfNeeded();
    return fb->width;
}

int cameraModule::getHeight()
{
    runIfNeeded();
    return fb->height;
}

size_t cameraModule::getSize()
{
    runIfNeeded();
    return fb->len;
}

uint8_t *cameraModule::getfb()
{
    runIfNeeded();
    return fb->buf;
}

framesize_t cameraModule::getFrameSize()
{
    return _cam_config.frame_size;
}

void cameraModule::setFrameSize(framesize_t size)
{
    _cam_config.frame_size = size;
}

pixformat_t cameraModule::getPixelFormat()
{
    return _cam_config.pixel_format;
}

void cameraModule::setPixelFormat(pixformat_t format)
{
    switch (format)
    {
    case PIXFORMAT_RGB565:
    case PIXFORMAT_YUV422:
    case PIXFORMAT_GRAYSCALE:
    case PIXFORMAT_JPEG:
        _cam_config.pixel_format = format;
        break;
    default:
        _cam_config.pixel_format = PIXFORMAT_GRAYSCALE;
        break;
    }
}


esp_err_t cameraModule::init(camera_config_t config)
{
    memset(&_cam_config, 0, sizeof(_cam_config));
    memcpy(&_cam_config, &config, sizeof(config));

    esp_err_t err = esp_camera_init(&_cam_config);
    if (err != ESP_OK)
    {
        printf("Camera probe failed with error 0x%x", err);
        return err;
    }
    // ESP_ERROR_CHECK(gpio_install_isr_service(0));

    return ESP_OK;
}
