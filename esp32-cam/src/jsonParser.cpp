#include <cctype>
#include <iostream>
#include <map>
#include <sstream>
#include <string>

class JsonParser {
  public:
    static int parse(const std::string &json, std::map<std::string, std::string> *result) {
        if (!result) return 1; // Null pointer
        result->clear();
        size_t pos = 0;
        while (pos < json.length()) {
            skipWhitespace(json, pos);
            if (json[pos] == '"') {
                std::string key;
                if (parseString(json, pos, &key) != 0) return -2; // Error in parsing
                skipWhitespace(json, pos);

                // Expect a colon after the key
                if (pos >= json.length() || json[pos] != ':') {
                    return 3; // Missing ":" 
                }
                pos++; // Skip the colon
                skipWhitespace(json, pos);

                std::string value;
                if (parseValue(json, pos, &value) != 0) return -4; // Error in parsing
                (*result)[key] = value;
            } else {
                pos++;
            }
        }
        return 0; // Success
    }

  private:
    static void skipWhitespace(const std::string &str, size_t &pos) {
        while (pos < str.length() && std::isspace(str[pos])) {
            pos++;
        }
    }

    static int parseString(const std::string &str, size_t &pos, std::string *result) {
        if (!result) return 1; // Null pointer 
        result->clear();
        if (str[pos] != '"') {
            return 2; // Missing opening '"'
        }
        pos++; // Skip '"'
        std::ostringstream buffer;
        while (pos < str.length() && str[pos] != '"') {
            if (str[pos] == '\\') { // Handle escape sequences
                pos++;
                if (pos < str.length()) {
                    buffer << str[pos];
                } else {
                    return 3; // Incomplete escape sequence
                }
            } else {
                buffer << str[pos];
            }
            pos++;
        }
        if (pos >= str.length() || str[pos] != '"') {
            return 4; // Missing closing '"'
        }
        pos++; // Skip '"'
        *result = buffer.str();
        return 0;
    }

    static int parseValue(const std::string &str, size_t &pos, std::string *result) {
        if (!result) return 1; // Null pointer
        result->clear();
        if (str[pos] == '"') { return parseString(str, pos, result); }
        // Handle other types of values
        std::ostringstream buffer;
        while (pos < str.length() && str[pos] != ',' && str[pos] != '}') {
            if (!std::isspace(str[pos])) { buffer << str[pos]; }
            pos++;
        }
        *result = buffer.str();
        return 0;
    }
};


// TEST
// int main() {
//     std::string json = "{\"key1\": \"value1\", \"key2\": \"value2\", \"key3\": \"value3\"}";
//     std::map<std::string, std::string> parsedJson;
//     int status = JsonParser::parse(json, &parsedJson);

//     if (status == 0) {
//         for (const auto &[key, value] : parsedJson) {
//             std::cout << key << ": " << value << std::endl;
//         }
//     } else {
//         std::cerr << "Parsing failed with error code: " << status << std::endl;
//     }

//     json = R"({"name": "John", "age": "30", "city": "New York"})";
//     status = JsonParser::parse(json, &parsedJson);

//     if (status == 0) {
//         for (const auto &[key, value] : parsedJson) {
//             std::cout << key << ": " << value << std::endl;
//         }
//     } else {
//         std::cerr << "Parsing failed with error code: " << status << std::endl;
//     }

//     return 0;
// }
