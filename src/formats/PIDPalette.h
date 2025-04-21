#pragma once

#include "../File.h"

#include <SFML/Graphics/Texture.hpp>

class PIDPalette : public File
{
public:
    PIDPalette() = default;
    explicit PIDPalette(const unsigned char* ptr);

    sf::Color getColor(int i, bool treatFirstAsTransparent = true) const;

    bool load(std::istream& stream) override;
    bool save(std::ostream& stream) override;

    const sf::Texture& getTexture();
    unsigned char* getData() { return &data[0][0]; }


private:
    unsigned char data[256][3];
    sf::Texture texture;
    bool requiresTextureUpdate = true;
};
