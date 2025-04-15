#pragma once

#include "../File.h"

#include "SFML/Graphics/Texture.hpp"

#include "imgui.h"
#include "imgui-SFML.h"

class PIDStudio;
class PIDPalette;

typedef int PID_FLAGS;

enum PID_FLAGS_ {
    PID_Flag_Transparency   = 1 << 0,
    PID_Flag_VideoMemory    = 1 << 1,
    PID_Flag_SystemMemory   = 1 << 2,
    PID_Flag_Mirror         = 1 << 3,
    PID_Flag_Invert         = 1 << 4,
    PID_Flag_Compression    = 1 << 5,
    PID_Flag_Lights         = 1 << 6,
    PID_Flag_OwnPalette     = 1 << 7
};

class PIDFile : public File {

public:

    explicit PIDFile(PIDStudio* app) : app(app) {};
    ~PIDFile() { delete[] data; }

    bool        loadFromFile(const std::filesystem::path& filepath) override;
    bool        load(std::istream& stream) override;
    bool        saveToFile(const std::filesystem::path& filepath) override;
    bool        save(std::ostream& stream) override;

    const std::string&              getName() const { return name; }
    const std::string&              getWindowName() const { return windowName; }
    const std::filesystem::path&    getPath() const { return path; }

    int          getMagicNumber() const { return magic; }
    int          getWidth() const { return width; }
    int          getHeight() const { return height; }
    int          getOffsetX() const { return offsetX; }
    int          getOffsetY() const { return offsetY; }
    int*         getUserData() { return userdata; }
    PID_FLAGS    getFlags() const { return flags; }

    void setOffsetX(int x) { offsetX = x; }
    void setOffsetY(int y) { offsetY = y; }
    void setFlag(PID_FLAGS flag, bool state);

    sf::Image           makeImage();
    sf::Image           makeImageWithOffsets();
    const sf::Texture&  getTexture();
    void                resetTexture() { requiresTextureUpdate = true; }
    bool                isModified();

    std::shared_ptr<PIDPalette> getPalette() const { return palette; }
    void setPalette(const std::shared_ptr<PIDPalette>& p) { palette = p; requiresTextureUpdate = true; }

private:
    PIDStudio* app;
    int width, height, magic, offsetX, offsetY, userdata[2];
    int originalOffsetX, originalOffsetY;
    PID_FLAGS originalFlags;
    PID_FLAGS flags;
    uint8_t* data = nullptr;
    std::shared_ptr<PIDPalette> palette;
    std::string name;
    std::string windowName;
    std::filesystem::path path;
    sf::Image image;
    sf::Texture texture;
    bool requiresTextureUpdate = true;
};
