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
    stream > magic > flags > width > height > offsetX > offsetY > userdata;

    originalFlags = flags;
    originalOffsetX = offsetX;
    originalOffsetY = offsetY;

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

    if (flags & Flag_OwnPalette && !(flags & Flag_Lights)) {
        palette -> load(stream);
    }

    return true;
}

bool PIDFile::save(std::ostream &stream) {
    stream < magic < flags < width < height < offsetX < offsetY < userdata;

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
            if (isZero != (*outPtr == 0) || lengthCounter == width) {
                writeCompressedSegment();
                lastSegPtr = outPtr;
                if (isZero != (*outPtr == 0)) { isZero = !isZero; };
            } else if (outPtr - lastSegPtr == 127 || lengthCounter == width) {
                writeCompressedSegment();
                lastSegPtr = outPtr;
            }
            if (lengthCounter == width) {lengthCounter = 0;};
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

    if (flags & Flag_OwnPalette && !(flags & Flag_Lights)) {
        palette -> save(stream);
    }

    originalFlags = flags;
    originalOffsetX = offsetX;
    originalOffsetY = offsetY;

    return true;
}

const sf::Texture& PIDFile::getTexture() {
    if (requiresTextureUpdate) {
        const std::shared_ptr<PIDPalette>& imagePalette = (palette and not (flags & Flag_Lights)) ? palette : app->getDefaultPalette();

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

int PIDFile::getFlagIntValue(std::string flagName) {
    if (flagName == "Transparency") { return (int)Flag_Transparency;};
    if (flagName == "VideoMemory") { return (int)Flag_VideoMemory;};
    if (flagName == "SystemMemory") { return (int)Flag_SystemMemory;};
    if (flagName == "Mirror") { return (int)Flag_Mirror;};
    if (flagName == "Invert") { return (int)Flag_Invert;};
    if (flagName == "Compression") { return (int)Flag_Compression;};
    if (flagName == "Lights") { return (int)Flag_Lights;};
    if (flagName == "OwnPalette") { return (int)Flag_OwnPalette;};
    return 0;
}

void PIDFile::setFlag(std::string flagName, bool state) {
    if (state) {
        flags = (FLAGS)(flags | (getFlagIntValue(flagName)));
    } else {
        flags = (FLAGS)(flags & ~getFlagIntValue(flagName));
    }
}

bool PIDFile::getFlag(std::string flagName) {
    return (bool)(flags & getFlagIntValue(flagName));
}

bool PIDFile::isModified() {
    return (flags != originalFlags || offsetX != originalOffsetX || offsetY != originalOffsetY);
}