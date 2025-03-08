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
    this->tlsWrapper = std::make_shared<TLSWrapper>();

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
                       "Connection: close\r\n"
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

int RequestHandler::createImagePOSTRequest(unsigned char *file_buffer, const size_t buffer_max_size,
                                           int current_data_len, const int64_t image_id) {
    const char *web_token = this->wirelessHandler->get_setting(Settings::WEB_TOKEN);
    const char *web_domain = this->wirelessHandler->get_setting(Settings::WEB_DOMAIN);
    const char *web_port = this->wirelessHandler->get_setting(Settings::WEB_PORT);
    std::string img_id = std::to_string(image_id);
    int total_chars = 168 + 3 + 2; // 168 is the total number of characters added to the request string

    total_chars += strlen(web_token) + strlen(web_domain) + strlen(web_port) + img_id.length() + current_data_len + 1;

    if (total_chars > buffer_max_size) {
        DEBUG("Error: Request buffer is too small");
        return -1;
    }

    const int content_length = 28 + img_id.length() + strlen(web_token) + current_data_len;

    char *temp_buff = static_cast<char *>(heap_caps_malloc(total_chars, MALLOC_CAP_SPIRAM));
    if (temp_buff == nullptr) {
        DEBUG("Error: Failed to allocate memory for temporary buffer");
        return -2;
    }

    int sprintflen;

    if ((sprintflen = snprintf(temp_buff, total_chars,
                               "POST /api/upload HTTP/1.0\r\n"
                               "Host: %s:%s\r\n"
                               "User-Agent: esp-idf/1.0 esp32\r\n"
                               "Connection: close\r\n"
                               "Content-Type: application/json\r\n"
                               "Content-Length: %d\r\n"
                               "\r\n"
                               "{\"token\":\"%s\",\"id\":%s,\"data\":\"%s\"}\r\n",
                               web_domain, web_port, content_length, web_token, img_id.c_str(), file_buffer)) < 0) {
        DEBUG("Error: Failed to create POST request");
        free(temp_buff);
        return -3;
    }

    memcpy(file_buffer, temp_buff, total_chars);

    std::string asd(temp_buff, 500);
    std::string end(temp_buff + total_chars - 250, 250);
    DEBUG("Request start: ", asd.c_str());
    DEBUG("Request end: ", end.c_str());
    DEBUG("Last char: ", temp_buff[total_chars - 1], ",", temp_buff[total_chars - 2], ",", temp_buff[total_chars - 3]);
    DEBUG("snprintf return: ", sprintflen);

    free(temp_buff);
    return total_chars - 1; // don't include the null terminator
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
                       "Connection: close\r\n"
                       "\r\n");
}

void RequestHandler::updateUserInstructionsGETRequest() {
    std::string getRequest;
    this->createUserInstructionsGETRequest(&getRequest);
    strncpy(this->getUserInsturctionsRequest.str_buffer, getRequest.c_str(),
            sizeof(this->getUserInsturctionsRequest.str_buffer) - 1);
    this->getUserInsturctionsRequest.str_buffer[sizeof(this->getUserInsturctionsRequest.str_buffer) - 1] = '\0';
    this->getUserInsturctionsRequest.buffer_length = getRequest.length();
    this->getUserInsturctionsRequest.requestType = RequestType::GET_COMMANDS;
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
                       "Connection: close\r\n"
                       "\r\n");
}

