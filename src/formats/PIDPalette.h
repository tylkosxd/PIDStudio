#pragma once

#include "../File.h"

#include <SFML/Graphics/Texture.hpp>

class PIDPalette : public File
{
public:
    PIDPalette() = default;
    explicit PIDPalette(const unsigned char* ptr);

    sf::Color getColor(int i, bool treatFirstAsTransparent = true) const;
    void setColor(int i, uint8_t r, uint8_t g, uint8_t b);

    bool loadFromFile(const std::filesystem::path& filepath) override;
    bool load(std::istream& stream) override;
    bool save(std::ostream& stream) override;

    const sf::Texture& getTexture();
    unsigned char* getData() { return &data[0][0]; }

    void setName(const std::string&);
    std::string getName() { return name; }

    std::filesystem::path getPath() { return path; }
    void setPath(const std::filesystem::path& filepath) { path = filepath; }

private:
    unsigned char data[256][3];
    sf::Texture texture;
    bool requiresTextureUpdate = true;
    std::string name;
    std::filesystem::path path;
};
