
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include <string>

class DiagnosticsPoster {
    public:
        DiagnosticsPoster(QueueHandle_t *webSrvRequestQueue);
        void postDiagnostics(std::string message, int status);

    private:
        QueueHandle_t *webSrvRequestQueue;
};