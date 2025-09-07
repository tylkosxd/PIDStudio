#include "PIDFile.h"

#include "PIDPalette.h"
#include "../PIDStudio.h"
#include "../PaletteLUV.h"
#include "PCXFile.h"
#include "BMPFile.h"
#include "PNGFile.h"

#include <string>
#include <cmath>

#define PALETTE_OFFSET -768
#define DATA_OFFSET 32

#define DEFAULT_RLE_THRESHOLD 192
#define RLE2_THRESHOLD 128

bool PIDFile::loadFromFile(const std::filesystem::path& filepath) {
    path = filepath;
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
        stream.seekg(PALETTE_OFFSET, std::ios_base::end);
        ownPalette = std::make_shared<PIDPalette>();
        ownPalette->loadFromStream(stream);
        stream.seekg(DATA_OFFSET);
        palette = ownPalette;
    }

    dataSize = width * height;
    data = new uint8_t[dataSize];
    uint8_t *outPtr = data;
    const uint8_t *endPtr = outPtr + dataSize;

    int length;
    uint8_t currentByte;

    auto outputCurrentByte = [&]() { *outPtr++ = currentByte; };

    auto fillWith = [&](uint8_t byte) {
        memset(outPtr, byte, length);
        outPtr += length;
    };

    auto readAndOutputBytes = [&]() {
        for (int i = 0; i < length; i++) {
            stream > currentByte;
            outputCurrentByte();
        }
    };

    auto readRLE2 = [&]() { // RLE-raw data hybrid encoding
        while (outPtr < endPtr && stream.good()) {
            stream > currentByte;
            if (currentByte > RLE2_THRESHOLD) {
                length = currentByte - RLE2_THRESHOLD;
                fillWith(0);
            } else {
                length = currentByte;
                readAndOutputBytes();
            }
        }
    };

    auto readDefaultRLE = [&]() {
        while (outPtr < endPtr && stream.good()) {
            stream > currentByte;
            if (currentByte > DEFAULT_RLE_THRESHOLD) {
                length = currentByte - DEFAULT_RLE_THRESHOLD;
                stream > currentByte;
                fillWith(currentByte);
            } else {
                outputCurrentByte();
            }
        }
    };

    if (flags & PID_Flag_Compression) {
        readRLE2();
    } else {
        readDefaultRLE();
    }

    return true;
}

bool PIDFile::exportToPNG(const std::filesystem::path& path) {
    if (requiresTextureUpdate) {
        makeImage();
    }
    return image.saveToFile(path.string());
}

bool PIDFile::saveToFile(const std::filesystem::path& path) {
    std::string ext = path.extension().string();
    if (ext == ".png") {
        if (requiresTextureUpdate) {
            makeImage();
        }
        return image.saveToFile(path.string());
    }
    return File::saveToFile(path);
}

bool PIDFile::save(std::ostream &stream) {

    int w_width, w_height, w_offsetX, w_offsetY;
    PID_FLAGS w_flags;
    uint8_t* w_data;
    size_t w_dataSize;

    if (optimize()) {
        w_flags = flags | PID_Flag_Optimized;
        w_width = optWidth;
        w_height = optHeight;
        w_offsetX = optOffsetX;
        w_offsetY = optOffsetY;
        w_data = optData;
        w_dataSize = optDataSize;
    } else {
        w_flags = flags;
        w_width = width;
        w_height = height;
        w_offsetX = offsetX;
        w_offsetY = offsetY;
        w_data = data;
        w_dataSize = dataSize;
    }

    stream < magic < w_flags
    < w_width < w_height < w_offsetX < w_offsetY < userdata;

    uint8_t *outPtr = w_data;
    const uint8_t *endPtr = outPtr + w_dataSize;
    uint8_t *lastSegPtr = outPtr;

    uint8_t singleByte;
    int length = 0;
    int pixelsLineCounter = 0;
    bool isZero;

    auto writeRLE2Segment = [&]() {
        if (isZero) {
            stream < (uint8_t) (outPtr - lastSegPtr + RLE2_THRESHOLD);
        } else {
            stream < (uint8_t) (outPtr - lastSegPtr);
            stream.write((const char*)lastSegPtr, outPtr - lastSegPtr);
        }
    };

    static constexpr uint8_t DEFAULT_RLE_MIN_LENGTH = DEFAULT_RLE_THRESHOLD + 1;

    auto writeDefaultRLESegment = [&]() {
        if (length > 0 || singleByte >= DEFAULT_RLE_THRESHOLD) {
            stream < (uint8_t) (length + DEFAULT_RLE_MIN_LENGTH);
            length = 0;
        }
        stream < singleByte;
    };

    auto writeRLE2 = [&]() {
        if (outPtr >= endPtr) return;

        singleByte = *outPtr;
        isZero = singleByte == 0;

        while (++outPtr < endPtr) {
            length++;
            if (isZero != (*outPtr == 0) || length == w_width) {
                writeRLE2Segment();
                lastSegPtr = outPtr;
                if (isZero != (*outPtr == 0)) 
                    isZero = !isZero;
            } else if (outPtr - lastSegPtr + 1 == RLE2_THRESHOLD) {
                writeRLE2Segment();
                lastSegPtr = outPtr;
            }
            if (length == w_width)
                length = 0;
            singleByte = *outPtr;
        }
        writeRLE2Segment();
    };

    static constexpr uint8_t MAX_LENGTH = 0xFF - DEFAULT_RLE_THRESHOLD - 1;

    auto writeDefaultRLE = [&]() {
        if (outPtr >= endPtr) return;

        singleByte = *outPtr;
        while (++outPtr < endPtr) {
            pixelsLineCounter++;
            if (singleByte == *outPtr && length < MAX_LENGTH && pixelsLineCounter < w_width) {
                length++;
            } else {
                writeDefaultRLESegment();
            }
            singleByte = *outPtr;
            if (pixelsLineCounter == w_width)
                pixelsLineCounter = 0;
        }
        writeDefaultRLESegment();
    };

    if (flags & PID_Flag_Compression) {
        writeRLE2();
    } else {
        writeDefaultRLE();
    }

    if (flags & PID_Flag_OwnPalette) {
        palette -> save(stream);
    }

    return true;
}

