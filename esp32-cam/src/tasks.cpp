#include "tasks.hpp"
#include "debug.hpp"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/dns.h"
#include "lwip/err.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "nvs_flash.h"
#include "requestHandler.hpp"
#include "sd-card.hpp"
#include "sdkconfig.h"
#include "socket.h"
#include "timesync-lib.hpp"
#include "wireless.hpp"
#include <memory>
#include <string.h>

#include "testMacros.hpp"


// ----------------------------------------------------------
// ----------------SUPPORTING-FUNCTIONS----------------------
// ----------------------------------------------------------


TaskReturnCode send_request_and_enqueue_response(RequestHandler *requestHandler, QueueMessage *request) {
    addrinfo *dns_lookup_results = nullptr;
    in_addr *ip_address = nullptr;
    int socket_descriptor = 0;
    int read_result = 0;
    char receive_buffer[BUFFER_SIZE];
    const addrinfo hints = {
        .ai_flags = 0, // default
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM,
        .ai_protocol = 0,        // default
        .ai_addrlen = 0,         // default
        .ai_addr = nullptr,      // default
        .ai_canonname = nullptr, // default
        .ai_next = nullptr       // default
    };
    timeval receiving_timeout;
    receiving_timeout.tv_sec = 5;
    receiving_timeout.tv_usec = 0;
    int retry_count = 0;

    // DNS lookup
    int err = getaddrinfo(requestHandler->getWebServerCString(), requestHandler->getWebPortCString(), &hints,
                          &dns_lookup_results);
    if (err != 0 || dns_lookup_results == nullptr) {
        DEBUG("DNS lookup failed err=", err, " dns_lookup_results=", dns_lookup_results);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        return TaskReturnCode::DNS_LOOKUP_FAIL;
    }

    // Print resolved IP.
    ip_address = &((struct sockaddr_in *)dns_lookup_results->ai_addr)->sin_addr;
    DEBUG("DNS lookup succeeded. IP=", inet_ntoa(*ip_address));

    // Create socket
    socket_descriptor = socket(dns_lookup_results->ai_family, dns_lookup_results->ai_socktype, 0);
    if (socket_descriptor < 0) {
        DEBUG("... Failed to allocate socket.");
        freeaddrinfo(dns_lookup_results);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        return TaskReturnCode::SOCKET_ALLOCATION_FAIL;
    }
    DEBUG("... allocated socket");

    // Connect to server
    if (connect(socket_descriptor, dns_lookup_results->ai_addr, dns_lookup_results->ai_addrlen) != 0) {
        DEBUG("... socket connect failed errno=", errno);
        close(socket_descriptor);
        freeaddrinfo(dns_lookup_results);
        vTaskDelay(5000 / portTICK_PERIOD_MS);
        return TaskReturnCode::SOCKET_CONNECT_FAIL;
    }
    DEBUG("... connected");

    // Send request
    freeaddrinfo(dns_lookup_results);
    if (write(socket_descriptor, request->request, strlen(request->request)) < 0) {
        DEBUG("... socket send failed\n");
        close(socket_descriptor);
        vTaskDelay(5000 / portTICK_PERIOD_MS);
        return TaskReturnCode::SOCKET_SEND_FAIL;
    }
    DEBUG("... socket send success");

    // Set receiving timeout
    if (setsockopt(socket_descriptor, SOL_SOCKET, SO_RCVTIMEO, &receiving_timeout, sizeof(receiving_timeout)) < 0) {
        DEBUG("... failed to set socket receiving timeout");
        close(socket_descriptor);
        vTaskDelay(4000 / portTICK_PERIOD_MS);
        return TaskReturnCode::SOCKET_TIMEOUT_FAIL;
    }
    DEBUG("... set socket receiving timeout success");

    /* Read HTTP response */
    do {
        bzero(receive_buffer, sizeof(receive_buffer));
        read_result = read(socket_descriptor, receive_buffer, sizeof(receive_buffer) - 1);
        for (int i = 0; i < read_result; i++) {
            putchar(receive_buffer[i]);
        }
    } while (read_result > 0);

    DEBUG("\n... done reading from socket. Last read return=", read_result, " errno=", errno);
    close(socket_descriptor);

    // Send response to the response queue
    strncpy(request->request, receive_buffer, BUFFER_SIZE);
    request->request[BUFFER_SIZE - 1] = '\0';
    request->requestType = RequestType::WEB_SERVER_RESPONSE;
    while (xQueueSend(requestHandler->getWebSrvResponseQueue(), &request, 0) != pdTRUE &&
           retry_count < ENQUEUE_REQUEST_RETRIES) {
        retry_count++;
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    return TaskReturnCode::SUCCESS;
}

// ----------------------------------------------------------
// ------------------------TASKS-----------------------------
// ----------------------------------------------------------

void send_request_task(void *pvParameters) {
    RequestHandler *requestHandler = (RequestHandler *)pvParameters;
    QueueMessage *request = nullptr;

    while (true) {
        // Wait for a request to be available
        if (xQueueReceive(requestHandler->getWebSrvRequestQueue(), &request, portMAX_DELAY) == pdTRUE) {
            switch (request->requestType) {
            case RequestType::GET_COMMANDS:
                DEBUG("GET_COMMANDS request received");
                // Create a GET request
                // Send the request and enqueue the response
                break;
            case RequestType::POST_IMAGE:
                DEBUG("POST_IMAGE request received");
                // Create a POST request
                // Add image link to the request
                // Send the request and enqueue the response
                break;
            case RequestType::POST_DIAGNOSTICS:
                DEBUG("POST_DIAGNOSTICS request received");
                // Create a POST request
                // Add diagnostics data to the request
                // Send the request and enqueue the response
                break;
            default:
                DEBUG("Unknown request type received");
                break;
            }
        }
    }
}