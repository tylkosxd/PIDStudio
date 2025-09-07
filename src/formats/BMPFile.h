#pragma once

#include "../File.h"

class PIDPalette;

class BMPFile : public File {
    enum COMPRESSION {
        NONE,
        RLE8,
        RLE4,
        BITFIELDS,
        JPEG,
        PNG,
        ALPHA_BITFIELDS,
        CMYK,
        CMYKRLE8,
        CMYKRLE4
    };
    public:
        BMPFile() = default;
        ~BMPFile() { if (destroyData) delete[] data; };
        bool        loadFromFile(const std::filesystem::path& filepath) override;
        bool        load(std::istream& stream) override;
        std::shared_ptr<PIDPalette> getPalette() const { return palette; }
        void keepData() {destroyData = false;}

    public:
        uint8_t* data;
        size_t dataSize;
        int width, height;
        std::filesystem::path path;
        
    private:
        struct header {
            uint16_t identifier;
            uint32_t fileSize;
            uint16_t reserved1;
            uint16_t reserved2;
            uint32_t offsetToData;
        } header;
        struct dibHeader {
            uint32_t thisHeaderSize;
            uint32_t width;
            uint32_t height;
            uint16_t planes;
            uint16_t bitsPerPixel;
            COMPRESSION compression;
            uint32_t dataSize;
            int32_t hRes;
            int32_t vRes;
            uint32_t paletteSize;
            int32_t unimportant;
        } dibHeader;

        std::shared_ptr<PIDPalette> palette;
        bool destroyData = true;
};