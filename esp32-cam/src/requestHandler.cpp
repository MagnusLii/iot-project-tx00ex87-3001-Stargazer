#include "requestHandler.hpp"
#include "testMacros.hpp"

RequestHandler::RequestHandler(std::string webServer, std::string webPort, std::string webPath,
                               std::shared_ptr<WirelessHandler> wirelessHandler, std::shared_ptr<SDcard> sdcard) {
    this->webServer = webServer;
    this->webPort = webPort;
    this->webPath = webPath;
    this->wirelessHandler = wirelessHandler;
    this->sdcard = sdcard;
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

// TEST FUNCTION
void createTestGETRequest(std::string *request) {
    std::string query = "?1=1&2=2";

    *request = "GET " WEB_PATH + query +
               " HTTP/1.0\r\n"
               "Host: " WEB_SERVER ":" WEB_PORT "\r\n"
               "User-Agent: esp-idf/1.0 esp32\r\n"
               "Connection: keep-alive\r\n"
               "\r\n";
}

// TEST FUNCTION
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
    return  RequestHandlerReturnCode::SUCCESS;
}
