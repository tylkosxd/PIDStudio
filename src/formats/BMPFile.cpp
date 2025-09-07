#include "BMPFile.h"
#include "PIDPalette.h"
#include "../GUI/Dialogs.h"

#define HEADER_SIZE 14
#define BITMAP_IDENTIFIER 0x4D42
#define MAX_PALETTE_SIZE 256

bool BMPFile::loadFromFile(const std::filesystem::path& filepath) {
    path = filepath;
    return File::loadFromFile(filepath);
}

bool BMPFile::load(std::istream& stream) {
    stream > header.identifier;

    if (header.identifier != BITMAP_IDENTIFIER) {
        UI::errorMessageBox(ERROR_INVALID_BMP);
        return false;
    }

    stream > header.fileSize > header.reserved1 > header.reserved2 > header.offsetToData > 
    dibHeader.thisHeaderSize > dibHeader.width > dibHeader.height > dibHeader.planes > dibHeader.bitsPerPixel;

    if (dibHeader.bitsPerPixel != 8){
        UI::errorMessageBox(ERROR_NOT_8_BIT_IMAGE);
        return false;
    }

    stream > dibHeader.compression > dibHeader.dataSize > dibHeader.hRes > dibHeader.vRes > dibHeader.paletteSize;

    if (dibHeader.paletteSize > MAX_PALETTE_SIZE) {
        UI::errorMessageBox(ERROR_INVALID_BMP);
        return false;
    }

    if (dibHeader.paletteSize == 0)
        dibHeader.paletteSize = MAX_PALETTE_SIZE;

    if (dibHeader.compression > RLE8){
        UI::errorMessageBox(ERROR_INVALID_BMP);
        return false;
    }

    stream.seekg(HEADER_SIZE + dibHeader.thisHeaderSize);
    { /* load palette */
        unsigned char paletteRaw[256][3] = {0};
        unsigned char bitmapColor[4];
        for (int entry = 0; entry < dibHeader.paletteSize; entry++) {
            stream > bitmapColor;
            /* a palette entry in bmp is 4 bytes in reverse order (blue-green-red-0) */
            paletteRaw[entry][0] = bitmapColor[2];
            paletteRaw[entry][1] = bitmapColor[1];
            paletteRaw[entry][2] = bitmapColor[0];
        }
        palette = std::make_shared<PIDPalette>((const unsigned char*)paletteRaw);
    }

    stream.seekg(header.offsetToData);

    width = dibHeader.width;
    height = dibHeader.height;
    dataSize = width * height;
    data = new uint8_t[dataSize];

    int column = 0;
    int dataOffset = 2*width;

    uint8_t* outPtr = data + ((height-1)*width);
    uint8_t* beginPtr = data;
    uint8_t currentByte;
    uint8_t length = 0;
    unsigned int stride = (width + 3) & ~3; // rounding up to a multiple of 4 (DWORD padding)

    bool success = true;

    switch (dibHeader.compression) {
    case NONE:
        while (stream.good() && outPtr >= beginPtr) {
            stream > currentByte;
            if (column < width) {
                *outPtr++ = currentByte;
            }
            if (++column == stride) {
                column = 0;
                outPtr -= dataOffset;
            }
        };
        break;
    case RLE8:
        while (outPtr >= beginPtr && stream.good()) {
            stream > currentByte;
            if (currentByte == 0) {
                stream > length;
                if (length == 0) { /* special - end of line */
                    outPtr -= dataOffset;
                } else if (length == 1) { /* special - end of bitmap*/
                    break;
                } else if (length == 2) { /* special - offseting */
                    uint8_t offsetX, offsetY;
                    stream > offsetX > offsetY;
                    outPtr += (offsetX - offsetY*width);
                } else if (length) {
                    for (int i = 0; i < length; i++) {
                        stream > currentByte;
                        *outPtr++ = currentByte;
                    }
                    if (length & 1 == 1) { /* WORD padding */
                        stream > currentByte;
                    }
                }
            } else {
                length = currentByte;
                stream > currentByte;
                memset(outPtr, currentByte, length);
                outPtr += length;
            }
        };
        break;
    default:
        success = false;
        break;
    }

    return success;
}