#include <string>
#include <vector>
#include <cstring>


int parse_json(const char *buffer, const int buffer_len, std::vector<std::string> &parsed_results) {
    if (buffer == nullptr || buffer_len <= 0) {
        return 1; // Invalid buffer
    }
    std::string tmp_buffer(buffer, buffer_len);

    size_t start = tmp_buffer.find_first_of('{');
    size_t end = tmp_buffer.find_last_of('}');
    
    if (start == std::string::npos || end == std::string::npos) {
        return 2;
    }

    tmp_buffer = tmp_buffer.substr(start, end - start + 1);

    size_t pos = 0;
    bool key = true;
    while (pos < tmp_buffer.length()) {
        size_t next_quote = tmp_buffer.find('\"', pos);
        if (next_quote == std::string::npos) {
            break;
        }

        size_t token_start = next_quote + 1;
        size_t token_end = tmp_buffer.find('\"', token_start);
        if (token_end == std::string::npos) {
            break;
        }

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

int parse_json(const char *buffer, std::vector<std::string> &parsed_results) {
    if (buffer == nullptr) {
        return 1; // Invalid buffer
    }
    std::string tmp_buffer(buffer);

    size_t start = tmp_buffer.find_first_of('{');
    size_t end = tmp_buffer.find_last_of('}');
    
    if (start == std::string::npos || end == std::string::npos) {
        return 2;
    }

    tmp_buffer = tmp_buffer.substr(start, end - start + 1);

    size_t pos = 0;
    bool key = true;
    while (pos < tmp_buffer.length()) {
        size_t next_quote = tmp_buffer.find('\"', pos);
        if (next_quote == std::string::npos) {
            break;
        }

        size_t token_start = next_quote + 1;
        size_t token_end = tmp_buffer.find('\"', token_start);
        if (token_end == std::string::npos) {
            break;
        }

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
