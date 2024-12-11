#ifndef CAMERA_HPP
#define CAMERA_HPP

#include "esp_camera.h"
#include "esp_vfs_fat.h"
#include "sd-card.hpp"
#include <memory>
#include <string>

#define CAM_PIN_PWDN  32
#define CAM_PIN_RESET -1 // software reset will be performed
#define CAM_PIN_XCLK  0
#define CAM_PIN_SIOD  26
#define CAM_PIN_SIOC  27

#define CAM_PIN_D7    35
#define CAM_PIN_D6    34
#define CAM_PIN_D5    39
#define CAM_PIN_D4    36
#define CAM_PIN_D3    21
#define CAM_PIN_D2    19
#define CAM_PIN_D1    18
#define CAM_PIN_D0    5
#define CAM_PIN_VSYNC 25
#define CAM_PIN_HREF  23
#define CAM_PIN_PCLK  22
#define CAM_XCLK_FREQ 20000000

enum class CameraReturnCode {
    SUCCESS,
    CAMERA_INIT_FAILED,
    CAMERA_DEINIT_FAILED,
    CAMERA_REINIT_FAILED,
    CAMERA_CAPTURE_FAILED,
    CAMERA_SAVE_FAILED
};

class Camera {
  public:
    enum class Filetype {
        RGB565 = PIXFORMAT_RGB565,
        YUV422 = PIXFORMAT_YUV422,
        DO_NOT_USE = PIXFORMAT_YUV420,
        GRAYSCALE = PIXFORMAT_GRAYSCALE,
        JPEG = PIXFORMAT_JPEG
    };

    Camera(std::shared_ptr<SDcard> sdcard, int PWDN = CAM_PIN_PWDN, int RESET = CAM_PIN_RESET, int XCLK = CAM_PIN_XCLK,
           int SIOD = CAM_PIN_SIOD, int SIOC = CAM_PIN_SIOC, int D7 = CAM_PIN_D7, int D6 = CAM_PIN_D6,
           int D5 = CAM_PIN_D5, int D4 = CAM_PIN_D4, int D3 = CAM_PIN_D3, int D2 = CAM_PIN_D2, int D1 = CAM_PIN_D1,
           int D0 = CAM_PIN_D0, int VSYNC = CAM_PIN_VSYNC, int HREF = CAM_PIN_HREF, int PCLK = CAM_PIN_PCLK,
           int XCLK_FREQ = CAM_XCLK_FREQ, ledc_timer_t LEDC_TIMER = LEDC_TIMER_0,
           ledc_channel_t LEDC_CHANNEL = LEDC_CHANNEL_0, pixformat_t PIXEL_FORMAT = PIXFORMAT_JPEG,
           framesize_t FRAME_SIZE = FRAMESIZE_UXGA, int jpeg_quality = 8, int fb_count = 1);

    int reinit_cam();
    int take_picture_and_save_to_sdcard(std::string mount_point = "/sdcard", std::string filename = "image.jpg");

  private:
    std::shared_ptr<SDcard> sdcard;
    camera_config_t camera_config;
    Filetype image_filetype;
    const char *image_format_strings[5] = {".rgb", ".yuv", "INVALID-FILETYPE", ".gray", ".jpg"};
};

void take_picture_and_save_to_sdcard_in_loop_task(void *pvParameters);

#endif