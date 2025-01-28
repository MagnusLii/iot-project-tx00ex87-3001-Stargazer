#ifndef JSONPARSER_HPP
#define JSONPARSER_HPP

#include <string>
#include <vector>

int parse_json(const char *buffer, const int buffer_len, const std::vector<std::string> keys_to_look_for,
               std::vector<std::string> &parsed_results);
int parse_json(const char *buffer, std::vector<std::string> &parsed_results);

#endif