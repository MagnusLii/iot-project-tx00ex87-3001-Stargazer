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

/**
 * @brief Constructor for the RequestHandler class.
 *
 * Initializes the RequestHandler with shared pointers to `WirelessHandler` and `SDcardHandler`.
 * It also creates necessary queues, mutexes, and initializes the TLSWrapper. Furthermore,
 * it constructs and stores GET requests for user instructions and timestamps to be used later.
 *
 * @param wirelessHandler A shared pointer to the `WirelessHandler` object used for wireless communication.
 * @param sdcardHandler A shared pointer to the `SDcardHandler` object used for SD card operations.
 *
 * @return void
 *
 * @note This constructor also initializes two queues for handling web service requests and responses,
 *       as well as a mutex for request synchronization. Additionally, it constructs the requests for user
 *       instructions and timestamp retrieval, storing them in the class's request buffers.
 */
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

/**
 * @brief Destructor for the RequestHandler class.
 *
 * Cleans up the resources allocated by the RequestHandler. This includes deleting the created queues and
 * mutex, and performing any necessary cleanup for the TLSWrapper.
 *
 * @return void
 *
 * @note This destructor ensures that any dynamically allocated resources, such as the web service request
 *       and response queues and mutex, are properly freed.
 */
RequestHandler::~RequestHandler() {
    if (this->webSrvRequestQueue) {
        vQueueDelete(this->webSrvRequestQueue); // Delete web service request queue
    }
    if (this->webSrvResponseQueue) {
        vQueueDelete(this->webSrvResponseQueue); // Delete web service response queue
    }
    if (this->requestMutex) {
        vSemaphoreDelete(this->requestMutex); // Delete request mutex
    }
}

/**
 * @brief Constructs an HTTP POST request for uploading an image.
 *
 * This function creates an HTTP POST request with the provided image data and associated metadata.
 * The request includes authentication token, image ID, and base64-encoded image data. The generated
 * request is then stored in the provided string pointer.
 *
 * @param[out] requestPtr Pointer to a string where the constructed POST request will be stored.
 * @param[in] image_id The ID of the image being uploaded.
 * @param[in] base64_image_data The base64-encoded image data to be uploaded.
 *
 * @return RequestHandlerReturnCode Returns `SUCCESS` if the request is created successfully.
 * @return                                 - `INVALID_ARGUMENT` if `requestPtr` is null or `base64_image_data` is empty.
 * @return                                 - `UN_CLASSIFIED_ERROR` if an error occurs during request creation.
 */
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

/**
 * @brief Constructs an HTTP POST request for uploading an image, using a file buffer.
 *
 * This function constructs an HTTP POST request to upload an image, including metadata such as
 * an authentication token, image ID, and base64-encoded image data. The request is stored in the
 * provided file buffer, and its length is validated against the maximum buffer size.
 *
 * @param[out] file_buffer Pointer to a buffer that will store the constructed POST request.
 *                         The buffer should have enough space to hold the entire request.
 * @param[in] buffer_max_size The maximum size allowed for the buffer to store the request.
 * @param[in] current_data_len The length of the image data to be uploaded.
 * @param[in] image_id The ID of the image being uploaded.
 *
 * @return int Returns the number of characters written to the buffer (excluding the null terminator)
 * @return            if successful. Returns a negative value on error:
 * @return            - `-1`: Request buffer is too small.
 * @return            - `-2`: Failed to allocate memory for temporary buffer.
 * @return            - `-3`: Failed to create POST request.
 */
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
 * @brief Constructs a GET request for fetching user instructions from the server.
 *
 * This function constructs an HTTP GET request to retrieve user instructions from the server.
 * The request includes the authentication token, domain, and port from the wireless handler.
 *
 * @param[out] requestPtr Pointer to a string where the constructed GET request will be stored.
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

/**
 * @brief Updates the user instructions GET request structure with the latest request string.
 *
 * This function creates a new user instructions GET request using `createUserInstructionsGETRequest`
 * and updates the `getUserInsturctionsRequest` structure with the request string, length, and request type.
 */
