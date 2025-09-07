#include "PNGFile.h"
#include "PIDPalette.h"
#include "../PaletteLUV.h"

bool PNGFile::loadFromFile(const std::filesystem::path& filepath) {
    path = filepath;
    return image.loadFromFile(filepath.string());
}

bool PNGFile::indexToPalette(const std::shared_ptr<PIDPalette>& pal, const std::unique_ptr<PaletteLUV>& palLUV) {
    sf::Vector2u size = image.getSize();
    if (size.x <= 0 || size.y <= 0)
        return false;

    palette = pal;
    width = size.x;
    height = size.y;
    dataSize = size.x*size.y;
    data = new uint8_t[dataSize];
    int lastColor = 0;
    uint8_t lastPalColor = 0;
    int currentColor;

    // very slow code, but yields great results, TODO: utilize GPU to calculate the distances and/or write some faster algorithm without sacrificing the quality.
    for (size_t y = 0; y < size.y; y++) {
        for (size_t x = 0; x < size.x; x++) {
            sf::Color color = image.getPixel(x, y);
            size_t index = x + y*width;

            // case 1: simply set as transparent (index 0) for opacity < 50%
            if (color.a < 128) {
                data[index] = 0;
                continue;
            }
            
            currentColor = (color.r << 6) + (color.g << 4) + (color.b << 2) + color.a;
            
            // case 2: color is the same as previous 
            if (currentColor == lastColor) {
                data[index] = lastPalColor;
                continue;
            }
            // case 3: get the color based on distance in the LUV color space
            lastPalColor = palLUV->getMostSimilarColor(color.r, color.g, color.b);
            data[index] = lastPalColor;
            lastColor = currentColor;
        }
    }

    isIndexed = true;
    return true;
};