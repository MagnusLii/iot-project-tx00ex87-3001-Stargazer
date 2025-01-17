#ifndef TASKS_HPP
#define TASKS_HPP

#include "requestHandler.hpp"

void get_request_timer_callback(TimerHandle_t timer);

void init_task(void *pvParameters);
void send_request_task(void *pvParameters);
void uart_read_task(void *pvParameters);
void handle_uart_data_task(void *pvParameters);

void take_picture_and_save_to_sdcard_in_loop_task(void *pvParameters);

#endif