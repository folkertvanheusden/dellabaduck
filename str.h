// (C) 2021 by Folkert van Heusden <mail@vanheusden.com>
// Released under Apache License v2.0

#include <string>

std::string myformat(const char *const fmt, ...);

std::vector<std::string> split(std::string in, std::string splitter);
std::string merge(const std::vector<std::string> & in, const std::string & seperator);

std::string str_tolower(std::string s);
