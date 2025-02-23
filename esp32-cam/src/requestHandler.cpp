#include "requestHandler.hpp"
#include "TLSWrapper.hpp"
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
#include "scopedMutex.hpp"
#include "sd-card.hpp"
#include "sdkconfig.h"
#include "socket.h"
#include "testMacros.hpp"
#include "timesync-lib.hpp"
#include "wireless.hpp"
#include <memory>
#include <sstream>
#include <string.h>

RequestHandler::RequestHandler(std::shared_ptr<WirelessHandler> wirelessHandler,
                               std::shared_ptr<SDcardHandler> sdcardHandler) {
    this->wirelessHandler = wirelessHandler;
    this->sdcardHandler = sdcardHandler;
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

    this->createTimestampGETRequest(&getRequest);
    strncpy(this->getTimestampRequest.str_buffer, getRequest.c_str(), sizeof(this->getTimestampRequest.str_buffer) - 1);
    this->getTimestampRequest.str_buffer[sizeof(this->getTimestampRequest.str_buffer) - 1] = '\0';
    this->getTimestampRequest.buffer_length = getRequest.length();
    this->getTimestampRequest.requestType = RequestType::GET_TIME;
}

RequestHandler::~RequestHandler() {
    vQueueDelete(this->webSrvRequestQueue);
    vQueueDelete(this->webSrvResponseQueue);
    vSemaphoreDelete(this->requestMutex);
}

