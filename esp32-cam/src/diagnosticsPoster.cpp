#include "diagnosticsPoster.hpp"
#include "defines.hpp"
#include "debug.hpp"

/**
 * @brief Constructs a DiagnosticsPoster object.
 *
 * This constructor initializes the DiagnosticsPoster with shared pointers to a 
 * RequestHandler and a WirelessHandler.
 *
 * @param requestHandler Shared pointer to the RequestHandler instance.
 * @param wirelessHandler Shared pointer to the WirelessHandler instance.
 */
DiagnosticsPoster::DiagnosticsPoster(std::shared_ptr<RequestHandler> requestHandler,
                                     std::shared_ptr<WirelessHandler> wirelessHandler) {
    this->requestHandler = requestHandler;
    this->wirelessHandler = wirelessHandler;
}

/**
 * @brief Destructor for the DiagnosticsPoster class.
 *
 * @return void
 */
DiagnosticsPoster::~DiagnosticsPoster() {}


/**
 * @brief Adds a diagnostics message to the request queue.
 *
 * This function constructs a POST request containing a diagnostics message and 
 * enqueues it for processing by the request handler.
 *
 * @param message The diagnostics message to be sent.
 * @param status_level The status level of the diagnostics message.
 *
 * @return bool Returns:
 * @return        - `true` if the request was successfully added to the queue.
 * @return        - `false` if request construction failed or the request was too long.
 *
 * @note Clears the message and request strings after use to save memory.
 *       Ensures that the request does not exceed the buffer size.
 */
bool DiagnosticsPoster::add_diagnostics_to_queue(std::string message, DiagnosticsStatus status_level) {
    std::string request;
    this->requestHandler->createGenericPOSTRequest(&request, "/api/diagnostics", "token",
                                                   wirelessHandler->get_setting(Settings::WEB_TOKEN), "status",
                                                   static_cast<int>(status_level), "message", message.c_str());
    message.clear(); // Clear to save memory

    if (request.empty()) {
        DEBUG("Error: Request string is empty after construction");
        return false;
    }
    if (request.length() > BUFFER_SIZE - 1) {
        DEBUG("Error: Request string is too long");
        return false;
    }

    QueueMessage queueMessage;
    queueMessage.buffer_length = request.length();
    strncpy(queueMessage.str_buffer, request.c_str(), sizeof(queueMessage.str_buffer) - 1);
    request.clear(); // Clear to save memory
    queueMessage.str_buffer[queueMessage.buffer_length] = '\0';
    queueMessage.requestType = RequestType::POST;

    return this->requestHandler->addRequestToQueue(QueueID::WEB_SRV_REQUEST_QUEUE, queueMessage);
}