void RequestHandler::updateUserInstructionsGETRequest() {
    std::string getRequest;
    this->createUserInstructionsGETRequest(&getRequest);
    strncpy(this->getUserInsturctionsRequest.str_buffer, getRequest.c_str(),
            sizeof(this->getUserInsturctionsRequest.str_buffer) - 1);
    this->getUserInsturctionsRequest.str_buffer[sizeof(this->getUserInsturctionsRequest.str_buffer) - 1] = '\0';
    this->getUserInsturctionsRequest.buffer_length = getRequest.length();
    this->getUserInsturctionsRequest.requestType = RequestType::GET_COMMANDS;
}

/**
 * @brief Constructs a GET request for fetching the current timestamp from the server.
 *
 * This function constructs an HTTP GET request to retrieve the current timestamp from the server.
 * The request includes the domain and port from the wireless handler.
 *
 * @param[out] requestPtr Pointer to a string where the constructed GET request will be stored.
 */
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

void RequestHandler::processArgs(std::ostringstream &content, bool &first) {}

/**
 * @brief Parses the HTTP return code from the response string.
 *
 * This function extracts and parses the HTTP return code (e.g., 200, 404) from the provided
 * HTTP response string. The response string is expected to include the status line (e.g., "HTTP/1.1 200 OK").
 *
 * @param[in] responseString Pointer to the HTTP response string containing the status line.
 *
 * @return Returns the parsed HTTP return code (e.g., 200, 404), or -1 in case of an error (e.g., invalid response
 * format).
 *
 * @note If the response string is null, or if the status line cannot be found or parsed, the function returns -1.
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
 * @brief Retrieves a pointer to the user instructions GET request.
 *
 * This function returns a pointer to the `getUserInsturctionsRequest` object, which contains
 * the HTTP GET request for user instructions.
 *
 * @return Pointer to the `QueueMessage` object representing the user instructions GET request.
 */
QueueMessage *RequestHandler::getUserInstructionsGETRequestptr() { return &this->getUserInsturctionsRequest; }

/**
 * @brief Retrieves a pointer to the timestamp GET request.
 *
 * This function returns a pointer to the `getTimestampRequest` object, which contains
 * the HTTP GET request for the timestamp.
 *
 * @return Pointer to the `QueueMessage` object representing the timestamp GET request.
 */
QueueMessage *RequestHandler::getTimestampGETRequestptr() { return &this->getTimestampRequest; }

/**
 * @brief Retrieves the web server domain as a C-string.
 *
 * This function fetches the web server domain setting from the `wirelessHandler` object.
 * It returns the domain as a C-string, which is used for constructing HTTP requests.
 *
 * @return C-string representing the web server domain.
 */
const char *RequestHandler::getWebServerCString() { return this->wirelessHandler->get_setting(Settings::WEB_DOMAIN); }

/**
 * @brief Retrieves the web server port as a C-string.
 *
 * This function fetches the web server port setting from the `wirelessHandler` object.
 * It returns the port as a C-string, which is used for constructing HTTP requests.
 *
 * @return C-string representing the web server port.
 */
const char *RequestHandler::getWebPortCString() { return this->wirelessHandler->get_setting(Settings::WEB_PORT); }

/**
 * @brief Retrieves the web server token as a C-string.
 *
 * This function fetches the web server token setting from the `wirelessHandler` object.
 * It returns the token as a C-string, which is used for authentication in HTTP requests.
 *
 * @return C-string representing the web server token.
 */
const char *RequestHandler::getWebServerTokenCString() {
    return this->wirelessHandler->get_setting(Settings::WEB_TOKEN);
}

/**
 * @brief Retrieves the web server certificate as a C-string.
 *
 * This function fetches the web server certificate setting from the `wirelessHandler` object.
 * It returns the certificate as a C-string, which is used for secure communication with the web server.
 *
 * @return C-string representing the web server certificate.
 */
const char *RequestHandler::getWebServerCertCsring() {
    return this->wirelessHandler->get_setting(Settings::WEB_CERTIFICATE);
}

