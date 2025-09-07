#pragma once

#include <SFML/Graphics/Texture.hpp>

class ImGuiWindow;

namespace ImGuiEx {

    void CenteredImage(const sf::Texture& texture, float offsetX = 0.0f, float offsetY = 10.0f);
    void CenteredImageHorizontal(const sf::Texture& texture, float offsetX = 0.0f);
    bool BringFocusTo(ImGuiWindow* window);
    bool BeginPopupForLastItem(const char* name);
    void CenterNextWindow(float width, float height);
    void ToolTip(const char* description);
    void ProgressBar(float fraction, unsigned int fillColor);
}
