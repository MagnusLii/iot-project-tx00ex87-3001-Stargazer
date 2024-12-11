#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include <string.h>

#include "debug.hpp"
#include "lwip/dns.h"
#include "lwip/err.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "requestHandler.hpp"
#include "sdkconfig.h"
#include <memory>

// for testing
#define WEB_SERVER "webhook.site"
#define WEB_PORT   "80"
#define WEB_PATH   "/cb950d1e-6f64-449c-a1f9-bbc1d0e94e0c"

RequestHandler::RequestHandler(std::string webServer, std::string webPort, std::string webPath,
                               std::shared_ptr<WirelessHandler> wirelessHandler) {
    this->webServer = webServer;
    this->webPort = webPort;
    this->webPath = webPath;
    this->wirelessHandler = wirelessHandler;
    this->webSrvRequestQueue = xQueueCreate(QUEUE_SIZE, sizeof(QueueMessage *));
    this->webSrvResponseQueue = xQueueCreate(QUEUE_SIZE, sizeof(QueueMessage *));
}

RequestHandlerReturnCode RequestHandler::createDiagnosticsPOSTRequest(std::string *requestPtr) {
    // Implement when requests are more clear.
    return RequestHandlerReturnCode::SUCCESS;
}

RequestHandlerReturnCode RequestHandler::createImagePOSTRequest(std::string *requestPtr) {
    // Implement when requests are more clear.
    return RequestHandlerReturnCode::SUCCESS;
}

RequestHandlerReturnCode RequestHandler::createUserInstructionsGETRequest(std::string *requestPtr) {
    // Implement when requests are more clear.
    return RequestHandlerReturnCode::SUCCESS;
}

void createTestGETRequest(std::string *request) {
    std::string query = "?1=1&2=2";

    *request = "GET " WEB_PATH + query +
               " HTTP/1.0\r\n"
               "Host: " WEB_SERVER ":" WEB_PORT "\r\n"
               "User-Agent: esp-idf/1.0 esp32\r\n"
               "Connection: keep-alive\r\n"
               "\r\n";
}

void createTestPOSTRequest(std::string *request) {
    std::string json = "{\"1\":\"1\", \"2\":\"2\"}";

    *request = "POST " WEB_PATH " HTTP/1.0\r\n"
               "Host: " WEB_SERVER ":" WEB_PORT "\r\n"
               "User-Agent: esp-idf/1.0 esp32\r\n"
               "Connection: keep-alive\r\n"
               "Content-Length: " +
               std::to_string(json.length()) +
               "\r\n"
               "Content-Type: application/json\r\n"
               "\r\n" +
               json;
}

const char* RequestHandler::getWebServerCString() {
    return this->webServer.c_str();
}

const char* RequestHandler::getWebPortCString() {
    return this->webPort.c_str();
}

QueueHandle_t RequestHandler::getWebSrvRequestQueue() {
    return this->webSrvRequestQueue;
}

QueueHandle_t RequestHandler::getWebSrvResponseQueue() {
    return this->webSrvResponseQueue;
}

RequestHandlerReturnCode RequestHandler::sendRequest(QueueMessage message) {
    
}

// -------------------------------------------------------------
// --------------------------- TASKS ---------------------------
// -------------------------------------------------------------

void sendRequestsTask(void *pvParameters) {
    RequestHandler *requestHandler = (RequestHandler *)pvParameters;
    QueueMessage *message = nullptr;
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

    while (true) {
        // Wait for a request to be available
        if (xQueueReceive(requestHandler->getWebSrvRequestQueue(), &message, portMAX_DELAY) == pdTRUE) {

            // DNS lookup
            int err = getaddrinfo(requestHandler->getWebServerCString(), requestHandler->getWebPortCString(), &hints, &dns_lookup_results);
            if (err != 0 || dns_lookup_results == nullptr) {
                DEBUG("DNS lookup failed err=", err, " dns_lookup_results=", dns_lookup_results);
                vTaskDelay(1000 / portTICK_PERIOD_MS);
                continue;
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
                continue;
            }
            DEBUG("... allocated socket");

            // Connect to server
            if (connect(socket_descriptor, dns_lookup_results->ai_addr, dns_lookup_results->ai_addrlen) != 0) {
                DEBUG("... socket connect failed errno=", errno);
                close(socket_descriptor);
                freeaddrinfo(dns_lookup_results);
                vTaskDelay(5000 / portTICK_PERIOD_MS);
                continue;
            }
            DEBUG("... connected");

            // Send request
            freeaddrinfo(dns_lookup_results);
            if (write(socket_descriptor, message->message, strlen(message->message)) < 0) {
                DEBUG("... socket send failed\n");
                close(socket_descriptor);
                vTaskDelay(5000 / portTICK_PERIOD_MS);
                continue;
            }
            DEBUG("... socket send success");

            // Set receiving timeout
            if (setsockopt(socket_descriptor, SOL_SOCKET, SO_RCVTIMEO, &receiving_timeout, sizeof(receiving_timeout)) < 0) {
                DEBUG("... failed to set socket receiving timeout");
                close(socket_descriptor);
                vTaskDelay(4000 / portTICK_PERIOD_MS);
                continue;
            }
            DEBUG("... set socket receiving timeout success");

            /* Read HTTP response */
            do { bzero(receive_buffer, sizeof(receive_buffer));
                read_result = read(socket_descriptor, receive_buffer, sizeof(receive_buffer) - 1);
                for (int i = 0; i < read_result; i++) {
                    putchar(receive_buffer[i]);
                }
            } while (read_result > 0);

            DEBUG("\n... done reading from socket. Last read return=", read_result, " errno=", errno);
            close(socket_descriptor);

            // Send response to the response queue
            strncpy(message->message, receive_buffer, BUFFER_SIZE);
            message->message[BUFFER_SIZE - 1] = '\0';
            message->messageType = MessageType::WEB_SERVER_RESPONSE;
            while (xQueueSend(requestHandler->getWebSrvResponseQueue(), &message, 0) != pdTRUE && retry_count < ENQUEUE_REQUEST_RETRIES) {
                retry_count++;
                vTaskDelay(100 / portTICK_PERIOD_MS);
            }
            retry_count = 0;
        }
    }
}
