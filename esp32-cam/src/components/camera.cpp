#include "camera.hpp"
#include "debug.hpp"
#include "esp_camera.h"

Camera::Camera(std::shared_ptr<SDcard> sdcard, int PWDN, int RESET, int XCLK, int SIOD, int SIOC, int D7, int D6,
               int D5, int D4, int D3, int D2, int D1, int D0, int VSYNC, int HREF, int PCLK, int XCLK_FREQ,
               ledc_timer_t LEDC_TIMER, ledc_channel_t LEDC_CHANNEL, pixformat_t PIXEL_FORMAT, framesize_t FRAME_SIZE,
               int jpeg_quality, int fb_count) {

    // DEBUG("Camera constructor called\n");
    // DEBUG("PWDN: %d\nRESET: %dXCLK: %d\nSIOD: %d\nSIOC: %d\nD7: %d\nD6: %d\nD5: %d\nD4: %d\nD3: %d\nD2: %d\nD1: "
    //       "%d\nD0: %d\nVSYNC: %d\nHREF: %d\nPCLK: %d\nXCLK_FREQ: %d\nLEDC_TIMER: %d\nLEDC_CHANNEL: %d\nPIXEL_FORMAT: "
    //       "%d\nFRAME_SIZE: %d\nJPEG_QUALITY: %d\nFB_COUNT: %d\n",
    //       PWDN, RESET, XCLK, SIOD, SIOC, D7, D6, D5, D4, D3, D2, D1, D0, VSYNC, HREF, PCLK, XCLK_FREQ, LEDC_TIMER,
    //       LEDC_CHANNEL, PIXEL_FORMAT, FRAME_SIZE, jpeg_quality, fb_count);

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

    this->camera_config.frame_size = FRAME_SIZE;
    this->camera_config.jpeg_quality = jpeg_quality;
    this->camera_config.fb_count = fb_count;

    esp_err_t err = esp_camera_init(&camera_config);
    if (err != ESP_OK) {
        // DEBUG("Camera init failed with error 0x%x\n", err);
        // TODO: Handle error
    }
}

int Camera::reinit_cam() {
    esp_err_t err = esp_camera_deinit();
    if (err != ESP_OK) {
        // DEBUG("Camera deinit failed with error 0x%x\n", err);
        // TODO: Handle error
    } else {
        err = esp_camera_init(&camera_config);
        if (err != ESP_OK) {
            // DEBUG("Camera reinit failed with error 0x%x\n", err);
            return 1;
        } else {
        }
    }
    DEBUG("Camera reinit successful\n");
    return 0; // Failure
}

int Camera::take_picture_and_save_to_sdcard(std::string mount_point, std::string filename) {
    camera_fb_t *pic = esp_camera_fb_get();
    if (pic) {
        std::string path = mount_point + "/" + filename + image_format_strings[(int)image_filetype];
        FILE *file = fopen(path.c_str(), "w");

        // DEBUG("Writing picture to %s\n", path.c_str());
        if (file) {
            fwrite(pic->buf, 1, pic->len, file);
            esp_camera_fb_return(pic);
            fclose(file);
            // DEBUG("Picture saved\n");

            return 0;
        }
    }
    return 1; // Failure
}

void take_picture_and_save_to_sdcard_in_loop_task(void *pvParameters) {
    std::shared_ptr<SDcard> sdcard_ptr = std::make_shared<SDcard>("/sdcard");
    Camera camera(sdcard_ptr);

    std::string mount_point = "/sdcard";
    std::string filename = "image";
    int counter = 0;

    while (true){
        std::string complete_filename = filename + std::to_string(counter); 
        camera.take_picture_and_save_to_sdcard(mount_point, complete_filename);
        counter++;
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }

}