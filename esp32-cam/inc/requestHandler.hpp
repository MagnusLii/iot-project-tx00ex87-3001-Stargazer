#ifndef REQUEST_HANDLER_HPP
#define REQUEST_HANDLER_HPP

#include "wireless.hpp"
#include <memory>
#include <string>
#include "sd-card.hpp"

#define BUFFER_SIZE             1024
#define ENQUEUE_REQUEST_RETRIES 3
#define QUEUE_SIZE              10

enum class RequestType {
    GET_COMMANDS,
    POST_IMAGE,
    POST_DIAGNOSTICS,
    WEB_SERVER_RESPONSE
};

struct QueueMessage {
    char request[BUFFER_SIZE];
    char imageFilename[BUFFER_SIZE];
    RequestType requestType;
};

enum class RequestHandlerReturnCode {
    SUCCESS,
    UN_CLASSIFIED_ERROR
};

class RequestHandler {
  public:
    RequestHandler(std::string webServer, std::string webPort, std::string webPath,
                   std::shared_ptr<WirelessHandler> wirelessHandler, std::shared_ptr<SDcard> sdcard);

    RequestHandlerReturnCode createDiagnosticsPOSTRequest(std::string *requestPtr);
    RequestHandlerReturnCode createImagePOSTRequest(std::string *requestPtr);
    RequestHandlerReturnCode createUserInstructionsGETRequest(std::string *requestPtr);

    const char *getWebServerCString();
    const char *getWebPortCString();
    QueueHandle_t getWebSrvRequestQueue();
    QueueHandle_t getWebSrvResponseQueue();

    RequestHandlerReturnCode sendRequest(QueueMessage message);

  private:
    std::string webServer;
    std::string webPort;
    std::string webPath;

    std::shared_ptr<WirelessHandler> wirelessHandler;
    std::shared_ptr<SDcard> sdcard;
    QueueHandle_t webSrvRequestQueue;
    QueueHandle_t webSrvResponseQueue;
};

void createTestGETRequest(std::string *request);
void createTestPOSTRequest(std::string *request);
void sendRequestsTask(void *pvParameters);

#endif