RequestHandlerReturnCode RequestHandler::createImagePOSTRequest(std::string *requestPtr, const int image_id,
                                                                std::string base64_image_data) {
    if (requestPtr == nullptr) {
        DEBUG("Error: requestPtr is null");
        return RequestHandlerReturnCode::INVALID_ARGUMENT;
    }

    if (base64_image_data.empty()) {
        DEBUG("Error: base64_image_data is empty");
        return RequestHandlerReturnCode::INVALID_ARGUMENT;
    }

    std::string content = "{"
                          "\"token\":\"";
    content.append(this->wirelessHandler->get_setting(Settings::WEB_TOKEN));

    content.append("\",\"id\":");
    content.append(std::to_string(image_id));
    content.append(",\"data\":\"");
    content.append(base64_image_data);
    content.append("\"}\r\n");

    *requestPtr = "POST "
                  "/api/upload"
                  " HTTP/1.0\r\n"
                  "Host: ";

    requestPtr->append(this->wirelessHandler->get_setting(Settings::WEB_DOMAIN));
    requestPtr->append(":");
    requestPtr->append(this->wirelessHandler->get_setting(Settings::WEB_PORT));
    requestPtr->append("\r\n"
                       "User-Agent: esp-idf/1.0 esp32\r\n"
                       "Connection: keep-alive\r\n"
                       "Content-Type: application/json\r\n"
                       "Content-Length: ");
    requestPtr->append(std::to_string(content.length()));
    requestPtr->append("\r\n"
                       "\r\n");
    requestPtr->append(content);

    DEBUG("Request: ", requestPtr->c_str());

    if (requestPtr->empty()) {
        DEBUG("Error: Request string is empty after construction");
        return RequestHandlerReturnCode::UN_CLASSIFIED_ERROR;
    }

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
void RequestHandler::createUserInstructionsGETRequest(std::string *requestPtr) {
    *requestPtr = "GET "
                  "/api/command?token=";

    requestPtr->append(this->wirelessHandler->get_setting(Settings::WEB_TOKEN));
    requestPtr->append(" HTTP/1.0\r\n"
                       "Host: ");
    requestPtr->append(this->wirelessHandler->get_setting(Settings::WEB_DOMAIN));
    requestPtr->append(":");
    requestPtr->append(this->wirelessHandler->get_setting(Settings::WEB_PORT));
    requestPtr->append("\r\n"
                       "User-Agent: esp-idf/1.0 esp32\r\n"
                       "Connection: keep-alive\r\n"
                       "\r\n");
}

void RequestHandler::createTimestampGETRequest(std::string *requestPtr) {
    *requestPtr = "GET "
                  "/api/time"
                  " HTTP/1.0\r\n"
                  "Host: ";
    requestPtr->append(this->wirelessHandler->get_setting(Settings::WEB_DOMAIN));
    requestPtr->append(":");
    requestPtr->append(this->wirelessHandler->get_setting(Settings::WEB_PORT));
    requestPtr->append("\r\n"
                       "User-Agent: esp-idf/1.0 esp32\r\n"
                       "Connection: keep-alive\r\n"
                       "\r\n");
}

// The function reads all variable arguments as const char*, no \" is added to the values so should be added by the
// caller if meant to be included as strings in the JSON.
RequestHandlerReturnCode RequestHandler::createGenericPOSTRequest(std::string *requestPtr, const char *endpoint,
                                                                  int numOfVariableArgs, ...) {
    if (requestPtr == nullptr) {
        DEBUG("Error: requestPtr is null");
        return RequestHandlerReturnCode::INVALID_ARGUMENT;
    }

    if (numOfVariableArgs % 2 != 0) {
        DEBUG("Error: Number of arguments is not divisible by 2");
        return RequestHandlerReturnCode::INVALID_NUM_OF_ARGS;
    }

    va_list args;
    va_start(args, numOfVariableArgs);

    std::string content = "{";
    for (int i = 0; i < numOfVariableArgs; i += 2) {
        const char *key = va_arg(args, const char *);
        const char *value = va_arg(args, const char *);
        content += std::string(key) + ":" + std::string(value);
        if (i < numOfVariableArgs - 2) { content += ","; }
    }
    content += "}";

    va_end(args);

    *requestPtr = "POST " + std::string(endpoint) +
                  " HTTP/1.0\r\n"
                  "Host: ";
    requestPtr->append(this->wirelessHandler->get_setting(Settings::WEB_DOMAIN));
    requestPtr->append(":");
    requestPtr->append(this->wirelessHandler->get_setting(Settings::WEB_PORT));
    requestPtr->append("\r\n"
                       "User-Agent: esp-idf/1.0 esp32\r\n"
                       "Connection: keep-alive\r\n"
                       "Content-Type: application/json\r\n"
                       "Content-Length: ");
    requestPtr->append(std::to_string(content.length()));
    requestPtr->append("\r\n"
                       "\r\n");
    requestPtr->append(content);

    DEBUG("Request: ", requestPtr->c_str());

    if (requestPtr->empty()) {
        DEBUG("Error: Request string is empty after construction");
        return RequestHandlerReturnCode::FAILED_TO_CREATE_REQUEST;
    }

    return RequestHandlerReturnCode::SUCCESS;
}

/**
 * Parses the HTTP response to extract the return code.
 *
 * @param response - A C-style string containing the HTTP response.
 *
 * @return int - The HTTP return code. Returns -1 if the return code could not be parsed.
 */
int RequestHandler::parseHttpReturnCode(const char *responseString) {
    if (responseString == nullptr) {
        DEBUG("Error: response is null");
        return -1;
    }

    const char *status_line_end = strstr(responseString, "\r\n");
    if (status_line_end == nullptr) {
        DEBUG("Error: Could not find end of status line");
        return -1;
    }

    std::string status_line(responseString, status_line_end - responseString);
    size_t code_start = status_line.find(' ') + 1;
    size_t code_end = status_line.find(' ', code_start);

    if (code_start == std::string::npos || code_end == std::string::npos) {
        DEBUG("Error: Could not parse return code from status line");
        return -1;
    }

    std::string code_str = status_line.substr(code_start, code_end - code_start);
    int return_code = std::stoi(code_str);

    DEBUG("Parsed HTTP return code: ", return_code);
    return return_code;
}

/**
 * Retrieves a pointer to the `QueueMessage` object representing the user instructions GET request.
 *
 * @return QueueMessage* - A pointer to the reusable `QueueMessage` object saved in the handler.
 */
QueueMessage *RequestHandler::getUserInstructionsGETRequestptr() { return &this->getUserInsturctionsRequest; }

QueueMessage *RequestHandler::getTimestampGETRequestptr() { return &this->getTimestampRequest; }

/**
 * Retrieves the web server's address as a C-style string.
 *
 * @return const char* - A pointer to a null-terminated string representing the web server's address.
 *                       This is used for DNS resolution and establishing a connection to the server.
 */
const char *RequestHandler::getWebServerCString() { return this->wirelessHandler->get_setting(Settings::WEB_DOMAIN); }

/**
 * Retrieves the web server's port as a C-style string.
 *
 * @return const char* - A pointer to a null-terminated string representing the web server's port.
 *                       This is used for establishing a connection to the server.
 */
const char *RequestHandler::getWebPortCString() { return this->wirelessHandler->get_setting(Settings::WEB_PORT); }

const char *RequestHandler::getWebServerTokenCString() {
    return this->wirelessHandler->get_setting(Settings::WEB_TOKEN);
}

const char *RequestHandler::getWebServerCertCsring() {
    return this->wirelessHandler->get_setting(Settings::WEB_CERTIFICATE);
}

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

RequestHandlerReturnCode RequestHandler::sendRequest(std::string request, QueueMessage *response) {
    if (!this->wirelessHandler->isConnected()) {
        DEBUG("Wireless is not connected");
        return RequestHandlerReturnCode::NOT_CONNECTED;
    }

    DEBUG("Taking request mutex");
    ScopedMutex lock(this->requestMutex);

    addrinfo *dns_lookup_results = nullptr;
    in_addr *ip_address = nullptr;
    int socket_descriptor = 0;
    char receive_buffer[BUFFER_SIZE] = {0};

    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints)); // Clear the structure
    hints.ai_family = AF_INET;        // Use IPv4
    hints.ai_socktype = SOCK_STREAM;  // TCP socket

    timeval receiving_timeout{.tv_sec = 5, .tv_usec = 0};

    int err = getaddrinfo(this->getWebServerCString(), this->getWebPortCString(), &hints, &dns_lookup_results);
    if (err != 0 || dns_lookup_results == nullptr) {
        DEBUG("DNS lookup failed err=", err);
        vTaskDelay(pdMS_TO_TICKS(1000));
        return RequestHandlerReturnCode::DNS_LOOKUP_FAIL;
    }

    ip_address = &((struct sockaddr_in *)dns_lookup_results->ai_addr)->sin_addr;
    DEBUG("DNS lookup succeeded. IP=", inet_ntoa(*ip_address));

    socket_descriptor = socket(dns_lookup_results->ai_family, dns_lookup_results->ai_socktype, 0);
    if (socket_descriptor < 0) {
        DEBUG("Failed to allocate socket.");
        freeaddrinfo(dns_lookup_results);
        vTaskDelay(pdMS_TO_TICKS(1000));
        return RequestHandlerReturnCode::SOCKET_ALLOCATION_FAIL;
    }
    DEBUG("Allocated socket");

    if (connect(socket_descriptor, dns_lookup_results->ai_addr, dns_lookup_results->ai_addrlen) != 0) {
        DEBUG("Socket connect failed errno=", errno);
        close(socket_descriptor);
        freeaddrinfo(dns_lookup_results);
        vTaskDelay(pdMS_TO_TICKS(5000));
        return RequestHandlerReturnCode::SOCKET_CONNECT_FAIL;
    }
    DEBUG("Connected");

    freeaddrinfo(dns_lookup_results);
    if (write(socket_descriptor, request.c_str(), request.length() + 1) < 0) {
        DEBUG("Socket send failed");
        close(socket_descriptor);
        vTaskDelay(pdMS_TO_TICKS(5000));
        return RequestHandlerReturnCode::SOCKET_SEND_FAIL;
    }
    DEBUG("Socket send success");

    if (setsockopt(socket_descriptor, SOL_SOCKET, SO_RCVTIMEO, &receiving_timeout, sizeof(receiving_timeout)) < 0) {
        DEBUG("Failed to set socket receiving timeout");
        close(socket_descriptor);
        vTaskDelay(pdMS_TO_TICKS(4000));
        return RequestHandlerReturnCode::SOCKET_TIMEOUT_FAIL;
    }
    DEBUG("Set socket receiving timeout success");

    int total_len = 0;
    for (int attempt = 0; attempt < RETRIES; attempt++) {
        int len = recv(socket_descriptor, receive_buffer + total_len, BUFFER_SIZE - 1 - total_len, 0);

        if (len > 0) {
            total_len += len;
            if (total_len >= BUFFER_SIZE - 1) break; // Prevent buffer overflow
        } else if (len == 0) {
            DEBUG("Connection closed by peer.");
            break;
        } else {
            if (errno == EAGAIN) {
                vTaskDelay(pdMS_TO_TICKS(RETRY_DELAY_MS));
                continue; // Retry the read
            } else {
                DEBUG("Socket error: errno=", errno);
                break;
            }
        }
    }

    receive_buffer[total_len] = '\0';
    close(socket_descriptor);

    if (total_len == 0) {
        DEBUG("Failed to receive any data from the server");
        return RequestHandlerReturnCode::UN_CLASSIFIED_ERROR;
    }

    strncpy(response->str_buffer, receive_buffer, BUFFER_SIZE);
    response->str_buffer[BUFFER_SIZE - 1] = '\0';

    DEBUG("Response: ", response->str_buffer);
    parseResponseIntoJson(response, total_len);

    return RequestHandlerReturnCode::SUCCESS;
}

