#pragma once

#include <string_view>
#include <string>
#include <filesystem>

bool charEqualsCaseSensitive(char a, char b);
bool charEqualsCaseInsensitive(char a, char b);
char charToLower(char a);
bool stringEquals(std::string_view lhs, std::string_view rhs, bool caseSensitive = true);
void copyCharArray(char* dst, std::string src, size_t maxLength);
