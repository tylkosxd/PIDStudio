#include "ImGuiExtensions.h"

#include <imgui-SFML.h>
#include "imgui_internal.h"

using namespace ImGui;

namespace ImGuiEx {

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

    bool BringFocusTo(ImGuiWindow* window) {
        if (!window || !window->DockNode || !window->DockNode->TabBar)
            return false;

        window->DockNode->TabBar->NextSelectedTabId = window->TabId;
        return true;
    }

    bool BeginPopupForLastItem(const char *name) {
        if (GetCurrentContext()->LastItemData.StatusFlags & ImGuiItemStatusFlags_HoveredRect
            || IsPopupOpen(name)) {
            return BeginPopupContextItem(name);
        }

        return false;
    }

    void CenterNextWindow(float width, float height) {
        ImGuiViewport* viewport = GetMainViewport();
        ImVec2 mainWindowSize = viewport->Size;
        ImVec2 mainWindowPos = viewport->Pos;
        const ImVec2 windowPos(
            (mainWindowSize.x - width) * 0.5f + mainWindowPos.x,
            (mainWindowSize.y - height) * 0.5f + mainWindowPos.y
        );
        SetNextWindowPos(windowPos);
        SetNextWindowSize(ImVec2(width, height));
    }

    void ToolTip(const char* description) {
        if (BeginItemTooltip()) {
            PushTextWrapPos(ImGui::GetFontSize() * 30.0f);
            TextUnformatted(description);
            PopTextWrapPos();
            EndTooltip();
        }
    }

    // copied from imgui_widgets.cpp, changed to accept a fill color.
    void ProgressBar(float fraction, unsigned int fillColor) {
        ImGuiWindow* window = GetCurrentWindow();
        if (window->SkipItems)
            return;

        const ImVec2& size_arg = ImVec2(-FLT_MIN, 0);

        ImGuiContext& g = *GImGui;
        const ImGuiStyle& style = g.Style;

        ImVec2 pos = window->DC.CursorPos;
        ImVec2 size = CalcItemSize(size_arg, CalcItemWidth(), g.FontSize + style.FramePadding.y * 2.0f);
        ImVec2 posEnd(pos.x + size.x, pos.y + size.y);
        ImRect bb(pos, posEnd);
        ItemSize(size, style.FramePadding.y);
        if (!ItemAdd(bb, 0))
            return;

        // Fraction < 0.0f will display an indeterminate progress bar animation
        // The value must be animated along with time, so e.g. passing '-1.0f * GetTime()' as fraction works.
        const bool is_indeterminate = (fraction < 0.0f);
        if (!is_indeterminate)
            fraction = ImSaturate(fraction);

        // Out of courtesy we accept a NaN fraction without crashing
        float fill_n0 = 0.0f;
        float fill_n1 = (fraction == fraction) ? fraction : 0.0f;

        if (is_indeterminate) {
            const float fill_width_n = 0.2f;
            fill_n0 = ImFmod(-fraction, 1.0f) * (1.0f + fill_width_n) - fill_width_n;
            fill_n1 = ImSaturate(fill_n0 + fill_width_n);
            fill_n0 = ImSaturate(fill_n0);
        }

        // Render
        RenderFrame(bb.Min, bb.Max, GetColorU32(ImGuiCol_FrameBg), true, style.FrameRounding);
        bb.Expand(ImVec2(-style.FrameBorderSize, -style.FrameBorderSize));
        RenderRectFilledRangeH(window->DrawList, bb, (ImU32)fillColor, fill_n0, fill_n1, style.FrameRounding);

        // Displaying the fraction as percentage string
        char overlay_buf[32];
        if (!is_indeterminate) {
            ImFormatString(overlay_buf, IM_ARRAYSIZE(overlay_buf), "%.0f%%", fraction * 100 + 0.01f);
            const char* overlay = overlay_buf;

            ImVec2 overlay_size = CalcTextSize(overlay, NULL);
            if (overlay_size.x > 0.0f) {
                float text_x = is_indeterminate ? (bb.Min.x + bb.Max.x - overlay_size.x) * 0.5f : ImLerp(bb.Min.x, bb.Max.x, fill_n1) + style.ItemSpacing.x;
                RenderTextClipped(ImVec2(ImClamp(text_x, bb.Min.x, bb.Max.x - overlay_size.x - style.ItemInnerSpacing.x), bb.Min.y), bb.Max, overlay, NULL, &overlay_size, ImVec2(0.0f, 0.5f), &bb);
            }
        }
    }

}