RequestHandlerReturnCode RequestHandler::sendRequest(const QueueMessage request, QueueMessage *response) {
    return this->sendRequest(std::string(request.str_buffer, request.buffer_length), response);
}

RequestHandlerReturnCode RequestHandler::sendRequestTLS(const std::string &request, QueueMessage *response) {
    if (!this->wirelessHandler->isConnected()) {
        DEBUG("Wireless is not connected");
        return RequestHandlerReturnCode::NOT_CONNECTED;
    }

    DEBUG("Taking request mutex");
    ScopedMutex lock(this->requestMutex); // ScopedMutex will automatically unlock when it goes out of scope

    TLSWrapper tls;
    if (!tls.connect(this->getWebServerCString(), this->getWebPortCString(), this->getWebServerCertCsring())) {
        DEBUG("TLS connection failed");
        return RequestHandlerReturnCode::SOCKET_CONNECT_FAIL;
    }
    DEBUG("TLS connection established");

    // Send request
    if (tls.send(request.c_str(), request.length()) < 0) {
        DEBUG("TLS send failed");
        tls.close();
        return RequestHandlerReturnCode::SOCKET_SEND_FAIL;
    }
    DEBUG("TLS send success");

    // Receive response
    char receive_buffer[BUFFER_SIZE] = {0};
    int total_len = tls.receive(receive_buffer, BUFFER_SIZE - 1);

    tls.close();

    if (total_len <= 0) {
        DEBUG("TLS receive failed");
        return RequestHandlerReturnCode::UN_CLASSIFIED_ERROR;
    }

    receive_buffer[total_len] = '\0';
    strncpy(response->str_buffer, receive_buffer, BUFFER_SIZE);
    response->str_buffer[BUFFER_SIZE - 1] = '\0';

    DEBUG("Response: ", response->str_buffer);

    parseResponseIntoJson(response, total_len);

    DEBUG("Returning success");
    return RequestHandlerReturnCode::SUCCESS;
}

