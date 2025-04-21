#include "ImGuiExtensions.h"

#include <imgui-SFML.h>
#include "imgui_internal.h"

namespace ImGui {
    void CenteredImage(const sf::Texture& texture, float offsetX, float offsetY) {
        ImVec2 windowSize = GetWindowSize();
        sf::Vector2u imageSize = texture.getSize();
        const ImVec2 pos(
            (windowSize.x - (float)imageSize.x) * 0.5f + offsetX,
            (windowSize.y - (float)imageSize.y) * 0.5f + offsetY
        );
        SetCursorPos(pos);
        Image(texture);
    }

    void CenteredImageHorizontal(const sf::Texture& texture, float offsetX) {
        sf::Vector2u imageSize = texture.getSize();
        const ImVec2 pos(
            (GetWindowWidth() - (float)imageSize.x) * 0.5f + offsetX, 
            (float)imageSize.y * 0.5f - 20.0f
        );
        SetCursorPos(pos);
        Image(texture);
    }

    bool BringFocusTo(ImGuiWindow* window)
    {
        if (!window || !window->DockNode || !window->DockNode->TabBar)
            return false;

        window->DockNode->TabBar->NextSelectedTabId = window->TabId;
        return true;
    }

    bool BeginPopupForLastItem(const char *name) {
        if (ImGui::GetCurrentContext()->LastItemData.StatusFlags & ImGuiItemStatusFlags_HoveredRect
            || ImGui::IsPopupOpen(name)) {
            return ImGui::BeginPopupContextItem(name);
        }

        return false;
    }
}
