#include "requestHandler.hpp"
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
#include "testMacros.hpp"
#include "timesync-lib.hpp"
#include "wireless.hpp"
#include <memory>
#include <string.h>

RequestHandler::RequestHandler(std::string webServer, std::string webPort, std::string webServerToken,
                               std::shared_ptr<WirelessHandler> wirelessHandler, std::shared_ptr<SDcard> sdcard) {
    this->webServer = webServer;
    this->webPort = webPort;
    this->webServerToken = webServerToken;
    this->wirelessHandler = wirelessHandler;
    this->sdcard = sdcard;
    this->webSrvRequestQueue = xQueueCreate(QUEUE_SIZE, sizeof(QueueMessage));
    this->webSrvResponseQueue = xQueueCreate(QUEUE_SIZE, sizeof(QueueMessage));
    this->requestMutex = xSemaphoreCreateMutex();

    std::string getRequest;
    this->createUserInstructionsGETRequest(&getRequest);
    strncpy(this->getUserInsturctionsRequest.str_buffer, getRequest.c_str(),
            sizeof(this->getUserInsturctionsRequest.str_buffer) - 1);
    this->getUserInsturctionsRequest.str_buffer[sizeof(this->getUserInsturctionsRequest.str_buffer) - 1] = '\0';
    this->getUserInsturctionsRequest.buffer_length = getRequest.length();
    this->getUserInsturctionsRequest.requestType = RequestType::GET_COMMANDS;

    DEBUG("getUserInsturctionsRequest.str_buffer: ", this->getUserInsturctionsRequest.str_buffer);
    DEBUG("getUserInsturctionsRequest.requestType: ", (int)this->getUserInsturctionsRequest.requestType);
}

RequestHandlerReturnCode RequestHandler::createDiagnosticsPOSTRequest(std::string *requestPtr) {
    // Implement when requests are more clear.
    return RequestHandlerReturnCode::SUCCESS;
}

RequestHandlerReturnCode RequestHandler::createImagePOSTRequest(std::string *requestPtr) {
    // Implement when requests are more clear.
    return RequestHandlerReturnCode::SUCCESS;
}

/**
 * Creates a GET request for user instructions and stores it in the provided pointer.
 *
 * @param requestPtr - A pointer to a `std::string` where the generated GET request will be stored.
 *                     The request includes the web server's address, port, and token for authentication.
 *
 * @return RequestHandlerReturnCode - Indicates the result of the operation. Always returns SUCCESS
 *                                    since the request is successfully generated.
 */
RequestHandlerReturnCode RequestHandler::createUserInstructionsGETRequest(std::string *requestPtr) {
    *requestPtr = "GET "
                  "/api/command?token=" +
                  this->webServerToken +
                  " HTTP/1.0\r\n"
                  "Host: " +
                  this->webServer + ":" + this->webPort +
                  "\r\n"
                  "User-Agent: esp-idf/1.0 esp32\r\n"
                  "Connection: keep-alive\r\n"
                  "\r\n";
    return RequestHandlerReturnCode::SUCCESS;
}

/**
 * Retrieves a pointer to the `QueueMessage` object representing the user instructions GET request.
 *
 * @return QueueMessage* - A pointer to the reusable `QueueMessage` object saved in the handler.
 */
QueueMessage *RequestHandler::getUserInstructionsGETRequestptr() { return &this->getUserInsturctionsRequest; }

/**
 * Retrieves the web server's address as a C-style string.
 *
 * @return const char* - A pointer to a null-terminated string representing the web server's address.
 *                       This is used for DNS resolution and establishing a connection to the server.
 */
const char *RequestHandler::getWebServerCString() { return this->webServer.c_str(); }

/**
 * Retrieves the web server's port as a C-style string.
 *
 * @return const char* - A pointer to a null-terminated string representing the web server's port.
 *                       This is used for establishing a connection to the server.
 */
const char *RequestHandler::getWebPortCString() { return this->webPort.c_str(); }

/**
 * Retrieves the queue handle used for web server request messages.
 *
 * @return QueueHandle_t - The handle to the queue where web server request messages are received.
 *                         This queue is used to pass HTTP request data to be processed by the handler.
 */
QueueHandle_t RequestHandler::getWebSrvRequestQueue() { return this->webSrvRequestQueue; }

/**
 * Retrieves the queue handle used for web server response messages.
 *
 * @return QueueHandle_t - The handle to the queue where web server response messages are sent.
 *                         This queue is used to pass the HTTP response data back to other tasks.
 */
QueueHandle_t RequestHandler::getWebSrvResponseQueue() { return this->webSrvResponseQueue; }

