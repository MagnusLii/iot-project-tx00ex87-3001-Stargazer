#ifndef TASKS_HPP
#define TASKS_HPP

#include "requestHandler.hpp"
#include "espPicoUartCommHandler.hpp"

void get_request_timer_task(void *pvParameters);
void get_request_timer_task(void *pvParameters);
bool enqueue_with_retry(const QueueHandle_t queue, const void *item, TickType_t ticks_to_wait, int retries);
bool read_file_with_retry(SDcardHandler *sdcardHandler, const std::string &filename, std::string &file_data, int retries);
bool read_file_with_retry_base64(SDcardHandler *sdcardHandler, const std::string &filename, std::string &file_data, int retries);

void init_task(void *pvParameters);
void send_request_to_websrv_task(void *pvParameters);
void uart_read_task(void *pvParameters);
void handle_uart_data_task(void *pvParameters);

void take_picture_and_save_to_sdcard_in_loop_task(void *pvParameters);

#endif