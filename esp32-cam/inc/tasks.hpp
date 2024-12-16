#ifndef TASKS_HPP
#define TASKS_HPP

#include "requestHandler.hpp"

enum class TaskReturnCode {
    SUCCESS,
    GENERIC_FAILURE,
    SOCKET_ALLOCATION_FAIL,
    SOCKET_CONNECT_FAIL,
    SOCKET_SEND_FAIL,
    SOCKET_TIMEOUT_FAIL,
    DNS_LOOKUP_FAIL
};

void send_request_task(void *pvParameters);
TaskReturnCode send_request_and_enqueue_response(RequestHandler *requestHandler, QueueMessage *message);

#endif