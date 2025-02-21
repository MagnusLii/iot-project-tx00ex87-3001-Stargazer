#include <cstring>
#include <string>

int parseHttpReturnCode(const char *responseString) {
    if (responseString == nullptr) {
        // DEBUG("Error: response is null");
        return -1;
    }

    const char *status_line_end = strstr(responseString, "\r\n");
    if (status_line_end == nullptr) {
        // DEBUG("Error: Could not find end of status line");
        return -1;
    }

    std::string status_line(responseString, status_line_end - responseString);
    size_t code_start = status_line.find(' ') + 1;
    size_t code_end = status_line.find(' ', code_start);

    if (code_start == std::string::npos || code_end == std::string::npos) {
        // DEBUG("Error: Could not parse return code from status line");
        return -1;
    }

    std::string code_str = status_line.substr(code_start, code_end - code_start);
    int return_code = std::stoi(code_str);

    // DEBUG("Parsed HTTP return code: ", return_code);
    return return_code;
}

int main() {
    const char response1[] = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: 13\r\n\r\nHello, World!";
    const char response2[] = "HTTP/1.1 404 Not Found\r\nContent-Type: text/plain\r\nContent-Length: 9\r\n\r\nNot Found";
    const char response3[] = "HTTP/1.1 500 Internal Server Error\r\nContent-Type: application/json\r\nContent-Length: "
                             "30\r\n\r\n{\"error\": \"server malfunction\"}";
    const char response4[] = "HTTP/1.1 301 Moved Permanently\r\nLocation: https://example.com/\r\n\r\n";
    const char response5[] =
        "HTTP/1.1 403 Forbidden\r\nContent-Type: text/plain\r\nContent-Length: 10\r\n\r\nForbidden";

    int code1 = parseHttpReturnCode(response1);
    int code2 = parseHttpReturnCode(response2);
    int code3 = parseHttpReturnCode(response3);
    int code4 = parseHttpReturnCode(response4);
    int code5 = parseHttpReturnCode(response5);

    // Print the parsed HTTP return codes
    printf("Response 1 code: %d\n", code1);
    printf("Response 2 code: %d\n", code2);
    printf("Response 3 code: %d\n", code3);
    printf("Response 4 code: %d\n", code4);
    printf("Response 5 code: %d\n", code5);

    return 0;
}