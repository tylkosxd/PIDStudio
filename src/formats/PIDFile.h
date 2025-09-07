#pragma once

#include "../File.h"

#include "SFML/Graphics/Texture.hpp"

class PIDStudio;
class PIDPalette;
class PCXFile;
class BMPFile;
class PNGFile;

typedef int PID_FLAGS;

enum PID_FLAGS_ {
    PID_Flag_Transparency   = 1 << 0,
    PID_Flag_VideoMemory    = 1 << 1,
    PID_Flag_SystemMemory   = 1 << 2,
    PID_Flag_Mirror         = 1 << 3,
    PID_Flag_Invert         = 1 << 4,
    PID_Flag_Compression    = 1 << 5,
    PID_Flag_Grayscale      = 1 << 6,
    PID_Flag_OwnPalette     = 1 << 7,
    PID_Flag_Reserved1      = 1 << 8,
    PID_Flag_Reserved2      = 1 << 9,
    // introduced by PIDStudio: 
    PID_Flag_Optimized      = 1 << 10,
    PID_Flag_Never_Optimize = 1 << 11
};

class PIDFile : public File {

public:
    explicit PIDFile(PIDStudio* app) : app(app) {};
    ~PIDFile() {
        delete[] data;
        if (originalData)
            delete[] originalData;
        if (optimizedJustNow)
            delete[] optData;
    }

    bool        loadFromFile(const std::filesystem::path& filepath) override;
    bool        load(std::istream& stream) override;

    bool        exportToPNG(const std::filesystem::path& path);

    bool        saveToFile(const std::filesystem::path& path) override;
    bool        save(std::ostream& stream) override;

    const std::string&              getName() const { return name; }
    const std::string&              getWindowName() const { return windowName; }
    const std::filesystem::path&    getPath() const { return path; }

    void        getFromPCX(const std::unique_ptr<PCXFile>& pcxFile);
    void        getFromBMP(const std::unique_ptr<BMPFile>& bmpFile);
    bool        getFromPNG(const std::unique_ptr<PNGFile>& pngFile);

    PID_FLAGS   getFlags() const { return flags; }
    void        restoreOriginalFlags() { flags = originalFlags; }
    void        restoreOriginalOffsets() { offsetX = originalOffsetX; offsetY = originalOffsetY; };

    void        setFlag(PID_FLAGS flag, bool state);

    const sf::Texture&  getTexture();
    void                resetTexture() { requiresTextureUpdate = true; }
    bool                isModified();
    void                rewriteOriginalData();
    bool                isTransformedToPalette() { return transformedToPalette;}

    std::shared_ptr<PIDPalette> getPalette() const { return palette; }
    std::shared_ptr<PIDPalette> getOwnPalette() const { return ownPalette; }
    void                        setPalette(const std::shared_ptr<PIDPalette>& p) { palette = p; requiresTextureUpdate = true; }
    void                        transformToPalette(const std::shared_ptr<PIDPalette>& outPalette);
    void                        resetTransformationToPalette();

    bool isPID() { return isReallyPID; }

public:
    int         width;
    int         height;
    int         offsetX = 0;
    int         offsetY = 0;
    int         userdata[2] = {0,0};
    PID_FLAGS   flags;
    bool        shouldOptimize = true;
    bool        justOpened = true;

private:
    PIDStudio*              app;
    std::string             name;
    std::string             windowName;
    std::filesystem::path   path;

    int         magic = 0x0A;
    int         originalOffsetX = 0;
    int         originalOffsetY = 0;
    PID_FLAGS   originalFlags;
    size_t      dataSize;
    uint8_t*    data = nullptr;
    uint8_t*    originalData = nullptr;
    sf::Image   image;
    sf::Texture texture;
    bool        requiresTextureUpdate = true;
    bool        transformedToPalette = false;
    bool        isReallyPID = true;

    std::shared_ptr<PIDPalette> palette;
    std::shared_ptr<PIDPalette> ownPalette;
    std::shared_ptr<PIDPalette> originalPalette;

    bool        optimizedJustNow = false;
    uint8_t*    optData;
    size_t      optDataSize;
    int         optWidth;
    int         optHeight;
    int         optOffsetX;
    int         optOffsetY;

private:
    void makeImage();
    bool optimize();
};