bool PIDFile::optimize() {

    if (flags & PID_Flag_Never_Optimize)
        return false;

    if (optimizedJustNow)
        return true;

    if (!shouldOptimize)
        return false;

    if (flags & PID_Flag_Optimized)
        return false;

    if (path.parent_path().parent_path().filename() == "TILES") {
        setFlag(PID_Flag_Never_Optimize, true);
        return false;
    }

    int top = 0;
    int left = 0;
    int bottom = 0;
    int right = 0;

    uint8_t* ptr;
    const uint8_t* endPtr = data + dataSize;
    uint8_t* startPtr = data;
    bool isZero = true;
    int line = 0;

    auto scanColumn = [&](int column) {
        ptr = startPtr + column;
        while (ptr < endPtr) { 
            if (*ptr) {
                isZero = false;
                break;
            }
            ptr += width;
        }
    };

    auto scanRow = [&](int row) {
        ptr = startPtr + row*width;
        uint8_t* endLinePtr = ptr + width;
        while (ptr < endLinePtr) {
            if (*ptr) {
                isZero = false;
                break;
            }
            ptr++;
        }
    };

    auto checkTop = [&]() {
        isZero = true; line = 0;
        while (line < height) {
            scanRow(line);
            if (!isZero)
                break;
            top++;
            line++;
        }
    };

    auto checkLeft = [&]() {
        isZero = true; line = 0;
        while (line < width) {
            scanColumn(line);
            if (!isZero)
                break;
            left++;
            line++;
        }
    };

    auto checkBottom = [&]() {
        isZero = true; line = height - 1;
        while (line >= 0) {
            scanRow(line);
            if (!isZero)
                break;
            bottom++;
            line--;
        }
    };

    auto checkRight = [&]() {
        isZero = true; line = width - 1;
        while (line >= 0) {
            scanColumn(line);
            if (!isZero)
                break;
            right++;
            line--;
        }
    };

    checkTop();
    if (isZero) return false; // the image is empty, do not optimize or resize
    checkLeft();
    checkBottom();
    checkRight();

    optWidth = width - right - left;
    optHeight = height - top - bottom;

    if (optWidth <= 0 || optHeight <= 0)
        return false; // something went really wrong

    // the width needs to be divisible by 4:
    int properWidth = ((optWidth + 3) & ~3);

    int diffForOffsets = (properWidth - optWidth)/2;
    left = left < diffForOffsets ? 0 : left - diffForOffsets;
    right = right < diffForOffsets ? 0 : right - diffForOffsets;

    optWidth = properWidth;

    optOffsetX = (left - right)/2 + offsetX;

    if ((top + bottom) & 1) { // the sum cannot be uneven - this ensures the offsets won't be off by 1px
        if (top > 0) {
            top--;
        } else {
            bottom--;
        }
        optHeight++;
    }

    optOffsetY = (top - bottom)/2 + offsetY;

    if (optWidth == width && optHeight == height) { // the image is already optimized
        setFlag(PID_Flag_Optimized, true);
        return false;
    }

    optDataSize = optWidth * optHeight;
    optData = new uint8_t[optDataSize];
    
    uint8_t* optDataPtr = optData;

    // Most common case
    if (optWidth <= width) {

        for(int y = top; y < top+optHeight; y++) {
            for (int x = left; x < left+optWidth; x++) {
                *optDataPtr = *(startPtr + x + y*width);
                optDataPtr++;
            } 
        }

        optimizedJustNow = true;
        return true;
    }
    // Very rare case - the image after this operation is wider than the original 
    else {
        memset(optDataPtr, 0, optDataSize);

        for(int y = top; y < top+optHeight; y++) {
            optDataPtr = optData + (y-top)*optWidth;
            for (int x = 0; x < width; x++) {
                *optDataPtr = *(startPtr + x + y*width);
                optDataPtr++;
            }
        }

        optimizedJustNow = true;
        return true;
    }
}

