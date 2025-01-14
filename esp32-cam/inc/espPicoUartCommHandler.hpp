#ifndef ESP_PICO_COMM_HANDLER_HPP
#define ESP_PICO_COMM_HANDLER_HPP

#include "driver/uart.h"
#include <stdint.h>
#include <string>

#define UART_RING_BUFFER_SIZE  1024
#define EVENT_QUEUE_SIZE       5
#define LONGEST_COMMAND_LENGTH 100

struct UartReceivedData {
    char buffer[LONGEST_COMMAND_LENGTH];
    size_t len;
};

class EspPicoCommHandler {
  public:
    // UART_NUM_0 is the only viable option for AI-Thinker ESPCAM, UART_NUM_1 is not available and UART_NUM_2 only has a
    // read pin. https://randomnerdtutorials.com/esp32-cam-ai-thinker-pinout/
    EspPicoCommHandler(uart_port_t uart_num = UART_NUM_0, uart_config_t uart_config = {
                                                              .baud_rate = 9600,
                                                              .data_bits = UART_DATA_8_BITS,
                                                              .parity = UART_PARITY_DISABLE,
                                                              .stop_bits = UART_STOP_BITS_1,
                                                              .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
                                                              .rx_flow_ctrl_thresh = 122,
                                                              .source_clk = UART_SCLK_DEFAULT,
                                                              .flags = 0
                                                          });

    void send_data(const char *data, size_t len);
    int receive_data(char *buffer, size_t max_len);

    uart_port_t get_uart_num();
    uart_config_t get_uart_config();
    QueueHandle_t get_uart_event_queue_handle();
    QueueHandle_t get_uart_received_data_queue_handle();
    TimerHandle_t get_request_timer_handle();

    bool set_response_wait_timer(std::string command);
    void disable_response_wait_timer();

  private:
    uart_port_t uart_num;
    uart_config_t uart_config;
    QueueHandle_t uart_event_queue;
    QueueHandle_t uart_received_data_queue;

    std::string previousCommand;
    bool waitingForResponse = false;
    TimerHandle_t responseWaitTimer;
};

#endif