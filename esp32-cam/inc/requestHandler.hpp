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
    int buffer_length;
    char imageFilename[BUFFER_SIZE];
    int image_id;
    RequestType requestType;
};

enum class RequestHandlerReturnCode {
    SUCCESS,
    UN_CLASSIFIED_ERROR,
    DNS_LOOKUP_FAIL,
    SOCKET_ALLOCATION_FAIL,
    SOCKET_CONNECT_FAIL,
    SOCKET_SEND_FAIL,
    SOCKET_TIMEOUT_FAIL,
    FAILED_MUTEX_AQUISITION,
    INVALID_ARGUMENT,
    INVALID_NUM_OF_ARGS,
    FAILED_TO_CREATE_REQUEST,
};

class RequestHandler {
  public:
    RequestHandler(std::string webServer, std::string webPort, std::string webServerToken,
                   std::shared_ptr<WirelessHandler> wirelessHandler, std::shared_ptr<SDcardHandler> sdcardHandler);
    // TODO: Add destructor

    RequestHandlerReturnCode createDiagnosticsPOSTRequest(std::string *requestPtr);
    RequestHandlerReturnCode createImagePOSTRequest(std::string *requestPtr, const int image_id, std::string base64_image_data);
    RequestHandlerReturnCode createUserInstructionsGETRequest(std::string *requestPtr);
    RequestHandlerReturnCode createGenericPOSTRequest(std::string *requestPtr, const char *endpoint, int numOfVariableArgs, ...);
    QueueMessage *getUserInstructionsGETRequestptr();

    const char *getWebServerCString();
    const char *getWebPortCString();
    QueueHandle_t getWebSrvRequestQueue();
    QueueHandle_t getWebSrvResponseQueue();

    RequestHandlerReturnCode sendRequest(const QueueMessage message, QueueMessage* response);
    RequestHandlerReturnCode sendRequest(std::string request, QueueMessage* response);

    int parseResponseIntoJson(QueueMessage* responseBuffer, const int buffer_size);

  private:
    std::string webServer;
    std::string webPort;
    std::string webServerToken;

    std::shared_ptr<WirelessHandler> wirelessHandler; // TODO: remove dependency on direct access to wirelessHandler
    std::shared_ptr<SDcardHandler> sdcardHandler; // TODO: remove dependency on direct access to sdcardHandler
    
    QueueHandle_t webSrvRequestQueue;  // Queue for sending requests to the web server
    QueueHandle_t webSrvResponseQueue; // Queue where responses from the web server are forwarded

    SemaphoreHandle_t requestMutex;

    QueueMessage getUserInsturctionsRequest;
};

#endif