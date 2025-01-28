// #include "debug.hpp"
#include <cstring>
#include <string>
#include <vector>

int parse_json(const char *buffer, const int buffer_len, const std::vector<std::string> keys_to_look_for,
               std::vector<std::string> &parsed_results) {
    if (buffer == nullptr || buffer_len <= 0) {
        // DEBUG("Invalid buffer, buffer_len: ", buffer_len);
        return 1; // Invalid buffer
    }
    std::string tmp_buffer(buffer, buffer_len);
    // DEBUG("Buffer: ", tmp_buffer.c_str());

    size_t start = tmp_buffer.find_first_of('{');
    size_t end = tmp_buffer.find_last_of('}');

    if (start == std::string::npos || end == std::string::npos) { return 2; }

    tmp_buffer = tmp_buffer.substr(start, end - start + 1);
    // DEBUG("tmp_buffer: ", tmp_buffer.c_str());

    size_t pos = 0;
    while (pos < tmp_buffer.length()) {
        size_t next_quote = tmp_buffer.find('\"', pos);
        if (next_quote == std::string::npos) { break; }

        size_t token_start = next_quote + 1;
        size_t token_end = tmp_buffer.find('\"', token_start);
        if (token_end == std::string::npos) { break; }

        std::string token = tmp_buffer.substr(token_start, token_end - token_start);

        size_t val_start;
        size_t val_end;
        int counter = 0;

        for (auto key : keys_to_look_for) {
            if (token == key) {
                val_start = tmp_buffer.find(":", token_end) + 1;
                val_end = tmp_buffer.find(',', val_start);
                printf("val_end: %d\n", val_end);
                parsed_results.push_back(tmp_buffer.substr(val_start + 1, val_end - val_start - 1));
                if (val_end == std::string::npos){
                    printf("npos\n");
                    parsed_results[counter].pop_back();
                }

                
                break;
            }
            counter++;
        }

        pos = token_end + 1;
    }

    if (parsed_results.empty()) {
        return 3; // No tokens found
    }

    for (auto result : parsed_results) {
        result.erase('\"');
        printf("Result: %s\n", result.c_str());
    }

    return 0;
}

int parse_json(const char *buffer, std::vector<std::string> &parsed_results) {
    if (buffer == nullptr) {
        return 1; // Invalid buffer
    }
    std::string tmp_buffer(buffer);

    size_t start = tmp_buffer.find_first_of('{');
    size_t end = tmp_buffer.find_last_of('}');

    if (start == std::string::npos || end == std::string::npos) { return 2; }

    tmp_buffer = tmp_buffer.substr(start, end - start + 1);

    size_t pos = 0;
    bool key = true;
    while (pos < tmp_buffer.length()) {
        size_t next_quote = tmp_buffer.find('\"', pos);
        if (next_quote == std::string::npos) { break; }

        size_t token_start = next_quote + 1;
        size_t token_end = tmp_buffer.find('\"', token_start);
        if (token_end == std::string::npos) { break; }

        std::string token = tmp_buffer.substr(token_start, token_end - token_start);

        if (key) {
            key = false;
        } else {
            parsed_results.push_back(token);
            key = true;
        }

        pos = token_end + 1;
    }

    if (parsed_results.empty()) {
        return 3; // No tokens found
    }

    return 0;
}

int main() {
    std::vector<std::string> keys = {"key1", "key2", "key3"};
    std::vector<std::string> results;

    const char *buffer = "{\"key1\": \"value1\", \"key2\": \"value2\", \"key3\": \"value3\"}";
    const int buffer_len = strlen(buffer);

    int ret = parse_json(buffer, buffer_len, keys, results);

    return 0;
}