/**
 * @brief Retrieves the web server request queue handle.
 *
 * This function returns the handle to the queue used for managing web server requests.
 * The queue is used to store and manage incoming or outgoing requests for processing.
 *
 * @return Handle to the web server request queue (`QueueHandle_t`).
 */
QueueHandle_t RequestHandler::getWebSrvRequestQueue() { return this->webSrvRequestQueue; }

/**
 * Retrieves the queue handle used for web server response messages.
 *
 * @return QueueHandle_t - The handle to the queue where web server response messages are sent.
 *                         This queue is used to pass the HTTP response data back to other tasks.
 */
QueueHandle_t RequestHandler::getWebSrvResponseQueue() { return this->webSrvResponseQueue; }

/**
 * @brief Sends a request to the web server and waits for the response.
 *
 * This function initiates a network connection to the web server, sends the specified request,
 * and waits for the server's response. It handles DNS lookup, socket connection, request transmission,
 * and response reception. If the server responds successfully, the response is parsed into the provided
 * `QueueMessage` object.
 *
 * @param request Pointer to the request string to be sent to the server.
 * @param request_len Length of the request string.
 * @param response Pointer to a `QueueMessage` object where the response will be stored.
 *
 * @return - `RequestHandlerReturnCode::SUCCESS` if the request was successfully sent and the response received.
 * @return - `RequestHandlerReturnCode::NOT_CONNECTED` if the wireless connection is not active.
 * @return - `RequestHandlerReturnCode::DNS_LOOKUP_FAIL` if DNS lookup fails.
 * @return - `RequestHandlerReturnCode::SOCKET_ALLOCATION_FAIL` if socket allocation fails.
 * @return - `RequestHandlerReturnCode::SOCKET_CONNECT_FAIL` if socket connection fails.
 * @return - `RequestHandlerReturnCode::SOCKET_SEND_FAIL` if sending the request over the socket fails.
 * @return - `RequestHandlerReturnCode::SOCKET_RECEIVE_FAIL` if receiving the response fails.
 * @return - `RequestHandlerReturnCode::UN_CLASSIFIED_ERROR` if no data is received from the server.
 */
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

/**
 * @brief Sends a request to the web server and waits for the response (overloaded version).
 *
 * This is an overloaded version of the `sendRequest` function that accepts the request as a pointer
 * to an unsigned char array. The function casts the unsigned char pointer to a `const char*` and then
 * calls the main `sendRequest` function to handle the request sending and response reception.
 *
 * @param request Pointer to the request data (unsigned char array) to be sent to the server.
 * @param request_len Length of the request data.
 * @param response Pointer to a `QueueMessage` object where the response will be stored.
 *
 * @return - `RequestHandlerReturnCode::SUCCESS` if the request was successfully sent and the response received.
 * @return - `RequestHandlerReturnCode::NOT_CONNECTED` if the wireless connection is not active.
 * @return - `RequestHandlerReturnCode::DNS_LOOKUP_FAIL` if DNS lookup fails.
 * @return - `RequestHandlerReturnCode::SOCKET_ALLOCATION_FAIL` if socket allocation fails.
 * @return - `RequestHandlerReturnCode::SOCKET_CONNECT_FAIL` if socket connection fails.
 * @return - `RequestHandlerReturnCode::SOCKET_SEND_FAIL` if sending the request over the socket fails.
 * @return - `RequestHandlerReturnCode::SOCKET_RECEIVE_FAIL` if receiving the response fails.
 * @return - `RequestHandlerReturnCode::UN_CLASSIFIED_ERROR` if no data is received from the server.
 */
RequestHandlerReturnCode RequestHandler::sendRequest(const unsigned char *request, const size_t request_len,
                                                     QueueMessage *response) {
    return this->sendRequest(reinterpret_cast<const char *>(request), request_len, response);
}

