// (C) 2021 by Folkert van Heusden <mail@vanheusden.com>
// Released under Apache License v2.0

#include <stdarg.h>
#include <string>
#include <unistd.h>

std::string myformat(const char *const fmt, ...)
{
        char *buffer = nullptr;
        va_list ap;

        va_start(ap, fmt);
        (void)vasprintf(&buffer, fmt, ap);
        va_end(ap);

        std::string result = buffer;
        free(buffer);

        return result;
}
