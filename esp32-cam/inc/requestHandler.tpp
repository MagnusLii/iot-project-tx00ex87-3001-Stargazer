#ifndef REQUEST_HANDLER_TPP
#define REQUEST_HANDLER_TPP

#include "requestHandler.hpp"
#include <iostream>
#include <sstream>
#include <type_traits>

template <typename... Args>
RequestHandlerReturnCode RequestHandler::createGenericPOSTRequest(std::string *requestPtr, const char *endpoint,
                                                                  Args... args) {
    if (requestPtr == nullptr) {
        std::cerr << "Error: requestPtr is null" << std::endl;
        return RequestHandlerReturnCode::INVALID_ARGUMENT;
    }

    if (sizeof...(args) % 2 != 0) {
        std::cerr << "Error: Number of arguments is not divisible by 2" << std::endl;
        return RequestHandlerReturnCode::INVALID_NUM_OF_ARGS;
    }

    std::ostringstream content;
    content << "{";
    bool first = true;
    processArgs(content, first, args...);
    content << "}";

    *requestPtr = "POST " + std::string(endpoint) +
                  " HTTP/1.0\r\n"
                  "Host: example.com\r\n"
                  "User-Agent: esp-idf/1.0 esp32\r\n"
                  "Connection: close\r\n"
                  "Content-Type: application/json\r\n"
                  "Content-Length: " +
                  std::to_string(content.str().length()) + "\r\n\r\n" + content.str();

    std::cout << "Request: " << *requestPtr << std::endl;

    return RequestHandlerReturnCode::SUCCESS;
}

// Process key-value pairs
template <typename T, typename U, typename... Args>
void RequestHandler::processArgs(std::ostringstream &content, bool &first, T key, U value, Args... args) {
    if (!first) { content << ","; }
    first = false;
    content << "\"" << key << "\":" << formatValue(value);
    processArgs(content, first, args...);
}

// Helper function to format values as JSON
template <typename T> std::string RequestHandler::formatValue(T value) {
    if constexpr (std::is_integral_v<T>) {
        return std::to_string(value);
    } else if constexpr (std::is_same_v<T, const char *> || std::is_same_v<T, std::string>) {
        return "\"" + std::string(value) + "\"";
    } else {
        static_assert(sizeof(T) == 0, "Unsupported data type");
    }
}

#endif // REQUEST_HANDLER_TPP
