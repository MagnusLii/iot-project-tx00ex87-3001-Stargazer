#ifndef REQUEST_HANDLER_TPP
#define REQUEST_HANDLER_TPP

#include "requestHandler.hpp"
#include <iostream>
#include <sstream>
#include <type_traits>

/**
 * @brief Creates a generic POST request with JSON body for the specified endpoint.
 * 
 * This template function constructs a complete HTTP POST request with the provided key-value 
 * pairs formatted as JSON in the request body. The function ensures that the number of arguments 
 * is even (each key must have a corresponding value) and handles any additional arguments through 
 * recursion. The request includes necessary headers like `Content-Type` and `Content-Length`, 
 * and it is designed to work with multiple types of argument pairs.
 * 
 * The function writes the generated request to the provided `requestPtr`, which points to a string 
 * containing the full HTTP POST request.
 * 
 * @tparam Args The types of the key-value pairs passed to create the JSON body.
 * 
 * @param requestPtr A pointer to the string that will hold the generated POST request.
 * @param endpoint The endpoint to which the request will be sent.
 * @param args The key-value pairs to be included in the JSON body of the request. These must 
 *             be passed in pairs (key, value).
 * 
 * @return `RequestHandlerReturnCode::SUCCESS` if the request is successfully created. 
 *         `RequestHandlerReturnCode::INVALID_ARGUMENT` if `requestPtr` is null. 
 *         `RequestHandlerReturnCode::INVALID_NUM_OF_ARGS` if the number of arguments is not 
 *         divisible by 2.
 * 
 * @note The function assumes that each key is a string and each value can be formatted via the 
 *       `formatValue()` method.
 */
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
                  "Host: " + this->getWebServerCString() +  "\r\n"
                  "User-Agent: esp-idf/1.0 esp32\r\n"
                  "Connection: close\r\n"
                  "Content-Type: application/json\r\n"
                  "Content-Length: " +
                  std::to_string(content.str().length()) + "\r\n\r\n" + content.str();

    return RequestHandlerReturnCode::SUCCESS;
}

/**
 * @brief Processes and formats multiple arguments into a JSON-like structure.
 * 
 * This template function recursively processes each key-value pair, formatting it as a JSON-like 
 * string. The function appends a comma between entries if necessary, and formats the key as a string 
 * with double quotes and the value using `formatValue()`. The recursion stops when no more arguments 
 * are provided.
 * 
 * The function handles a variable number of arguments (using variadic templates) and ensures that 
 * the first entry does not have a leading comma.
 * 
 * @tparam T The type of the current key.
 * @tparam U The type of the current value.
 * @tparam Args The types of the remaining arguments (if any).
 * 
 * @param content A reference to the `std::ostringstream` object where the formatted result is 
 *                accumulated.
 * @param first A reference to a boolean flag that tracks whether the current entry is the first 
 *              argument to prevent a leading comma.
 * @param key The key for the current argument to be formatted.
 * @param value The value for the current argument to be formatted.
 * @param args Additional key-value pairs to be processed (if any).
 * 
 * @note This function should be called with at least one key-value pair, and it will recursively 
 *       handle any additional arguments passed in the parameter pack.
 */
template <typename T, typename U, typename... Args>
void RequestHandler::processArgs(std::ostringstream &content, bool &first, T key, U value, Args... args) {
    if (!first) { content << ","; }
    first = false;
    content << "\"" << key << "\":" << formatValue(value);
    processArgs(content, first, args...);
}

/**
 * @brief Formats a value into a string representation.
 * 
 * This template function formats a value of type `T` into a `std::string`. It supports integral types 
 * (e.g., `int`, `long`), `const char*`, and `std::string`. For integral types, it converts the value 
 * to its string representation using `std::to_string()`. For `const char*` and `std::string`, it wraps 
 * the value in double quotes. If an unsupported type is provided, a static assertion will be triggered.
 * 
 * @tparam T The type of the value to be formatted.
 * 
 * @param value The value to be formatted.
 * 
 * @return A `std::string` containing the formatted value.
 * 
 * @note If an unsupported data type is passed, a static assertion will be triggered at compile time.
 */
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
