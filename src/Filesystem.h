#pragma once
#include <filesystem>

std::string getFileExtension(const std::filesystem::path& path);
bool isValidFileName(const std::filesystem::path& filename);
bool fileExists(const std::filesystem::path& path, std::string filename);