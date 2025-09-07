#include "Filesystem.h"
#include "String.h"

bool fileExists(const std::filesystem::path& path, std::string filename) {
    std::transform(filename.begin(), filename.end(), filename.begin(), charToLower);
    for (auto const& path : std::filesystem::directory_iterator(path)) {
        std::string name = path.path().filename().string();
        std::transform(name.begin(), name.end(), name.begin(), charToLower);
        if (name == filename) {
            return true;
        }
    }
    return false;
}

std::string getFileExtension(const std::filesystem::path& path) {
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), charToLower);
    return ext;
}

bool isValidFileName(const std::filesystem::path& filename) {
    std::string str = filename.string(); 
    for (char c : str) {
        if (c == ':' || c == '<' || c == '>' || c == '"' || c == '/' || c == '?' || c == '\\' || c == '*' || c == '|')
            return false;
    }
    return true;
}