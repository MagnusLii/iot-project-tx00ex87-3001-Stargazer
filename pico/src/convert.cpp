#include "convert.hpp"

bool str_to_int(std::string &str, int &result) {
    std::stringstream ss(str);
    if (!(ss >> result)) { return false; }
    return true;
}

bool str_to_vec(const std::string &str, const char delim, std::vector<std::string> &vec) {
    std::stringstream ss(str);
    std::string token;
    bool result = false;

    while (std::getline(ss, token, delim)) {
        vec.push_back(token);
    }

    if (!vec.empty()) result = true;

    return result;
}
