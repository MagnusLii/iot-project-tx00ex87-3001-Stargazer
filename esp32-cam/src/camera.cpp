#include "camera.hpp"
#include "debug.hpp"
#include "esp_camera.h"
#include "requestHandler.hpp"
#include "timesync-lib.hpp"

/**
 * @brief Constructs a CameraHandler object with the specified configuration.
 *
 * This constructor initializes the camera with the provided pin assignments, configuration parameters,
 * and SD card reference. It also initializes the camera hardware and handles any potential errors during
 * initialization.
 *
 * @param sdcardPtr A shared pointer to an SDcardHandler object for accessing storage.
 * @param webSrvRequestQueueHandle A handle to the web server request queue.
 * @param PWDN, RESET, XCLK, SIOD, SIOC, D7, D6, D5, D4, D3, D2, D1, D0, VSYNC, HREF, PCLK Pin assignments for the
 * camera hardware.
 * @param XCLK_FREQ The clock frequency for the camera.
 * @param LEDC_TIMER The LEDC timer for controlling the camera's clock.
 * @param LEDC_CHANNEL The LEDC channel for controlling the camera's clock.
 * @param PIXEL_FORMAT The pixel format to be used for the camera.
 * @param FRAME_SIZE The frame size for the camera.
 * @param jpeg_quality The quality of the JPEG image to be captured.
 * @param fb_count The number of frame buffers to be used by the camera.
 *
 * @return void This function does not return any value.
 *
 * @note This constructor initializes the camera with the provided settings, mounts the SD card,
 * and handles errors during the camera initialization process.
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
 * @brief Destroys the CameraHandler object and cleans up resources.
 *
 * This destructor ensures that any allocated resources for the camera are released. It handles any
 * necessary cleanup of the camera's configuration and deallocates any memory if required.
 *
 * @return void This function does not return any value.
 *
 * @note The destructor cleans up camera resources to avoid memory leaks or undefined behavior.
 */
CameraHandler::~CameraHandler() {
    esp_err_t err = esp_camera_deinit();
    if (err != ESP_OK) {
        DEBUG("CameraHandler deinit failed with error: ", err);
    } else {
        DEBUG("CameraHandler deinitialized successfully");
    }
}

/**
 * @brief Reinitializes the camera by deinitializing and then reinitializing it.
 *
 * This function attempts to deinitialize and then reinitialize the camera hardware with the current configuration.
 * If any step fails, it logs the error and returns a specific error code.
 *
 * @return int Returns `0` if the camera reinitialization is successful,
 *             `1` if deinitialization fails, and `2` if reinitialization fails.
 *
 * @note This function is used to reset the camera hardware.
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
 * @brief Captures an image using the camera and saves it to the SD card.
 *
 * This function captures an image from the camera, retrieves the image buffer,
 * and writes it to the specified file on the SD card.
 *
 * @param full_filename_str The full file path (including mount point and filename) where the image will be saved.
 *
 * @return int Returns:
 *  - `0`: Success.
 *  - `1`: Image capture failure.
 *  - `2`: Failed to write image to file.
 *
 * @note The function retrieves a frame buffer from the camera and ensures the buffer
 *       is released after writing to the file.
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
 * @brief Appends a timestamp-based filename with the appropriate image file extension.
 *
 * This function retrieves the current local time as a string and appends it to the provided
 * filename reference. The appropriate file extension is also added based on the camera's
 * image file type.
 *
 * @param filenamePtr Reference to a string where the generated filename will be appended.
 *
 * @return int Returns:
 *  - `0`: Success, filename generated successfully.
 *  - `1`: Failed to retrieve local time.
 *
 * @note Ensure that time synchronization is properly set up before calling this function.
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
 * @brief Notifies the request handler of a new image by sending a message to the queue.
 *
 * This function constructs a queue message containing the image filename and sends it to
 * the request handler queue for further processing.
 *
 * @param filename The name of the image file to be sent.
 *
 * @return int Returns:
 *  - `1`: Success, message sent to the queue.
 *  - `0`: Failed to send message to the queue.
 *
 * @note The function uses a queue to communicate with the request handler. Ensure that
 *       `webSrvRequestQueueHandle` is properly initialized before calling this function.
 */
int CameraHandler::notify_request_handler_of_image(const char *filename) {
    QueueMessage message;
    message.requestType = RequestType::POST_IMAGE;
    strncpy(message.imageFilename, filename, BUFFER_SIZE);
    if (xQueueSend(this->webSrvRequestQueueHandle, &message, 0) != pdTRUE) {
        DEBUG("Failed to send message to queue");
        return 0;
    }
    return 1;
}
