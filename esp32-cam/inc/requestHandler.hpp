#ifndef REQUEST_HANDLER_HPP
#define REQUEST_HANDLER_HPP

#include "defines.hpp"
#include "sd-card.hpp"
#include "wireless.hpp"
#include <memory>
#include <string>

#define ENQUEUE_REQUEST_RETRIES 3
#define QUEUE_SIZE              3

enum class RequestType {
    GET_COMMANDS,
    POST_IMAGE,
    POST_DIAGNOSTICS,
    API_UPLOAD,
    API_COMMAND,
    API_DIAGNOSTICS,
    API_TIME,
};

enum class WebServerResponseType {
};

struct QueueMessage {
    char str_buffer[BUFFER_SIZE];
    char imageFilename[BUFFER_SIZE];
    RequestType requestType;
};

enum class RequestHandlerReturnCode {
    SUCCESS,
    UN_CLASSIFIED_ERROR,
    DNS_LOOKUP_FAIL,
    SOCKET_ALLOCATION_FAIL,
    SOCKET_CONNECT_FAIL,
    SOCKET_SEND_FAIL,
    SOCKET_TIMEOUT_FAIL
};

class RequestHandler {
  public:
    RequestHandler(std::string webServer, std::string webPort, std::string webServerToken,
                   std::shared_ptr<WirelessHandler> wirelessHandler, std::shared_ptr<SDcard> sdcard);

    RequestHandlerReturnCode createDiagnosticsPOSTRequest(std::string *requestPtr);
    RequestHandlerReturnCode createImagePOSTRequest(std::string *requestPtr);
    RequestHandlerReturnCode createUserInstructionsGETRequest(std::string *requestPtr);
    QueueMessage *getUserInstructionsGETRequestptr();

    const char *getWebServerCString();
    const char *getWebPortCString();
    QueueHandle_t getWebSrvRequestQueue();
    QueueHandle_t getWebSrvResponseQueue();

    RequestHandlerReturnCode sendRequest(QueueMessage message);
    RequestHandlerReturnCode sendRequest(std::string request);

    int parseResponse(char *buffer);

  private:
    std::string webServer;
    std::string webPort;
    std::string webServerToken;

    std::shared_ptr<WirelessHandler> wirelessHandler;
    std::shared_ptr<SDcard> sdcard;
    QueueHandle_t webSrvRequestQueue;  // Queue for sending requests to the web server
    QueueHandle_t webSrvResponseQueue; // Queue where responses from the web server are forwarded

    QueueMessage getUserInsturctionsRequest;
};

void createTestGETRequest(std::string *request);
void createTestPOSTRequest(std::string *request);

#endif