RequestHandlerReturnCode RequestHandler::sendRequest(const QueueMessage request, QueueMessage* response) {
    // Lock mutex
    if (xSemaphoreTake(this->requestMutex, portMAX_DELAY) != pdTRUE) {
        DEBUG("Failed to take request mutex");
        return RequestHandlerReturnCode::FAILED_MUTEX_AQUISITION;
    }

    addrinfo *dns_lookup_results = nullptr;
    in_addr *ip_address = nullptr;
    int socket_descriptor = 0;
    int read_result = 0;
    char receive_buffer[BUFFER_SIZE];
    const addrinfo hints = {
        .ai_flags = 0,              // Default settings for the DNS lookup
        .ai_family = AF_INET,       // Use IPv4
        .ai_socktype = SOCK_STREAM, // Use TCP
        .ai_protocol = 0,           // Any protocol
        .ai_addrlen = 0,            // Default
        .ai_addr = nullptr,         // Default
        .ai_canonname = nullptr,    // Default
        .ai_next = nullptr          // Default
    };
    timeval receiving_timeout;
    receiving_timeout.tv_sec = 5;  // Timeout for receiving data in seconds
    receiving_timeout.tv_usec = 0; // Timeout in microseconds

    // Perform DNS lookup for the server
    int err = getaddrinfo(this->getWebServerCString(), this->getWebPortCString(), &hints, &dns_lookup_results);
    if (err != 0 || dns_lookup_results == nullptr) {
        DEBUG("DNS lookup failed err=", err, " dns_lookup_results=", dns_lookup_results);
        vTaskDelay(1000 / portTICK_PERIOD_MS); // Retry delay
        return RequestHandlerReturnCode::DNS_LOOKUP_FAIL;
    }

    // Extract and print resolved IP address
    ip_address = &((struct sockaddr_in *)dns_lookup_results->ai_addr)->sin_addr;
    DEBUG("DNS lookup succeeded. IP=", inet_ntoa(*ip_address));

    // Create a TCP socket
    socket_descriptor = socket(dns_lookup_results->ai_family, dns_lookup_results->ai_socktype, 0);
    if (socket_descriptor < 0) {
        DEBUG("... Failed to allocate socket.");
        freeaddrinfo(dns_lookup_results); // Cleanup DNS results
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        return RequestHandlerReturnCode::SOCKET_ALLOCATION_FAIL;
    }
    DEBUG("... allocated socket");

    // Establish a connection with the server
    if (connect(socket_descriptor, dns_lookup_results->ai_addr, dns_lookup_results->ai_addrlen) != 0) {
        DEBUG("... socket connect failed errno=", errno);
        close(socket_descriptor);
        freeaddrinfo(dns_lookup_results);
        vTaskDelay(5000 / portTICK_PERIOD_MS); // Retry delay
        return RequestHandlerReturnCode::SOCKET_CONNECT_FAIL;
    }
    DEBUG("... connected");

    // Send the request data to the server
    freeaddrinfo(dns_lookup_results); // DNS results are no longer needed
    if (write(socket_descriptor, request.str_buffer, strlen(request.str_buffer)) < 0) {
        DEBUG("... socket send failed\n");
        close(socket_descriptor);
        vTaskDelay(5000 / portTICK_PERIOD_MS);
        return RequestHandlerReturnCode::SOCKET_SEND_FAIL;
    }
    DEBUG("... socket send success");

    // Set a timeout for receiving data from the server
    if (setsockopt(socket_descriptor, SOL_SOCKET, SO_RCVTIMEO, &receiving_timeout, sizeof(receiving_timeout)) < 0) {
        DEBUG("... failed to set socket receiving timeout");
        close(socket_descriptor);
        vTaskDelay(4000 / portTICK_PERIOD_MS);
        return RequestHandlerReturnCode::SOCKET_TIMEOUT_FAIL;
    }
    DEBUG("... set socket receiving timeout success");

    // read response
    int len = recv(socket_descriptor, receive_buffer, BUFFER_SIZE - 1, 0);
    receive_buffer[len] = '\0'; // Null-terminate the response

    // // Read the server's response in a loop
    // int iterator = 0;
    // do {
    //     read_result = read(socket_descriptor, receive_buffer, sizeof(receive_buffer) - 1);
    //     for (iterator = 0; iterator < read_result; iterator++) {
    //         putchar(receive_buffer[iterator]);
    //     }
    // } while (read_result > 0);
    // receive_buffer[iterator] = '\0'; // Null-terminate the response


    DEBUG("\n... done reading from socket. Last read return=", read_result, " errno=", errno);
    DEBUG(receive_buffer, "\n");
    close(socket_descriptor);

    // Send the response to a message queue
    strncpy(response->str_buffer, receive_buffer, BUFFER_SIZE); // Copy response to queue message
    response->str_buffer[BUFFER_SIZE - 1] = '\0';               // Ensure null termination

    // Set the request type for the response
    switch (request.requestType) {
        case RequestType::GET_COMMANDS:
            response->requestType = RequestType::API_COMMAND;
            break;

        case RequestType::POST_IMAGE:
            response->requestType = RequestType::API_UPLOAD;
            break;

        case RequestType::POST_DIAGNOSTICS:
            response->requestType = RequestType::API_DIAGNOSTICS;
            break;

        default:
            DEBUG("Unknown request type received");
            break;
    }

    DEBUG("\n\n\n\n\nResponse: ", response->str_buffer);

    parseResponseIntoJson(response, len); // Parse the response into json

    // Unlock mutex
    xSemaphoreGive(this->requestMutex);

    return RequestHandlerReturnCode::SUCCESS;
}

int RequestHandler::parseResponseIntoJson(QueueMessage* responseBuffer, const int buffer_size) {
    std::string response(responseBuffer->str_buffer, buffer_size);

    DEBUG("Response: ", response.c_str());

    size_t pos = response.find_first_of("{");
    if (pos == std::string::npos) {
        DEBUG("'{' not found in response.");
        return 1;
    }

    size_t end = response.find_last_of("}");
    if (end == std::string::npos) {
        DEBUG("'}' not found in response.");
        return 1;
    }

    std::string json = response.substr(pos, end - pos + 1);

    strncpy(responseBuffer->str_buffer, json.c_str(), json.length());
    responseBuffer->str_buffer[json.length()] = '\0';
    DEBUG("JSON: ", responseBuffer->str_buffer);
    responseBuffer->buffer_length = json.length();

    return 0;
}