/**
 * @brief Sends a request to the web server and waits for the response (overloaded version).
 *
 * This is an overloaded version of the `sendRequest` function that accepts the request as a `std::string`. 
 * The function converts the string into a C-style string (`const char*`) and then calls the main `sendRequest` 
 * function to handle the request sending and response reception.
 *
 * @param request A `std::string` containing the request data to be sent to the server.
 * @param response Pointer to a `QueueMessage` object where the response will be stored.
 *
 * @return - `RequestHandlerReturnCode::SUCCESS` if the request was successfully sent and the response received.
 * @return - `RequestHandlerReturnCode::NOT_CONNECTED` if the wireless connection is not active.
 * @return - `RequestHandlerReturnCode::DNS_LOOKUP_FAIL` if DNS lookup fails.
 * @return - `RequestHandlerReturnCode::SOCKET_ALLOCATION_FAIL` if socket allocation fails.
 * @return - `RequestHandlerReturnCode::SOCKET_CONNECT_FAIL` if socket connection fails.
 * @return - `RequestHandlerReturnCode::SOCKET_SEND_FAIL` if sending the request over the socket fails.
 * @return - `RequestHandlerReturnCode::SOCKET_RECEIVE_FAIL` if receiving the response fails.
 * @return - `RequestHandlerReturnCode::UN_CLASSIFIED_ERROR` if no data is received from the server.
 */
RequestHandlerReturnCode RequestHandler::sendRequest(std::string request, QueueMessage *response) {
    return this->sendRequest(request.c_str(), request.length(), response);
}

/**
 * @brief Sends a request from a `QueueMessage` object and waits for the response (overloaded version).
 *
 * This is an overloaded version of the `sendRequest` function that accepts a `QueueMessage` object as input. 
 * The function extracts the request data from the `QueueMessage`'s `str_buffer` and `buffer_length`, converts it 
 * into a `std::string`, and then calls the main `sendRequest` function to handle the request sending and response reception.
 *
 * @param request A `QueueMessage` object containing the request data (in `str_buffer` and `buffer_length`).
 * @param response Pointer to a `QueueMessage` object where the response will be stored.
 *
 * @return - `RequestHandlerReturnCode::SUCCESS` if the request was successfully sent and the response received.
 * @return - `RequestHandlerReturnCode::NOT_CONNECTED` if the wireless connection is not active.
 * @return - `RequestHandlerReturnCode::DNS_LOOKUP_FAIL` if DNS lookup fails.
 * @return - `RequestHandlerReturnCode::SOCKET_ALLOCATION_FAIL` if socket allocation fails.
 * @return - `RequestHandlerReturnCode::SOCKET_CONNECT_FAIL` if socket connection fails.
 * @return - `RequestHandlerReturnCode::SOCKET_SEND_FAIL` if sending the request over the socket fails.
 * @return - `RequestHandlerReturnCode::SOCKET_RECEIVE_FAIL` if receiving the response fails.
 * @return - `RequestHandlerReturnCode::UN_CLASSIFIED_ERROR` if no data is received from the server.
 */
RequestHandlerReturnCode RequestHandler::sendRequest(const QueueMessage request, QueueMessage *response) {
    return this->sendRequest(std::string(request.str_buffer, request.buffer_length), response);
}

/**
 * @brief Sends a request over a TLS connection and waits for the response.
 *
 * This function establishes a TLS connection to the web server using the provided request details. 
 * It sends the request data over the TLS connection and waits for the server's response, which is then 
 * stored in the provided `QueueMessage` response object. The function also handles connection establishment, 
 * request sending, and response receiving, with appropriate error handling at each step.
 *
 * @param request A pointer to a `char` array containing the request data.
 * @param request_len The length of the request data in bytes.
 * @param response A pointer to a `QueueMessage` object where the server's response will be stored.
 *
 * @return - `RequestHandlerReturnCode::SUCCESS` if the request was successfully sent and the response received.
 * @return - `RequestHandlerReturnCode::NOT_CONNECTED` if the wireless connection is not active.
 * @return - `RequestHandlerReturnCode::SOCKET_CONNECT_FAIL` if the TLS connection could not be established.
 * @return - `RequestHandlerReturnCode::SOCKET_SEND_FAIL` if sending the request over the TLS connection fails.
 * @return - `RequestHandlerReturnCode::UN_CLASSIFIED_ERROR` if receiving the response fails or no data is received.
 */
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

