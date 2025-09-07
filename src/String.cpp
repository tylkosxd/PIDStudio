#include "String.h"

#include <algorithm>
#include <cctype>

bool charEqualsCaseSensitive(char a, char b) {
    return a == b;
}

bool charEqualsCaseInsensitive(char a, char b) {
    return std::tolower(static_cast<unsigned char>(a)) ==
        std::tolower(static_cast<unsigned char>(b));
}

char charToLower(char a) {
    return std::tolower(a);
}

bool stringEquals(std::string_view lhs, std::string_view rhs, bool caseSensitive) {
    return std::ranges::equal(lhs, rhs, caseSensitive ? charEqualsCaseSensitive : charEqualsCaseInsensitive);
}

void copyCharArray(char* dst, std::string src, size_t maxLength) {
    size_t srcLen = src.length();
    size_t len = srcLen < maxLength ? srcLen : maxLength;
    if (len > 0)
        std::strncpy(dst, src.data(), len);
    dst[len] = 0;
}