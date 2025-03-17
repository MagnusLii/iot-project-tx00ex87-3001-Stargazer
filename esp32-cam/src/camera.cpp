#include "camera.hpp"
#include "debug.hpp"
#include "esp_camera.h"
#include "requestHandler.hpp"
#include "timesync-lib.hpp"

/**
 * @brief Constructs a CameraHandler object and initializes the camera module.
 *
 * This constructor configures and initializes the ESP32 camera module with the specified
 * pin assignments, image settings, and hardware configurations. It also attempts to 
 * recover from initialization failures before restarting the system if necessary.
 *
 * @param sdcardPtr Shared pointer to an SD card handler for storing captured images.
 * @param webSrvRequestQueueHandle Handle to a FreeRTOS queue for web server requests.
 * @param PWDN Power-down pin for the camera module.
 * @param RESET Reset pin for the camera module.
 * @param XCLK External clock pin for the camera.
 * @param SIOD SCCB data pin.
 * @param SIOC SCCB clock pin.
 * @param D7-D0 Data pins for image capture.
 * @param VSYNC Vertical sync pin.
 * @param HREF Horizontal reference pin.
 * @param PCLK Pixel clock pin.
 * @param XCLK_FREQ External clock frequency in Hz.
 * @param LEDC_TIMER LEDC timer used for generating the XCLK signal.
 * @param LEDC_CHANNEL LEDC channel used for XCLK generation.
 * @param PIXEL_FORMAT Image pixel format (e.g., RGB, JPEG).
 * @param FRAME_SIZE Image resolution/frame size.
 * @param jpeg_quality JPEG compression quality (lower is better quality).
 * @param fb_count Number of frame buffers for capturing images.
 *
 * @note If camera initialization fails, the constructor attempts a retry. If the second attempt fails, 
 *       the system is restarted using `esp_restart()`.
 */
