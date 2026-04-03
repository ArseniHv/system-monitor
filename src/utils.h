#pragma once
#include <string>
#include <vector>

// Trim leading/trailing whitespace.
std::string trim(const std::string& s);

// Split a string by a delimiter.
std::vector<std::string> split(const std::string& s, char delim);

// Format a number of kilobytes as "X.X GB" / "X.X MB" etc.
std::string format_kb(long kb);