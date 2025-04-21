#pragma once

#include <SFML/Graphics/Texture.hpp>

class ImGuiWindow;

namespace ImGui {
    void CenteredImage(const sf::Texture& texture, float offsetX = 0.0f, float offsetY = 10.0f);
    void CenteredImageHorizontal(const sf::Texture& texture, float offsetX = 0.0f);
    bool BringFocusTo(ImGuiWindow* window);
    bool BeginPopupForLastItem(const char* name);
}
