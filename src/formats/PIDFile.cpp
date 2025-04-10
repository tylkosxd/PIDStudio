#include "PIDFile.h"

#include "PIDPalette.h"
#include "../PIDStudio.h"

#ifdef DEBUG
#include <iostream>
#endif // DEBUG

bool PIDFile::loadFromFile(const std::filesystem::path& filepath) {
    this->path = filepath;
    name = filepath.filename().string();
    windowName = name + "###" + filepath.string();

    return File::loadFromFile(filepath);
}

bool PIDFile::load(std::istream& stream) {
    stream > magic > flags > width > height > offsetX > offsetY > unknown;

#ifdef DEBUG
    if (magic != 10) {
        std::cout << "Unexpected magic number" << std::endl;
    }
#endif // DEBUG

    if (flags & Flag_OwnPalette) {
        stream.seekg(-768, std::ios_base::end);
        palette = std::make_shared<PIDPalette>();
        palette->loadFromStream(stream);
        stream.seekg(32);
    }

    data = new uint8_t[width * height];
    uint8_t *outPtr = data;
    uint8_t *endPtr = outPtr + width * height;

    int length;
    uint8_t currentByte;

    auto outputCurrentByte = [&]() { *outPtr++ = currentByte; };
    auto fillWithCurrentByte = [&]() {
        memset(outPtr, currentByte, length);
        outPtr += length;
    };
    auto fillWithZeros = [&]() {
        currentByte = 0;
        fillWithCurrentByte();
    };
    auto readAndOutputBytes = [&]() {
        for (int i = 0; i < length; i++) {
            stream > currentByte;
            outputCurrentByte();
        }
    };

    auto readCompressedPixels = [&]() {
        while (outPtr < endPtr && stream.good()) {
            stream > currentByte;
            if (currentByte > 128) {
                length = currentByte - 128;
                fillWithZeros();
            } else {
                length = currentByte;
                readAndOutputBytes();
            }
        }
    };

    auto readUncompressedPixels = [&]() {
        while (outPtr < endPtr && stream.good()) {
            stream > currentByte;
            if (currentByte > 192) {
                length = currentByte - 192;
                stream > currentByte;
                fillWithCurrentByte();
            } else {
                outputCurrentByte();
            }
        }
    };

    if (flags & Flag_Compression) {
        readCompressedPixels();
    } else {
        readUncompressedPixels();
    }

    return true;
}

bool PIDFile::save(std::ostream &stream) {
    stream < magic < flags < width < height < offsetX < offsetY < unknown;

    uint8_t *outPtr = data;
    uint8_t *endPtr = outPtr + width * height;
    uint8_t *lastSegPtr = outPtr;

    uint8_t singleByte;
    int length = 0;
    bool isZero;
    int lengthCounter = 0;

    auto writeCompressedSegment = [&]() {
        if (isZero) {
            stream < (uint8_t) (outPtr - lastSegPtr + 128);
        } else {
            stream < (uint8_t) (outPtr - lastSegPtr);
            stream.write((const char*)lastSegPtr, outPtr - lastSegPtr);
        }
    };

    auto writeUncompressedSegment = [&]() {
        if (length > 0) {
            stream < (uint8_t) (length + 192 + 1);
            length = 0;
        } else if (singleByte > 192) {
            stream < (uint8_t) (192 + 1);
        }
        stream < singleByte;
    };

    auto writeCompressedPixels = [&]() {
        if (outPtr >= endPtr) { return; }

        singleByte = *outPtr;
        isZero = singleByte == 0;

        while (++outPtr < endPtr) {
            lengthCounter++;
            lengthCounter = lengthCounter == width + 1 ? 1 : lengthCounter;
            if (isZero != (*outPtr == 0) || lengthCounter == width) {
                writeCompressedSegment();
                lastSegPtr = outPtr;
                if (isZero != (*outPtr == 0)) { isZero = !isZero; };
            } else if (outPtr - lastSegPtr == 127 || lengthCounter == width) {
                writeCompressedSegment();
                lastSegPtr = outPtr;
            }

            singleByte = *outPtr;
        }
        writeCompressedSegment();
    };

    auto writeUncompressedPixels = [&]() {
        if (outPtr >= endPtr) { return; }

        singleByte = *outPtr;
        while (++outPtr < endPtr) {
            if (singleByte == *outPtr) {
                length++;
            } else {
                writeUncompressedSegment();
            }

            singleByte = *outPtr;
        }
        writeUncompressedSegment();
    };

    if (flags & Flag_Compression) {
        writeCompressedPixels();
    } else {
        writeUncompressedPixels();
    }

    return true;
}

const sf::Texture& PIDFile::getTexture()
{
    if (requiresTextureUpdate) {
        const std::shared_ptr<PIDPalette>& imagePalette = palette ? palette : app->getDefaultPalette();

        sf::Image img;
        img.create(width, height);

        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++)
            {
                img.setPixel(x, y, imagePalette->getColor(data[y * width + x]));
            }
        }

        texture.loadFromImage(img);
        texture.setSmooth(true);

        requiresTextureUpdate = false;
    }
    return texture;
}

std::string PIDFile::getFlagsDescription()
{
    std::string flagsDescription;

    if (flags & Flag_Transparency) flagsDescription += "Flag_Transparency\n";
    if (flags & Flag_VideoMemory) flagsDescription += "Flag_VideoMemory\n";
    if (flags & Flag_SystemMemory) flagsDescription += "Flag_SystemMemory\n";
    if (flags & Flag_Mirror) flagsDescription += "Flag_Mirror\n";
    if (flags & Flag_Invert) flagsDescription += "Flag_Invert\n";
    if (flags & Flag_Compression) flagsDescription += "Flag_Compression\n";
    if (flags & Flag_Lights) flagsDescription += "Flag_Lights\n";
    if (flags & Flag_OwnPalette) flagsDescription += "Flag_OwnPalette\n";

    return flagsDescription;
}