/**
 * @brief Sends a request over a TLS connection using an unsigned char request.
 *
 * This function is a wrapper around the `sendRequestTLS` method that accepts an `unsigned char` array as the 
 * request data. It casts the `unsigned char` array to a `const char*` and then delegates the request to 
 * the `sendRequestTLS` function for processing. It handles the TLS connection, request sending, and response receiving.
 *
 * @param request A pointer to an `unsigned char` array containing the request data.
 * @param request_len The length of the request data in bytes.
 * @param response A pointer to a `QueueMessage` object where the server's response will be stored.
 *
 * @return - `RequestHandlerReturnCode::SUCCESS` if the request was successfully sent and the response received.
 * @return - `RequestHandlerReturnCode::NOT_CONNECTED` if the wireless connection is not active.
 * @return - `RequestHandlerReturnCode::SOCKET_CONNECT_FAIL` if the TLS connection could not be established.
 * @return - `RequestHandlerReturnCode::SOCKET_SEND_FAIL` if sending the request over the TLS connection fails.
 * @return - `RequestHandlerReturnCode::UN_CLASSIFIED_ERROR` if receiving the response fails or no data is received.
 */
RequestHandlerReturnCode RequestHandler::sendRequestTLS(const unsigned char *request, const size_t request_len,
                                                        QueueMessage *response) {
    return this->sendRequestTLS(reinterpret_cast<const char *>(request), request_len, response);
}

/**
 * @brief Sends a request over a TLS connection using a `std::string` request.
 *
 * This function is a wrapper around the `sendRequestTLS` method that accepts a `std::string` as the 
 * request data. It converts the `std::string` to a C-style string (`const char*`) and then delegates the request to 
 * the `sendRequestTLS` function for processing. It handles the TLS connection, request sending, and response receiving.
 *
 * @param request A `std::string` containing the request data.
 * @param response A pointer to a `QueueMessage` object where the server's response will be stored.
 *
 * @return - `RequestHandlerReturnCode::SUCCESS` if the request was successfully sent and the response received.
 * @return - `RequestHandlerReturnCode::NOT_CONNECTED` if the wireless connection is not active.
 * @return - `RequestHandlerReturnCode::SOCKET_CONNECT_FAIL` if the TLS connection could not be established.
 * @return - `RequestHandlerReturnCode::SOCKET_SEND_FAIL` if sending the request over the TLS connection fails.
 * @return - `RequestHandlerReturnCode::UN_CLASSIFIED_ERROR` if receiving the response fails or no data is received.
 */
RequestHandlerReturnCode RequestHandler::sendRequestTLS(const std::string &request, QueueMessage *response) {
    return this->sendRequestTLS(request.c_str(), request.length(), response);
}

/**
 * @brief Sends a request over a TLS connection using a `QueueMessage` request.
 *
 * This function is a wrapper around the `sendRequestTLS` method that accepts a `QueueMessage` object as the request data. 
 * It converts the `QueueMessage` data (from `str_buffer` and `buffer_length`) into a `std::string` and then delegates the 
 * request to the `sendRequestTLS` function for processing. It handles the TLS connection, request sending, and response receiving.
 *
 * @param request A `QueueMessage` object containing the request data, with the request stored in `str_buffer` and the 
 *                buffer size in `buffer_length`.
 * @param response A pointer to a `QueueMessage` object where the server's response will be stored.
 *
 * @return - `RequestHandlerReturnCode::SUCCESS` if the request was successfully sent and the response received.
 * @return - `RequestHandlerReturnCode::NOT_CONNECTED` if the wireless connection is not active.
 * @return - `RequestHandlerReturnCode::SOCKET_CONNECT_FAIL` if the TLS connection could not be established.
 * @return - `RequestHandlerReturnCode::SOCKET_SEND_FAIL` if sending the request over the TLS connection fails.
 * @return - `RequestHandlerReturnCode::UN_CLASSIFIED_ERROR` if receiving the response fails or no data is received.
 */
RequestHandlerReturnCode RequestHandler::sendRequestTLS(const QueueMessage request, QueueMessage *response) {
    return this->sendRequestTLS(std::string(request.str_buffer, request.buffer_length), response);
}

