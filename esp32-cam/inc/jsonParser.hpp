#ifndef JSONPARSER_HPP
#define JSONPARSER_HPP

#include <unordered_map>
#include <string>

class JsonParser {
  public:
    static int parse(const std::string &json, std::unordered_map<std::string, std::string> *result);

  private:
    static void skipWhitespace(const std::string &str, size_t &pos);
    static int parseString(const std::string &str, size_t &pos, std::string *result);
    static int parseValue(const std::string &str, size_t &pos, std::string *result);
};

#endif