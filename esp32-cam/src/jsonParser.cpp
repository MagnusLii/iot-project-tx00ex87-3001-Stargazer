#include "jsonParser.hpp"
#include <cctype>
#include <iostream>
#include <map>
#include <sstream>
#include <string>

/**
 * @brief Parses a JSON string into a key-value map.
 *
 * This function parses a JSON string into an unordered map where each key-value pair 
 * from the JSON object is extracted and stored. It handles various error cases, such as 
 * null pointers, empty strings, and malformed JSON. It expects a standard JSON format 
 * with key-value pairs enclosed in curly braces.
 *
 * @param json The JSON string to parse.
 * @param result A pointer to an unordered map where the parsed key-value pairs will be stored.
 * 
 * @return int Returns:
 * @return        - `0` on successful parsing.
 * @return        - `1` if the result pointer is null.
 * @return        - `2` if there is an error in parsing a string.
 * @return        - `3` if a colon (`:`) is missing after a key.
 * @return        - `4` if there is an error in parsing a value.
 * @return        - `5` if the JSON is empty (i.e., "{}").
 * @return        - `6` if the JSON string is empty.
 * 
 * @note This function modifies the `result` map with the parsed key-value pairs.
 */
int JsonParser::parse(const std::string &json, std::unordered_map<std::string, std::string> *result) {
    if (!result) return 1; // Null pointer
    result->clear();

    // Check for empty str
    if (json.empty()) {
        return 6; // Empty str
    }

    size_t pos = 0;
    skipWhitespace(json, pos);

    // Check for empty JSON
    if (pos < json.length() && json[pos] == '{') {
        pos++;
        skipWhitespace(json, pos);
        if (pos < json.length() && json[pos] == '}') {
            return 5; // Empty JSON
        }
    }

    while (pos < json.length()) {
        skipWhitespace(json, pos);
        if (pos >= json.length()) break;
        if (json[pos] == '"') {
            std::string key;
            if (parseString(json, pos, &key) != 0) return 2; // Error in parsing
            skipWhitespace(json, pos);

            // Expect ":" after the key
            if (pos >= json.length() || json[pos] != ':') {
                return 3; // Missing ":"
            }
            pos++; // Skip ":"
            skipWhitespace(json, pos);

            std::string value;
            if (parseValue(json, pos, &value) != 0) return 4; // Error in parsing
            (*result)[key] = value;
        } else {
            pos++;
        }
    }
    return 0; // Success
}

/**
 * @brief Skips whitespace characters in a string.
 *
 * This function advances the given position in the string by skipping any whitespace 
 * characters (spaces, tabs, newlines) until a non-whitespace character is found or 
 * the end of the string is reached. It is used to help parse JSON or other text formats 
 * by ignoring irrelevant spaces.
 *
 * @param str The string to scan for whitespace characters.
 * @param pos A reference to the current position in the string, which is updated 
 *            to skip over any whitespace characters.
 * 
 * @note The function modifies the `pos` parameter to reflect the new position after 
 *       skipping the whitespace.
 */
void JsonParser::skipWhitespace(const std::string &str, size_t &pos) {
    while (pos < str.length() && std::isspace(str[pos])) {
        pos++;
    }
}

/**
 * @brief Parses a string enclosed in double quotes from a given string.
 *
 * This function extracts a string from the input string, handling escape sequences (e.g., `\"` for a quote 
 * inside a string) and ensuring the string is properly enclosed in double quotes. The extracted string 
 * is stored in the `result` parameter.
 *
 * @param str The string to parse.
 * @param pos A reference to the current position in the string, which is updated as the function processes 
 *            the string. The position is set to the position after the closing quote.
 * @param result A pointer to the string where the parsed result will be stored.
 * 
 * @return int Returns:
 * @return    - `0` if the string is successfully parsed.
 * @return    - `1` if the result pointer is null.
 * @return    - `2` if the opening quote (`"`) is missing.
 * @return    - `3` if an incomplete escape sequence is found.
 * @return    - `4` if the closing quote (`"`) is missing.
 * 
 * @note The function handles escape sequences (such as `\"` or `\\`) and stores the parsed string in the 
 *       `result` parameter. The `pos` parameter is updated to point to the character after the closing quote.
 */
int JsonParser::parseString(const std::string &str, size_t &pos, std::string *result) {
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

/**
 * @brief Parses a value from a JSON string.
 *
 * This function handles the parsing of a JSON value, which can be a string, number, or other valid data types.
 * If the value is a string (indicated by the leading `"`), the function delegates to the `parseString` function.
 * Otherwise, it collects characters until it encounters a delimiter (`,` or `}`) and stores the parsed value 
 * in the `result` parameter.
 *
 * @param str The string to parse.
 * @param pos A reference to the current position in the string, which is updated as the function processes 
 *            the string.
 * @param result A pointer to the string where the parsed result will be stored.
 * 
 * @return int Returns:
 * @return    - `0` if the value is successfully parsed.
 * @return    - `1` if the result pointer is null.
 * 
 * @note The function can parse both strings (using `parseString`) and other value types (such as numbers). 
 *       The `pos` parameter is updated to point to the character after the parsed value.
 */
int JsonParser::parseValue(const std::string &str, size_t &pos, std::string *result) {
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