RequestHandlerReturnCode RequestHandler::sendRequestTLS(const QueueMessage request, QueueMessage *response) {
    return this->sendRequestTLS(std::string(request.str_buffer, request.buffer_length), response);
}

int RequestHandler::parseResponseIntoJson(QueueMessage *responseBuffer, const int buffer_size) {
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

int64_t RequestHandler::parseTimestamp(const std::string &response) {
    DEBUG("Parsing timestamp from response: ", response.c_str());

    // Find content field
    std::string http_response = response;
    size_t start_pos = http_response.find("\r\n\r\n");
    if (start_pos == std::string::npos) {
        DEBUG("Content field not found");
        return -1; // "Content" field not found
    }

    // test end of str
    if (start_pos + 4 >= http_response.length()) {
        DEBUG("Timestamp field not found");
        return -2;
    }

    // test if number.
    for (size_t i = start_pos + 4; i < http_response.length(); i++) {
        if (http_response[i] < '0' || http_response[i] > '9') {
            DEBUG("Timestamp is not a number");
            return -3;
        }
    }

    std::string value = http_response.substr(start_pos + 4);

    DEBUG("Timestamp: ", value.c_str());

    return std::stoll(value);
}

bool RequestHandler::getTimeSyncedStatus() { return this->timeSynchronized; }

void RequestHandler::setTimeSyncedStatus(bool status) { this->timeSynchronized = status; }
