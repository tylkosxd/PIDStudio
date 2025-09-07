#pragma once

#include "../File.h"

class PIDPalette;

class PCXFile : public File {
public:
    PCXFile() = default;
    ~PCXFile() { if (destroyData) delete[] data; };
    bool        loadFromFile(const std::filesystem::path& filepath) override;
    bool        load(std::istream& stream) override;
    std::shared_ptr<PIDPalette> getPalette() const { return palette; }
    uint8_t* data;
    size_t dataSize;
    int width, height, offsetX, offsetY;
    std::filesystem::path path;
    void keepData() {destroyData = false;}

private:
    struct header {
        uint8_t magic;
        uint8_t version;
        uint8_t encoding;
        uint8_t bitsPerPixel;
        uint16_t xStart;
        uint16_t yStart;
        uint16_t xEnd;
        uint16_t yEnd;
        uint16_t horzRes;
        uint16_t vertRes;
        uint8_t egaPalette[48];
        uint8_t reserved1;
        uint8_t bitPlanes;
        uint16_t bytesPerLine;
        uint16_t paletteType;
        uint16_t horzScreenRes;
        uint16_t vertScreenRes;
        uint8_t reserved2[54];
    } header;
    std::shared_ptr<PIDPalette> palette;
    bool destroyData = true;
};
