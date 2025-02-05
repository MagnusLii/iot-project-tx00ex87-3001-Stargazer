#pragma once

#if !defined(ENABLE_DEBUG) && !defined(ENABLE_ESP_DEBUG)
#define DEBUG(...) ((void)0)
#endif
#ifdef ENABLE_DEBUG
#include <iostream>

namespace dbg {
template <typename... Args> void print(const char *file, int line, Args... args) {
    std::clog << "(DEBUG) [" << file << ":" << line << "] ";
    ((std::clog << args << " "), ...);
    std::clog << std::endl;
}
} // namespace dbg

#define DEBUG(...) dbg::print(__FILE__, __LINE__, __VA_ARGS__)
#endif
#ifdef ENABLE_ESP_DEBUG
#include "esp_log.h"
#include <type_traits>

namespace dbg {
template <typename... Args>
void print(const char *file, int line, Args... args) {
    char buffer[512];
    int offset = snprintf(buffer, sizeof(buffer), "[%s:%d] ", file, line);
    if (offset < 0 || offset >= sizeof(buffer)) return; // sprintf errors 

    auto append_to_buffer = [&buffer, &offset](auto arg) {
        if (offset < sizeof(buffer)) {
            if constexpr (std::is_integral_v<decltype(arg)>) {
                // Check if the argument is long
                if constexpr (std::is_same_v<decltype(arg), long int>) {
                    // Long integers
                    int written = snprintf(buffer + offset, sizeof(buffer) - offset, "%ld ", arg);
                    offset += (written > 0) ? written : 0;
                } else {
                    // Regular integers
                    int written = snprintf(buffer + offset, sizeof(buffer) - offset, "%d ", arg);
                    offset += (written > 0) ? written : 0;
                }
            } else if constexpr (std::is_floating_point_v<decltype(arg)>) {
                // Floats
                int written = snprintf(buffer + offset, sizeof(buffer) - offset, "%.2f ", arg);
                offset += (written > 0) ? written : 0;
            } else if constexpr (std::is_same_v<decltype(arg), const char*> || std::is_same_v<decltype(arg), char*>) {
                // Strings
                int written = snprintf(buffer + offset, sizeof(buffer) - offset, "%s ", arg);
                offset += (written > 0) ? written : 0;
            } else {
                // Pointers or unsupported types
                int written = snprintf(buffer + offset, sizeof(buffer) - offset, "<unsupported> ");
                offset += (written > 0) ? written : 0;
            }
        }
    };

    (append_to_buffer(args), ...);

    // Null terminate.
    if (offset < sizeof(buffer)) {
        buffer[offset] = '\0';
    } else {
        buffer[sizeof(buffer) - 1] = '\0';
    }

    ESP_LOGI("(DEBUG)", "%s", buffer);
}
} // namespace dbg

#define DEBUG(...) dbg::print(__FILE__, __LINE__, __VA_ARGS__)
#endif