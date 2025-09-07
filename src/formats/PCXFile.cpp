#include "PCXFile.h"
#include "PIDPalette.h"
#include "../GUI/Dialogs.h"

#define SUPPORTED_VERSION 0x05
#define MAGIC_NUMBER 0x0A
#define OFFSET_TO_PALETTE -768
#define OFFSET_TO_DATA 128
#define OFFSET_TO_PLANES_NUMBER 65
#define RLE_THRESHOLD 192

bool PCXFile::loadFromFile(const std::filesystem::path& filepath) {
    path = filepath;
    return File::loadFromFile(filepath);
}

bool PCXFile::load(std::istream& stream) {
    stream > header.magic > header.version > header.encoding > header.bitsPerPixel;

    if (header.magic != MAGIC_NUMBER || header.version != SUPPORTED_VERSION || header.bitsPerPixel != 8) {
        UI::errorMessageBox(ERROR_INVALID_PCX);
        return false;
    }

    stream > header.xStart > header.yStart > header.xEnd > header.yEnd;

    stream.seekg(OFFSET_TO_PLANES_NUMBER);
    stream > header.bitPlanes;

    if (header.bitPlanes != 1) {
        UI::errorMessageBox(ERROR_NOT_8_BIT_IMAGE);
        return false;
    }

    stream.seekg(OFFSET_TO_PALETTE, std::ios_base::end);
    palette = std::make_shared<PIDPalette>();
    palette->loadFromStream(stream);

    stream.seekg(OFFSET_TO_DATA);

    offsetX = header.xStart;
    offsetY = header.yStart;
    width = header.xEnd - header.xStart + 1;
    height = header.yEnd - header.yStart + 1;
    dataSize = width * height;
    data = new uint8_t[dataSize];
    uint8_t *outPtr = data;
    uint8_t *endPtr = outPtr + dataSize;

    size_t length;
    uint8_t currentByte;

    while (outPtr < endPtr && stream.good()) {
        stream > currentByte;
        if (currentByte > RLE_THRESHOLD) {
            length = currentByte - RLE_THRESHOLD;
            stream > currentByte;
            if (outPtr + length >= endPtr) {
                length = endPtr - outPtr - 1;
            }
            memset(outPtr, currentByte, length);
            outPtr += length;
        } else {
            *outPtr++ = currentByte;
        }
    }

    return true;
}
