#include "PIDFile.h"

#include "PIDPalette.h"
#include "../PIDStudio.h"

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

    if (flags & PID_Flag_OwnPalette) {
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

    if (flags & PID_Flag_Compression) {
        readCompressedPixels();
    } else {
        readUncompressedPixels();
    }

    return true;
}

bool PIDFile::saveToFile(const std::filesystem::path& filepath) {
    std::filesystem::path extension = filepath.extension();
    if (extension == ".png") {
        return makeImageWithOffsets().saveToFile(filepath.string());
    } else {
        return File::saveToFile(filepath);
    }
    return false;
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

    if (flags & PID_Flag_Compression) {
        writeCompressedPixels();
    } else {
        writeUncompressedPixels();
    }

    if (flags & PID_Flag_OwnPalette && !(flags & PID_Flag_Lights)) {
        palette -> save(stream);
    }

    originalFlags = flags;
    originalOffsetX = offsetX;
    originalOffsetY = offsetY;

    return true;
}

sf::Image PIDFile::makeImage() {
    const std::shared_ptr<PIDPalette>& imagePalette = 
        (palette && !(flags & PID_Flag_Lights)) ? palette : app->getDefaultPalette();
    sf::Image img;
    
    img.create(width, height);
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            img.setPixel(x, y, imagePalette->getColor(data[y * width + x]));
        }
    }
    return img;
}

const sf::Texture& PIDFile::getTexture() {
    if (requiresTextureUpdate) {
        image = makeImage();
        texture.loadFromImage(image);
        texture.setSmooth(true);
        requiresTextureUpdate = false;
    }
    return texture;
}

bool PIDFile::isModified() {
    return (
        flags != originalFlags || 
        offsetX != originalOffsetX || 
        offsetY != originalOffsetY
    );
}

void PIDFile::setFlag(PID_FLAGS flag, bool state) {
    if (state) {
        flags |= flag;
    } else {
        flags &= ~flag;
    }
}

sf::Image PIDFile::makeImageWithOffsets() {
    if (requiresTextureUpdate) image = makeImage();

    int absOffsetX = abs(offsetX); int absOffsetY = abs(offsetY);
    /* resizing for offsets 0, 1 or -1 can be ommited*/
    if (absOffsetX < 2 && absOffsetY < 2) { return image; }

    sf::Image newImage;
    newImage.create(width + 2*absOffsetX, height + 2*absOffsetY);
    newImage.createMaskFromColor(sf::Color::Black);
    newImage.copy(image, absOffsetX + offsetX, absOffsetY + offsetY);

    return newImage;
}