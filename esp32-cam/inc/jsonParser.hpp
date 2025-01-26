#ifndef JSONPARSER_HPP
#define JSONPARSER_HPP

#include <vector>
#include <string>

int parse_json(const char *buffer, const int buffer_len, std::vector<std::string> &parsed_results);
int parse_json(const char *buffer, std::vector<std::string> &parsed_results);

#endif