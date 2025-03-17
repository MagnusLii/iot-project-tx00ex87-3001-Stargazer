#ifndef DIAGNOSTICSPOSTER_HPP
#define DIAGNOSTICSPOSTER_HPP

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "requestHandler.hpp"
#include <string>

enum class DiagnosticsStatus {
    INFO = 1,
    WARNING = 2,
    ERROR = 3
};

class DiagnosticsPoster {
  public:
    DiagnosticsPoster(std::shared_ptr<RequestHandler> requestHandler, std::shared_ptr<WirelessHandler> wirelessHandler);
    ~DiagnosticsPoster();
    bool add_diagnostics_to_queue(std::string message, DiagnosticsStatus status_level);

  private:
    std::shared_ptr<RequestHandler> requestHandler;
    std::shared_ptr<WirelessHandler> wirelessHandler;
};

#endif // DIAGNOSTICSPOSTER_HPP