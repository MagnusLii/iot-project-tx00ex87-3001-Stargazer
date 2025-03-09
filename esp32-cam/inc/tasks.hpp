#ifndef TASKS_HPP
#define TASKS_HPP

#include "requestHandler.hpp"
#include "espPicoUartCommHandler.hpp"
#include "sd-card.hpp"
#include "wireless.hpp"
#include "camera.hpp"
#include "requestHandler.hpp"
#include "espPicoUartCommHandler.hpp"
#include <memory>
#include "diagnosticsPoster.hpp"

struct Handlers {
    std::shared_ptr<WirelessHandler> wirelessHandler;
    std::shared_ptr<SDcardHandler> sdcardHandler;
    std::shared_ptr<RequestHandler> requestHandler;
    std::shared_ptr<EspPicoCommHandler> espPicoCommHandler;
    std::shared_ptr<CameraHandler> cameraHandler;
    std::shared_ptr<DiagnosticsPoster> diagnosticsPoster;
};

void get_request_timer_callback(void *pvParameters);
void get_timestamp_timer_callback(void *pvParameters);
bool enqueue_with_retry(const QueueHandle_t queue, const void *item, TickType_t ticks_to_wait, int retries);
bool read_file_with_retry(SDcardHandler *sdcardHandler, const std::string &filename, std::string &file_data, int retries);
// bool read_file_with_retry_base64(SDcardHandler *sdcardHandler, const std::string &filename, std::string &file_data, int retries);
bool read_file_base64_and_send(SDcardHandler *sdcardHandler, RequestHandler *requesthandler, const std::string &filename, const int64_t image_id, QueueMessage *response);

void init_task(void *pvParameters);
void send_request_to_websrv_task(void *pvParameters);
void uart_read_task(void *pvParameters);
void handle_uart_data_task(void *pvParameters);

#endif