#include "utils.h"
#include <algorithm>
#include <cctype>
#include <sstream>

std::string trim(const std::string& s) {
    auto start = s.begin();
    while (start != s.end() && std::isspace(*start)) ++start;
    auto end = s.end();
    while (end != start && std::isspace(*(end - 1))) --end;
    return {start, end};
}

std::vector<std::string> split(const std::string& s, char delim) {
    std::vector<std::string> tokens;
    std::istringstream ss(s);
    std::string token;
    while (std::getline(ss, token, delim))
        if (!token.empty()) tokens.push_back(token);
    return tokens;
}

std::string format_kb(long kb) {
    if (kb >= 1024 * 1024)
        return std::to_string(kb / 1024 / 1024) + "." +
               std::to_string((kb / 1024 % 1024) * 10 / 1024) + " GB";
    if (kb >= 1024)
        return std::to_string(kb / 1024) + "." +
               std::to_string((kb % 1024) * 10 / 1024) + " MB";
    return std::to_string(kb) + " KB";
}