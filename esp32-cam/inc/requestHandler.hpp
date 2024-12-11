#ifndef REQUEST_HANDLER_HPP
#define REQUEST_HANDLER_HPP

#include <string>
#include "wireless.hpp"
#include <memory>

#define BUFFER_SIZE 1024
#define ENQUEUE_REQUEST_RETRIES 3
#define QUEUE_SIZE 10

struct QueueMessage {
    char message[BUFFER_SIZE];
    MessageType messageType;
};

enum class MessageType {
    GET_COMMANDS,
    POST_IMAGE,
    POST_DIAGNOSTICS,
    WEB_SERVER_RESPONSE
}

enum class RequestHandlerReturnCode {
    SUCCESS,
    UN_CLASSIFIED_ERROR
};

class RequestHandler {
    public:
        RequestHandler(std::string webServer, std::string webPort, std::string webPath, std::shared_ptr<WirelessHandler> wirelessHandler);
        
        RequestHandlerReturnCode createDiagnosticsPOSTRequest(std::string *requestPtr);
        RequestHandlerReturnCode createImagePOSTRequest(std::string *requestPtr);
        RequestHandlerReturnCode createUserInstructionsGETRequest(std::string *requestPtr);

        const char* getWebServerCString();
        const char* getWebPortCString();
        QueueHandle_t getWebSrvRequestQueue();
        QueueHandle_t getWebSrvResponseQueue();

        RequestHandlerReturnCode sendRequest(QueueMessage message);

    private:
        std::string webServer;
        std::string webPort;
        std::string webPath;

        std::shared_ptr<WirelessHandler> wirelessHandler;
        QueueHandle_t webSrvRequestQueue;
        QueueHandle_t webSrvResponseQueue;
};

void createTestGETRequest(std::string *request);
void createTestPOSTRequest(std::string *request);
void sendRequestsTask(void *pvParameters);

#endif

// void send_request_task(void *pvParameters){
//     RequestHandler *requestHandler = (RequestHandler *)pvParameters;
//     requestHandler->createRequest();
// }