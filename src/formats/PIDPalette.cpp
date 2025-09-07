#include "PIDPalette.h"

#include <algorithm>

PIDPalette::PIDPalette(const unsigned char* ptr) {
    std::copy(ptr, ptr+sizeof(data), (unsigned char*)data);
}

bool PIDPalette::loadFromFile(const std::filesystem::path& filepath) {
    path = filepath;
    return File::loadFromFile(filepath);
}

bool PIDPalette::load(std::istream& stream) {
    stream > data;
    return true;
}

bool PIDPalette::save(std::ostream &stream) {
    stream < data;
    return true;
}

const sf::Texture& PIDPalette::getTexture() {
    if (requiresTextureUpdate) {
        sf::Image img;
        img.create(97, 97);
        for (int y = 0; y < 16; y++) {
            for (int x = 0; x < 16; x++) {
                for (int i = 0; i < 5; i++)
                    for (int j = 0; j < 5; j++)
                        img.setPixel(x * 6 + i + 1, y * 6 + j + 1, getColor(y * 16 + x, false));
            }
        }
        texture.loadFromImage(img);
        texture.setSmooth(true);

        requiresTextureUpdate = false;
    }
    return texture;
}

sf::Color PIDPalette::getColor(int i, bool treatFirstAsTransparent) const {
    if (i < 0 || i > 255)
        return sf::Color::Transparent;
    if (i || !treatFirstAsTransparent)
        return {data[i][0], data[i][1], data[i][2]};
    else 
        return sf::Color::Transparent;
}

void PIDPalette::setColor(int i, uint8_t r, uint8_t g, uint8_t b) {
    if (i < 0 || i > 255)
        return;
    data[i][0] = r;
    data[i][1] = g;
    data[i][2] = b;
}

void PIDPalette::setName(const std::string& n) {
    name = n.length() < 128 ? n : n.substr(0, 127);
}
