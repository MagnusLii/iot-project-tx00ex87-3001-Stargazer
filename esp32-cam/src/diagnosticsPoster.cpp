#include "diagnosticsPoster.hpp"

DiagnosticsPoster::DiagnosticsPoster(QueueHandle_t *webSrvRequestQueue) {
    this->webSrvRequestQueue = webSrvRequestQueue;
}

