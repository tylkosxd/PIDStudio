#pragma once

#include "../File.h"

#include <SFML/Graphics/Image.hpp>

class PIDPalette;
class PaletteLUV;

class PNGFile : public File {
public:
    PNGFile() = default;
    ~PNGFile() { if (destroyData) delete[] data; };

    bool        loadFromFile(const std::filesystem::path& filepath) override;
    bool        indexToPalette(const std::shared_ptr<PIDPalette>& pal, const std::unique_ptr<PaletteLUV>& palLUV);

    std::shared_ptr<PIDPalette> getPalette() const { return palette; }

    void keepData() {destroyData = false;}

public:
    uint8_t* data;
    size_t dataSize;
    int width, height;
    std::filesystem::path path;
    bool isIndexed = false;

private:
    sf::Image image;
    std::shared_ptr<PIDPalette> palette;
    bool destroyData = true;
};