/**
 * @brief Parses the response buffer and extracts a JSON string.
 *
 * This function extracts a substring from the provided response buffer that represents a valid JSON object, 
 * defined by the curly braces `{}`. The JSON portion is then copied back to the provided `QueueMessage` object, 
 * with the updated `buffer_length` reflecting the length of the extracted JSON.
 *
 * @param responseBuffer A pointer to a `QueueMessage` containing the response data to be parsed.
 * @param buffer_size The size of the response buffer.
 *
 * @return - 0 if the JSON object was successfully extracted and stored in the response buffer.
 * @return - 1 if no valid JSON object (curly braces) was found in the response.
 */
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

/**
 * @brief Parses a timestamp from an HTTP response.
 *
 * This function searches the provided HTTP response for the content field and attempts to extract a timestamp value.
 * It expects the timestamp to be a numeric value following the "\r\n\r\n" separator. The function checks that the 
 * timestamp field contains only numeric characters and returns the parsed timestamp as a 64-bit integer. 
 * If the content or timestamp field is invalid, the function returns an error code.
 *
 * @param response The HTTP response as a string from which to parse the timestamp.
 *
 * @return - The parsed timestamp as a 64-bit integer if successful.
 * @return - -1 if the "Content" field was not found in the response.
 * @return - -2 if the timestamp field was not found or is improperly formatted.
 * @return - -3 if the timestamp contains non-numeric characters.
 */
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

/**
 * @brief Gets the current time synchronization status.
 *
 * This function returns the current status of time synchronization. It checks whether the time has been synchronized 
 * with the server or not, based on the internal `timeSynchronized` variable.
 *
 * @return - `true` if the time has been synchronized, 
 * @return - `false` otherwise.
 */
bool RequestHandler::getTimeSyncedStatus() { return this->timeSynchronized; }

/**
 * @brief Sets the time synchronization status.
 *
 * This function updates the internal status variable `timeSynchronized` to indicate whether the time has been
 * synchronized with the server. The status is set based on the provided boolean value.
 *
 * @param status A boolean indicating the desired time synchronization status.
 *               - `true` to mark the time as synchronized.
 *               - `false` to mark the time as unsynchronized.
 */
void RequestHandler::setTimeSyncedStatus(bool status) { this->timeSynchronized = status; }

/**
 * @brief Adds a request message to the specified queue.
 *
 * This function attempts to send a message to a specified FreeRTOS queue. If the message is successfully
 * added to the queue, it returns `true`; otherwise, it returns `false` and logs an error.
 *
 * @param queueHandle The handle of the queue to which the message will be sent.
 * @param message The message to be added to the queue.
 * 
 * @return - `true` if the message was successfully sent to the queue, 
 * @return - `false` if the operation failed (e.g., the queue is full).
 */
bool RequestHandler::addRequestToQueue(QueueHandle_t queueHandle, QueueMessage message) {
    if (xQueueSend(queueHandle, &message, 0) != pdTRUE) {
        DEBUG("Failed to send message to queue");
        return false;
    }
    return true;
}

/**
 * @brief Adds a request message to the specified queue based on the provided queue ID.
 *
 * This function selects the appropriate queue based on the given `QueueID` and attempts to send the
 * provided message to that queue. If the message is successfully added to the queue, it returns `true`;
 * otherwise, it returns `false` and logs an error if the queue ID is invalid.
 *
 * @param queueid The `QueueID` that specifies which queue to send the message to. 
 * @param message The message to be added to the queue.
 * 
 * @return - `true` if the message was successfully sent to the queue, 
 * @return - `false` if the operation failed or an invalid queue ID was provided.
 */
bool RequestHandler::addRequestToQueue(QueueID queueid, QueueMessage message) {
    switch (queueid) {
        case QueueID::WEB_SRV_REQUEST_QUEUE:
            return addRequestToQueue(this->webSrvRequestQueue, message);
        case QueueID::WEB_SRV_RESPONSE_QUEUE:
            return addRequestToQueue(this->webSrvResponseQueue, message);
        default:
            DEBUG("Invalid queue ID");
            return false;
    }
}