CameraHandler::CameraHandler(std::shared_ptr<SDcardHandler> sdcardPtr, QueueHandle_t webSrvRequestQueueHandle, int PWDN,
                             int RESET, int XCLK, int SIOD, int SIOC, int D7, int D6, int D5, int D4, int D3, int D2,
                             int D1, int D0, int VSYNC, int HREF, int PCLK, int XCLK_FREQ, ledc_timer_t LEDC_TIMER,
                             ledc_channel_t LEDC_CHANNEL, pixformat_t PIXEL_FORMAT, framesize_t FRAME_SIZE,
                             int jpeg_quality, int fb_count) {
    DEBUG("CameraHandler constructor called\n");

    this->sdcardHandler = sdcardPtr;
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

    DEBUG("Initializing camera");
    esp_err_t err = esp_camera_init(&camera_config);
    if (err != ESP_OK) {
        DEBUG("CameraHandler init failed with error: %d", err);

        // Handle specific error codes.
        switch (err) {
            case ESP_ERR_NO_MEM:
                DEBUG("Error: Insufficient memory for camera initialization.");
                break;
            case ESP_ERR_INVALID_ARG:
                DEBUG("Error: Invalid camera configuration.");
                break;
            case ESP_ERR_INVALID_STATE:
                DEBUG("Error: Camera already initialized or in invalid state.");
                break;
            case ESP_ERR_NOT_FOUND:
                DEBUG("Error: Camera sensor not found.");
                break;
            default:
                DEBUG("Error: Unknown error occurred.");
                break;
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
        DEBUG("Retrying camera initialization...\n");
        err = esp_camera_init(&camera_config);
        // Restart if it fails again as it's usually unrecoverable.
        if (err != ESP_OK) {
            DEBUG("Camera re-initialization failed. Restarting...");
            esp_restart();
        }
    } else {
        DEBUG("CameraHandler initialized successfully");
    }
}

/**
 * @brief Destructor for the CameraHandler class.
 *
 * This destructor releases resources associated with the camera module by deinitializing
 * the ESP32 camera driver and resetting the shared pointer to the SD card handler.
 *
 * @note Calls `esp_camera_deinit()` to properly shut down the camera hardware.
 *       Resets `sdcardHandler` to release ownership and free resources.
 */
CameraHandler::~CameraHandler() {
    DEBUG("CameraHandler destructor called\n");

    // Deinitialize the camera
    esp_camera_deinit();

    // Nullify the shared pointer to the SD card handler
    this->sdcardHandler.reset();

    DEBUG("CameraHandler resources released\n");
}

/**
 * @brief Reinitializes the camera module.
 *
 * This function deinitializes the camera and attempts to reinitialize it with the 
 * current configuration. It helps recover from camera failures without restarting 
 * the system.
 *
 * @return int Returns:
 * @return        - `0` on successful reinitialization.
 * @return        - `1` if deinitialization fails.
 * @return        - `2` if reinitialization fails.
 *
 * @note If reinitialization fails, the camera remains non-functional until another attempt is made.
 */
int CameraHandler::reinit_cam() {
    esp_err_t err = esp_camera_deinit();
    if (err != ESP_OK) {
        DEBUG("CameraHandler deinit failed with error: ", err);
        return 1; // CameraHandler deinit failure
    }

    err = esp_camera_init(&camera_config);
    if (err != ESP_OK) {
        DEBUG("CameraHandler reinit failed with error: ", err);
        return 2; // CameraHandler reinit failure
    }

    DEBUG("CameraHandler reinit successful\n");
    return 0; // Success
}

/**
 * @brief Captures an image and saves it to the SD card.
 *
 * This function captures an image using the camera module and writes it to the 
 * specified file on the SD card.
 *
 * @param full_filename_str The full path and filename where the image should be saved.
 *
 * @return int Returns:
 * @return        - `0` on success.
 * @return        - `1` if image capture fails.
 * @return        - `2` if writing the image to the SD card fails.
 *
 * @note Attempts to capture an image up to three times before failing.
 *       The captured image buffer is properly returned after processing.
 */
int CameraHandler::take_picture_and_save_to_sdcard(const char *full_filename_str) {
    camera_fb_t *pic = nullptr;
    for (int i = 0; i < 3; i++) {
        esp_camera_fb_return(pic);
        pic = esp_camera_fb_get();
    }
    if (!pic) {
        DEBUG("Failed to capture image");
        return 1; // Image capture failure
    }

    if (this->sdcardHandler->write_file(full_filename_str, pic->buf, pic->len) != 0) {
        esp_camera_fb_return(pic);
        return 2; // Failed to write image to file
    }

    esp_camera_fb_return(pic);

    return 0; // Success
}

/**
 * @brief Generates an image filename with a timestamp.
 *
 * This function retrieves the current local time as a string and appends it to the 
 * provided filename reference, followed by the appropriate image file extension.
 *
 * @param filenamePtr Reference to a string where the generated filename will be stored.
 *
 * @return int Returns:
 * @return        - `0` on success.
 * @return        - `1` if retrieving the local time fails.
 *
 * @note The filename format includes a timestamp followed by the image file extension.
 */
int CameraHandler::create_image_filename(std::string &filenamePtr) {
    char datetime[IMAGE_NAME_MAX_LENGTH];
    if (get_localtime_string(datetime, IMAGE_NAME_MAX_LENGTH) == timeSyncLibReturnCodes::SUCCESS) {
        filenamePtr += datetime;
        filenamePtr += image_format_strings[(int)this->image_filetype];
        return 0; // Success
    }
    return 1; // Local time retrieval failure
}

/**
 * @brief Notifies the request handler of a newly saved image.
 *
 * This function sends a message to the web server request queue, informing it 
 * that a new image is available for processing.
 *
 * @param filename The name of the image file to be sent in the request.
 *
 * @return int Returns:
 * @return        - `0` if the message was successfully added to the queue.
 * @return        - `1` if the queue message could not be sent.
 *
 * @note Uses `xQueueSend` to enqueue the request. If the queue is full, the function fails.
 */
int CameraHandler::notify_request_handler_of_image(const char *filename) {
    QueueMessage message;
    message.requestType = RequestType::POST_IMAGE;
    strncpy(message.imageFilename, filename, BUFFER_SIZE);
    if (xQueueSend(this->webSrvRequestQueueHandle, &message, 0) != pdTRUE) {
        DEBUG("Failed to send message to queue");
        return 1;
    }
    return 0;
}