void RequestHandler::processArgs(std::ostringstream &content, bool &first) {
    // Base case for recursion - no action needed
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

RequestHandlerReturnCode RequestHandler::sendRequest(const char *request, const size_t request_len,
                                                     QueueMessage *response) {
    if (!this->wirelessHandler->isConnected()) {
        DEBUG("Wireless is not connected");
        return RequestHandlerReturnCode::NOT_CONNECTED;
    }

    DEBUG("Taking request mutex");
    ScopedMutex lock(this->requestMutex);

    struct addrinfo hints = {};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    addrinfo *dns_lookup_results = nullptr;
    int err = getaddrinfo(this->getWebServerCString(), this->getWebPortCString(), &hints, &dns_lookup_results);
    if (err != 0 || !dns_lookup_results) {
        DEBUG("DNS lookup failed, err=", err);
        return RequestHandlerReturnCode::DNS_LOOKUP_FAIL;
    }

    int socket_descriptor = socket(dns_lookup_results->ai_family, dns_lookup_results->ai_socktype, 0);
    if (socket_descriptor < 0) {
        DEBUG("Socket allocation failed, errno=", errno);
        freeaddrinfo(dns_lookup_results);
        return RequestHandlerReturnCode::SOCKET_ALLOCATION_FAIL;
    }

    if (connect(socket_descriptor, dns_lookup_results->ai_addr, dns_lookup_results->ai_addrlen) != 0) {
        DEBUG("Socket connect failed, errno=", errno);
        close(socket_descriptor);
        freeaddrinfo(dns_lookup_results);
        return RequestHandlerReturnCode::SOCKET_CONNECT_FAIL;
    }

    freeaddrinfo(dns_lookup_results); // Safe to free now
    DEBUG("Connected to server");

    DEBUG("strlen(request)=", strlen(request));
    DEBUG("request_len=", request_len);

    if (write(socket_descriptor, request, request_len) < 0) {
        DEBUG("Socket send failed, errno=", errno);
        close(socket_descriptor);
        return RequestHandlerReturnCode::SOCKET_SEND_FAIL;
    }

    DEBUG("Request sent");

    timeval receiving_timeout = {.tv_sec = 5, .tv_usec = 0};
    setsockopt(socket_descriptor, SOL_SOCKET, SO_RCVTIMEO, &receiving_timeout, sizeof(receiving_timeout));

    std::vector<char> receive_buffer(BUFFER_SIZE, 0);
    int total_len = 0;

    for (int attempt = 0; attempt < RETRIES; ++attempt) {
        int len = recv(socket_descriptor, receive_buffer.data() + total_len, BUFFER_SIZE - 1 - total_len, 0);

        if (len > 0) {
            total_len += len;
            if (total_len >= BUFFER_SIZE - 1) break;
        } else if (len == 0) {
            DEBUG("Connection closed by peer.");
            break;
        } else {
            if (errno == EAGAIN) {
                DEBUG("Receive timeout, retrying...");
                vTaskDelay(pdMS_TO_TICKS(RETRY_DELAY_MS));
                continue;
            }
            DEBUG("Socket receive failed, errno=", errno);
            close(socket_descriptor);
            return RequestHandlerReturnCode::SOCKET_RECEIVE_FAIL;
        }
    }

    close(socket_descriptor);

    if (total_len == 0) {
        DEBUG("No data received from server");
        return RequestHandlerReturnCode::UN_CLASSIFIED_ERROR;
    }

    receive_buffer[total_len] = '\0';
    strncpy(response->str_buffer, receive_buffer.data(), BUFFER_SIZE - 1);
    response->str_buffer[BUFFER_SIZE - 1] = '\0';

    DEBUG("Response: ", response->str_buffer);
    parseResponseIntoJson(response, total_len);

    return RequestHandlerReturnCode::SUCCESS;
}

RequestHandlerReturnCode RequestHandler::sendRequest(const unsigned char *request, const size_t request_len,
                                                     QueueMessage *response) {
    return this->sendRequest(reinterpret_cast<const char *>(request), request_len, response);
}

RequestHandlerReturnCode RequestHandler::sendRequest(std::string request, QueueMessage *response) {
    return this->sendRequest(request.c_str(), request.length(), response);
}

RequestHandlerReturnCode RequestHandler::sendRequest(const QueueMessage request, QueueMessage *response) {
    return this->sendRequest(std::string(request.str_buffer, request.buffer_length), response);
}

RequestHandlerReturnCode RequestHandler::sendRequestTLS(const char *request, const size_t request_len,
                                                        QueueMessage *response) {
    if (!this->wirelessHandler->isConnected()) {
        DEBUG("Wireless is not connected");
        return RequestHandlerReturnCode::NOT_CONNECTED;
    }

    DEBUG("Taking request mutex");
    ScopedMutex lock(this->requestMutex); // ScopedMutex will automatically unlock when it goes out of scope

    if (!this->tlsWrapper->connect(this->getWebServerCString(), this->getWebPortCString(),
                                   this->getWebServerCertCsring())) {
        DEBUG("TLS connection failed");
        return RequestHandlerReturnCode::SOCKET_CONNECT_FAIL;
    }
    DEBUG("TLS connection established");

    // Send request
    if (this->tlsWrapper->send(request, request_len) < 0) {
        DEBUG("TLS send failed");
        this->tlsWrapper->close();
        return RequestHandlerReturnCode::SOCKET_SEND_FAIL;
    }
    DEBUG("TLS send success");

    // Receive response
    char receive_buffer[BUFFER_SIZE] = {0};
    int total_len = this->tlsWrapper->receive(receive_buffer, BUFFER_SIZE - 1);

    this->tlsWrapper->close();

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

RequestHandlerReturnCode RequestHandler::sendRequestTLS(const unsigned char *request, const size_t request_len,
                                                        QueueMessage *response) {
    return this->sendRequestTLS(reinterpret_cast<const char *>(request), request_len, response);
}

RequestHandlerReturnCode RequestHandler::sendRequestTLS(const std::string &request, QueueMessage *response) {
    return this->sendRequestTLS(request.c_str(), request.length(), response);
}

RequestHandlerReturnCode RequestHandler::sendRequestTLS(const QueueMessage request, QueueMessage *response) {
    return this->sendRequestTLS(std::string(request.str_buffer, request.buffer_length), response);
}

int RequestHandler::parseResponseIntoJson(QueueMessage *responseBuffer, const int buffer_size) {
    std::string response(responseBuffer->str_buffer, buffer_size);

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
