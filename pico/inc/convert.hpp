#pragma once

#include <string>
#include <vector>
#include <sstream>

bool str_to_int(std::string &str, int &result);
bool str_to_vec(const std::string &str, const char delim, std::vector<std::string> &vec);