void PIDFile::getFromPCX(const std::unique_ptr<PCXFile>& pcxFile) {
    offsetX         = pcxFile->offsetX;
    offsetY         = pcxFile->offsetY;
    originalOffsetX = offsetX;
    originalOffsetY = offsetY;
    flags           = PID_Flag_Transparency;
    originalFlags   = flags;
    width           = pcxFile->width;
    height          = pcxFile->height;
    pcxFile->keepData();
    data            = pcxFile->data;
    dataSize        = pcxFile->dataSize;
    path            = pcxFile->path;
    name            = path.filename().string();
    windowName      = name + "###" + path.string();
    palette         = pcxFile->getPalette();
    ownPalette      = palette;
    isReallyPID     = false;
};

void PIDFile::getFromBMP(const std::unique_ptr<BMPFile>& bmpFile) {
    flags           = PID_Flag_Transparency;
    originalFlags   = flags;
    width           = bmpFile->width;
    height          = bmpFile->height;
    bmpFile->keepData();
    data            = bmpFile->data;
    dataSize        = bmpFile->dataSize;
    path            = bmpFile->path;
    name            = path.filename().string();
    windowName      = name + "###" + path.string();
    palette         = bmpFile->getPalette();
    ownPalette      = palette;
    isReallyPID     = false;
};

bool PIDFile::getFromPNG(const std::unique_ptr<PNGFile>& pngFile) {
    if (!pngFile->isIndexed)
        return false; // first call pngFile->indexToPalette()
    flags           = PID_Flag_Transparency;
    originalFlags   = flags;
    width           = pngFile->width;
    height          = pngFile->height;
    pngFile->keepData();
    data            = pngFile->data;
    dataSize        = pngFile->dataSize;
    path            = pngFile->path;
    name            = path.filename().string();
    windowName      = name + "###" + path.string();
    palette         = pngFile->getPalette();
    ownPalette      = palette;
    isReallyPID     = false;
    return true;
}

void PIDFile::rewriteOriginalData() {
    originalFlags = flags;
    originalOffsetX = offsetX;
    originalOffsetY = offsetY;
    if (originalPalette)
        originalPalette = palette;
    if (originalData) {
        for (int i = 0; i < dataSize; i++) {
            originalData[i] = data[i];
        }
    }
    transformedToPalette = false;
}

void PIDFile::makeImage() {
    const std::shared_ptr<PIDPalette>& imagePalette = 
        (palette && !(flags & PID_Flag_Grayscale)) ? palette : app->defaultPalette;
    sf::Image img;
    
    img.create(width+2*abs(offsetX), height+2*abs(offsetY), sf::Color::Transparent);

    int diffX = abs(offsetX) + offsetX;
    int diffY = abs(offsetY) + offsetY;

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            img.setPixel(x+diffX, y+diffY, imagePalette->getColor(data[y * width + x]));
        }
    }
    image = img;
}

const sf::Texture& PIDFile::getTexture() {
    if (requiresTextureUpdate) {
        makeImage();
        texture.loadFromImage(image);
        requiresTextureUpdate = false;
    }
    return texture;
}

bool PIDFile::isModified() {
    return (
        flags != originalFlags || 
        offsetX != originalOffsetX || 
        offsetY != originalOffsetY ||
        transformedToPalette == true
    );
}

void PIDFile::setFlag(PID_FLAGS flag, bool state) {
    if (state) flags |= flag;
    else flags &= ~flag;
}

void PIDFile::transformToPalette(const std::shared_ptr<PIDPalette>& outPalette) {
    if (!palette || palette == outPalette)
        return;

    /* make a copy of palette's pointer and image data only once */
    if (!originalPalette)
        originalPalette = palette;
    if (!originalData) {
        originalData = new uint8_t[dataSize];
        for (int i = 0; i < dataSize; i++)
            originalData[i] = data[i];
    }

    /* reverse the changes when the palette to transform is the original palette */
    if (outPalette == originalPalette) {
        for (int i = 0; i < dataSize; i++)
            data[i] = originalData[i];
        palette = originalPalette;
        transformedToPalette = false;
        resetTexture();
        return;
    }

    if (app->lastInPalette == palette && app->lastOutPalette == outPalette) {
        for (int index = 0; index < dataSize; index++)
            data[index] = (app->lastColorTable)[originalData[index]];
    } else {
        /* Luv gets much better results than rgb */
        auto inPaletteLuv = std::make_unique<PaletteLUV>(originalPalette->getData());
        auto outPaletteLuv = std::make_unique<PaletteLUV>(outPalette->getData());
        
        for (int index = 0; index < 256; index++)
            (app->lastColorTable)[index] = inPaletteLuv->getMostSimilarColor(index, outPaletteLuv);

        for (int index = 0; index < dataSize; index++)
            data[index] = (app->lastColorTable)[originalData[index]];
        
        app->lastInPalette = palette;
        app->lastOutPalette = outPalette;
    }
    
    palette = outPalette;
    transformedToPalette = true;
    resetTexture();
}

void PIDFile::resetTransformationToPalette() {
    if (transformedToPalette) {
        for (int i = 0; i < dataSize; i++)
            data[i] = originalData[i];
        palette = originalPalette;
        resetTexture();
        transformedToPalette = false;
    }
}