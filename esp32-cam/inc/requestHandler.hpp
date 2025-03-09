#ifndef REQUEST_HANDLER_HPP
#define REQUEST_HANDLER_HPP

#include "TLSWrapper.hpp"
#include "defines.hpp"
#include "sd-card.hpp"
#include "wireless.hpp"
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <type_traits>

enum class RequestType {
    UNDEFINED,
    GET_COMMANDS,
    POST_IMAGE,
    POST,
    GET_TIME,
};

enum class WebServerResponseType {
};

struct QueueMessage {
    char str_buffer[BUFFER_SIZE] = {0};
    int buffer_length = 0;
    char imageFilename[BUFFER_SIZE] = {0};
    int image_id = 0;
    RequestType requestType = RequestType::UNDEFINED;
};

enum class RequestHandlerReturnCode {
    SUCCESS,
    UN_CLASSIFIED_ERROR,
    DNS_LOOKUP_FAIL,
    SOCKET_ALLOCATION_FAIL,
    SOCKET_CONNECT_FAIL,
    SOCKET_SEND_FAIL,
    SOCKET_RECEIVE_FAIL,
    SOCKET_TIMEOUT_FAIL,
    FAILED_MUTEX_AQUISITION,
    INVALID_ARGUMENT,
    INVALID_NUM_OF_ARGS,
    FAILED_TO_CREATE_REQUEST,
    NOT_CONNECTED,
    MEM_ALLOCATION_FAIL,
};

enum QueueID {
    WEB_SRV_REQUEST_QUEUE,
    WEB_SRV_RESPONSE_QUEUE,
};

class RequestHandler {
  public:
    RequestHandler(std::shared_ptr<WirelessHandler> wirelessHandler, std::shared_ptr<SDcardHandler> sdcardHandler);
    ~RequestHandler();

    RequestHandlerReturnCode createImagePOSTRequest(std::string *requestPtr, const int image_id,
                                                    std::string base64_image_data);
    int createImagePOSTRequest(unsigned char *file_buffer, const size_t buffer_max_size, int current_data_len,
                               const int64_t image_id);
    void createUserInstructionsGETRequest(std::string *requestPtr);
    void updateUserInstructionsGETRequest();
    void createTimestampGETRequest(std::string *requestPtr);

    template <typename... Args>
    RequestHandlerReturnCode createGenericPOSTRequest(std::string *requestPtr, const char *endpoint, Args... args);

    int parseHttpReturnCode(const char *responseString);
    QueueMessage *getUserInstructionsGETRequestptr();
    QueueMessage *getTimestampGETRequestptr();

    const char *getWebServerCString();
    const char *getWebPortCString();
    const char *getWebServerTokenCString();
    const char *getWebServerCertCsring();
    QueueHandle_t getWebSrvRequestQueue();
    QueueHandle_t getWebSrvResponseQueue();

    RequestHandlerReturnCode sendRequest(const char *request, const size_t request_len, QueueMessage *response);
    RequestHandlerReturnCode sendRequest(const unsigned char *request, const size_t request_len,
                                         QueueMessage *response);
    RequestHandlerReturnCode sendRequest(const QueueMessage message, QueueMessage *response);
    RequestHandlerReturnCode sendRequest(std::string request, QueueMessage *response);
    RequestHandlerReturnCode sendRequestTLS(const char *request, const size_t request_len, QueueMessage *response);
    RequestHandlerReturnCode sendRequestTLS(const unsigned char *request, const size_t request_len,
                                            QueueMessage *response);
    RequestHandlerReturnCode sendRequestTLS(const QueueMessage request, QueueMessage *response);
    RequestHandlerReturnCode sendRequestTLS(const std::string &request, QueueMessage *response);

    int parseResponseIntoJson(QueueMessage *responseBuffer, const int buffer_size);
    int64_t parseTimestamp(const std::string &response);

    bool getTimeSyncedStatus();
    void setTimeSyncedStatus(bool status);

    bool addRequestToQueue(QueueHandle_t queueHandle, QueueMessage message);
    bool addRequestToQueue(QueueID queueid, QueueMessage message);

  private:
    // Helper functions for createGenericPOSTRequest
    template <typename T, typename U, typename... Args>
    void processArgs(std::ostringstream &content, bool &first, T key, U value, Args... args);
    void processArgs(std::ostringstream &content, bool &first);
    template <typename T> std::string formatValue(T value);

    std::shared_ptr<WirelessHandler> wirelessHandler;
    std::shared_ptr<SDcardHandler> sdcardHandler;
    std::shared_ptr<TLSWrapper> tlsWrapper;

    QueueHandle_t webSrvRequestQueue;  // Queue for sending requests to the web server
    QueueHandle_t webSrvResponseQueue; // Queue where responses from the web server are forwarded

    SemaphoreHandle_t requestMutex;

    QueueMessage getUserInsturctionsRequest;
    QueueMessage getTimestampRequest;

    bool timeSynchronized = false;
};

#include "requestHandler.tpp"

#endif