#ifndef ESP_PICO_COMM_HANDLER_HPP
#define ESP_PICO_COMM_HANDLER_HPP

#include "driver/uart.h"
#include <stdint.h>

#define UART_RING_BUFFER_SIZE 1024
#define EVENT_QUEUE_SIZE 10

class EspPicoCommHandler {
  public:
    EspPicoCommHandler(uart_port_t uart_num = UART_NUM_2, uart_config_t uart_config = {
                                                              .baud_rate = 115200,
                                                              .data_bits = UART_DATA_8_BITS,
                                                              .parity = UART_PARITY_DISABLE,
                                                              .stop_bits = UART_STOP_BITS_1,
                                                              .flow_ctrl = UART_HW_FLOWCTRL_CTS_RTS,
                                                              .rx_flow_ctrl_thresh = 122,
                                                          });

  private:
    uart_port_t uart_num;
    uart_config_t uart_config;
    QueueHandle_t uart_event_queue;

};

#endif