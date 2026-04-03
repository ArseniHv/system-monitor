#include "utils.h"
#include <algorithm>
#include <cctype>
#include <sstream>

std::string trim(const std::string& s) {
    size_t start = 0;
    while (start < s.size() && std::isspace((unsigned char)s[start]))
        ++start;
    size_t end = s.size();
    while (end > start && std::isspace((unsigned char)s[end - 1]))
        --end;
    return s.substr(start, end - start);
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
    if (kb >= 1024L * 1024L) {
        long gb_int  = kb / 1024 / 1024;
        long gb_frac = (kb / 1024 % 1024) * 10 / 1024;
        return std::to_string(gb_int) + "." + std::to_string(gb_frac) + " GB";
    }
    if (kb >= 1024) {
        long mb_int  = kb / 1024;
        long mb_frac = (kb % 1024) * 10 / 1024;
        return std::to_string(mb_int) + "." + std::to_string(mb_frac) + " MB";
    }
    return std::to_string(kb) + " KB";
}