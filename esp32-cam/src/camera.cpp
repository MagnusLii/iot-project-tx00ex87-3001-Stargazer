#include "camera.hpp"
#include "debug.hpp"
#include "esp_camera.h"
#include "timesync-lib.hpp"
#include "requestHandler.hpp"

Camera::Camera(std::shared_ptr<SDcard> sdcardPtr, QueueHandle_t webSrvRequestQueueHandle, int PWDN, int RESET,
               int XCLK, int SIOD, int SIOC, int D7, int D6, int D5, int D4, int D3, int D2, int D1, int D0, int VSYNC,
               int HREF, int PCLK, int XCLK_FREQ, ledc_timer_t LEDC_TIMER, ledc_channel_t LEDC_CHANNEL,
               pixformat_t PIXEL_FORMAT, std::string mount_point, framesize_t FRAME_SIZE, int jpeg_quality,
               int fb_count) {

    DEBUG("Camera constructor called\n");
    DEBUG("PWDN: ", PWDN, "\nRESET: ", RESET, "\nXCLK: ", XCLK, "\nSIOD: ", SIOD, "\nSIOC: ", SIOC, "\nD7: ", D7,
          "\nD6: ", D6, "\nD5: ", D5, "\nD4: ", D4, "\nD3: ", D3, "\nD2: ", D2, "\nD1: ", D1, "\nD0: ", D0,
          "\nVSYNC: ", VSYNC, "\nHREF: ", HREF, "\nPCLK: ", PCLK, "\nXCLK_FREQ: ", XCLK_FREQ,
          "\nLEDC_TIMER: ", LEDC_TIMER, "\nLEDC_CHANNEL: ", LEDC_CHANNEL, "\nPIXEL_FORMAT: ", PIXEL_FORMAT,
          "\nFRAME_SIZE: ", FRAME_SIZE, "\nJPEG_QUALITY: ", jpeg_quality, "\nFB_COUNT: ", fb_count);

    this->sdcard = sdcard;

    this->camera_config.pin_pwdn = PWDN;
    this->camera_config.pin_reset = RESET;
    this->camera_config.pin_xclk = XCLK;
    this->camera_config.pin_sccb_sda = SIOD;
    this->camera_config.pin_sccb_scl = SIOC;

    this->camera_config.pin_d7 = D7;
    this->camera_config.pin_d6 = D6;
    this->camera_config.pin_d5 = D5;
    this->camera_config.pin_d4 = D4;
    this->camera_config.pin_d3 = D3;
    this->camera_config.pin_d2 = D2;
    this->camera_config.pin_d1 = D1;
    this->camera_config.pin_d0 = D0;
    this->camera_config.pin_vsync = VSYNC;
    this->camera_config.pin_href = HREF;
    this->camera_config.pin_pclk = PCLK;

    this->camera_config.xclk_freq_hz = 20000000;
    this->camera_config.ledc_timer = LEDC_TIMER;
    this->camera_config.ledc_channel = LEDC_CHANNEL;

    this->camera_config.pixel_format = PIXEL_FORMAT;
    this->image_filetype = Filetype::JPEG;
    this->webSrvRequestQueueHandle = webSrvRequestQueueHandle;

    this->camera_config.frame_size = FRAME_SIZE;
    this->camera_config.jpeg_quality = jpeg_quality;
    this->camera_config.fb_count = fb_count;
    this->mount_point = mount_point;

    esp_err_t err = esp_camera_init(&camera_config);
    if (err != ESP_OK) {
        DEBUG("Camera init failed with error: ", err);
        // TODO: Handle error
    }
}

CameraReturnCode Camera::reinit_cam() {
    esp_err_t err = esp_camera_deinit();
    if (err != ESP_OK) {
        DEBUG("Camera deinit failed with error: ", err);
        // TODO: Handle error
    } else {
        err = esp_camera_init(&camera_config);
        if (err != ESP_OK) {
            DEBUG("Camera reinit failed with error: ", err);
            return CameraReturnCode::GENERIC_ERROR; // TODO:: redefine error.
        } else {
        }
    }
    DEBUG("Camera reinit successful\n");
    return CameraReturnCode::SUCCESS;
}

CameraReturnCode Camera::take_picture_and_save_to_sdcard(const char *full_filename_str) {
    camera_fb_t *pic = esp_camera_fb_get();
    if (pic) {
        FILE *file = fopen(full_filename_str, "w");

        DEBUG("Writing picture to ", full_filename_str);
        if (file) { fwrite(pic->buf, 1, pic->len, file); }
        esp_camera_fb_return(pic);
        fclose(file);
        DEBUG("Picture saved");
        return CameraReturnCode::SUCCESS;
    }
    return CameraReturnCode::GENERIC_ERROR;
}

CameraReturnCode Camera::create_image_filename(std::string &filenamePtr) {
    char datetime[IMAGE_NAME_MAX_LENGTH];
    if (get_localtime_string(datetime, IMAGE_NAME_MAX_LENGTH) == timeSyncLibReturnCodes::SUCCESS) {
        filenamePtr = this->mount_point + "/" + datetime + image_format_strings[(int)this->image_filetype];
        return CameraReturnCode::SUCCESS;
    }
    return CameraReturnCode::GENERIC_ERROR;
}

CameraReturnCode Camera::notify_request_handler_of_image(const char *filename) {
    QueueMessage message;
    message.requestType = RequestType::POST_IMAGE;
    strncpy(message.imageFilename, filename, BUFFER_SIZE);
    if (xQueueSend(this->webSrvRequestQueueHandle, &message, 0) != pdTRUE) {
        DEBUG("Failed to send message to queue");
        return CameraReturnCode::GENERIC_ERROR;
    }
    return CameraReturnCode::SUCCESS;
}

void take_picture_and_save_to_sdcard_in_loop_task(void *pvParameters) {
    Camera *cameraPtr = (Camera *)pvParameters;
    std::string filepath;

    while (true) {
        cameraPtr->create_image_filename(filepath);
        cameraPtr->take_picture_and_save_to_sdcard(filepath.c_str());

        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}