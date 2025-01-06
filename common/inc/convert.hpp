#pragma once

#include <string>
#include <vector>
#include <sstream>
#include <iomanip>

bool str_to_int(std::string &str, int &result, bool hex = false);
bool str_to_vec(const std::string &str, const char delim, std::vector<std::string> &vec);
bool num_to_hex_str(int num, std::string &str, int width = 0, bool fill = false, bool uppercase = false);
