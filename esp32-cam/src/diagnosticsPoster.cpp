#include "diagnosticsPoster.hpp"

DiagnosticsPoster::DiagnosticsPoster(std::shared_ptr<RequestHandler> requestHandler,
                                     std::shared_ptr<WirelessHandler> wirelessHandler) {
    this->requestHandler = requestHandler;
    this->wirelessHandler = wirelessHandler;
}

bool DiagnosticsPoster::add_diagnostics_to_queue(std::string message, DiagnosticsStatus status_level) {
    std::string request;
    this->requestHandler->createGenericPOSTRequest(&request, "/api/diagnostics", "token",
                                                   wirelessHandler->get_setting(Settings::WEB_TOKEN), "status",
                                                   static_cast<int>(status_level), "message", message.c_str());
}
