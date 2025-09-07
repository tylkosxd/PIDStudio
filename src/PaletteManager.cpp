#include "PaletteManager.h"
#include "PIDStudio.h"
#include "formats/PIDPalette.h"
#include "String.h"
#include "gui/Dialogs.h"
#include "ImGuiExtensions.h"

#include <libintl.h>
#include <filesystem>
#include <fmt/core.h>
#include <cstdlib>
#include <imgui_internal.h>

#define _(String) gettext(String)

enum POPUP_STATE {
    KEEP_CLOSED,
    OPEN,
    KEEP_OPENED,
    CLOSE
};

const float MGR_WIDTH = 500.0f, MGR_HEIGHT = 800.0f;
const int COLOR_TABLE_WIDTH = 16;

POPUP_STATE l_mgrPopupState = KEEP_CLOSED;

std::shared_ptr<PIDPalette> l_palette;
ImVec4 l_colorMatrix[256];

ImVec4 l_currentColor;
int l_currentColorIndex = 0;

char l_paletteComboBuffer[128] = {0};

void makePaletteMatrix() {
    sf::Color color;
    if (!l_palette)
        return;
    for (int index = 0; index < 256; index++) {
        color = l_palette->getColor(index);
        l_colorMatrix[index] = ImVec4((float)color.r/255.0f, (float)color.g/255.0f, (float)color.b/255.0f, 1.0f);
    }
    l_currentColorIndex = 0;
    l_currentColor = l_colorMatrix[0];
}

void paletteCombo(PIDStudio* app) {
    using namespace ImGui;
    if (BeginCombo(_("Select palette"), l_paletteComboBuffer)) {

        for (const auto& palette : app->libPalettes) {
            if (Selectable((palette->getName()).c_str())) {
                copyCharArray(l_paletteComboBuffer, palette->getName(), 127);
                l_palette = palette;
                makePaletteMatrix();
            }
        }

        Separator();

        for (const auto& palette : app->customPalettes) {
            if (Selectable(palette->getName().c_str())) {
                copyCharArray(l_paletteComboBuffer, palette->getName(), 127);
                l_palette = palette;
                makePaletteMatrix();
            }
        }

        EndCombo();
    }
}

void palette() {
    static constexpr ImGuiColorEditFlags colorEditFlags = 
        ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_NoDragDrop | ImGuiColorEditFlags_PickerHueBar;
    static ImU32 highlightColor = 0xFF0050FF;
    using namespace ImGui;
    if (BeginTable(_("Palette"), COLOR_TABLE_WIDTH)) {
        for (int index = 0; index < 256; index++) {
            TableNextColumn();
            
            if (ColorButton(std::to_string(index).data(), l_colorMatrix[index], colorEditFlags)) {
                l_currentColorIndex = index;
                l_currentColor = l_colorMatrix[index];
            }

            // Selection highlight
            if (index == l_currentColorIndex)
                GetWindowDrawList()->AddRect(GetItemRectMin(), GetItemRectMax(), highlightColor, 0.0f, 0, 3.0f);
        }
        EndTable();
    }
}

void colorEditPicker() {
    using namespace ImGui;

    static ImGuiColorEditFlags flags = ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoDragDrop | ImGuiColorEditFlags_NoSmallPreview;
    ColorPicker3("Color Picker", &l_currentColor.x, flags);

    SameLine();
    const float colorPreviewWidth = GetWindowWidth() - GetStyle().WindowPadding.x - GetCursorPos().x;
    const ImVec2 cursorScreenPos = GetCursorScreenPos();
    NewLine();
    ImVec2 rectMax = {
        cursorScreenPos.x + colorPreviewWidth,
        cursorScreenPos.y + colorPreviewWidth
    };

    GetWindowDrawList()->AddRectFilled(cursorScreenPos, rectMax, GetColorU32(l_currentColor));
    GetWindowDrawList()->AddRect(cursorScreenPos, rectMax, 0xFF000000);
}

void savePaletteFromMatrix(PIDStudio* app) {
    namespace fs = std::filesystem;
    const fs::path& path = l_palette->getPath();

    if (path.empty())
        return;

    static uint8_t data[768];

    for (int index = 0; index < 256; index++) {
        l_palette->setColor(
            index,
            (uint8_t)(l_colorMatrix[index].x*255.0f),
            (uint8_t)(l_colorMatrix[index].y*255.0f),
            (uint8_t)(l_colorMatrix[index].z*255.0f)
        );
    }

    l_palette->saveToFile(l_palette->getPath());
}

void exportPaletteFromMatrix(PIDStudio* app) {
    static const char* palFilter[1] = {"*.pal"};
    static size_t filterSize = 1;
    const char* selectedFile = UI::saveFileDialog(palFilter, filterSize, _("Palette files"));

    if (!selectedFile)
        return;

    namespace fs = std::filesystem;
    fs::path path = selectedFile;

    if (fs::exists(path)) {
        UI::errorMessageBox(ERROR_FILE_OVERWRITING_NOT_ALLOWED);
        return;
    }

    static uint8_t data[768];

    for (int i = 0; i < 256; i++) {
        data[3*i]   = l_colorMatrix[i].x*255.0f;
        data[3*i+1] = l_colorMatrix[i].y*255.0f;
        data[3*i+2] = l_colorMatrix[i].z*255.0f;
    }

    auto palette = std::make_shared<PIDPalette>((const unsigned char*)data);
    palette->saveToFile(path);
    palette->setPath(path);

    app->mapPalette(path, "", palette, true);
}

void PaletteMgr::openManager(PIDStudio* app) {
    l_palette = app->currentPalette;
    if (!l_palette)
        l_palette = app->defaultPalette;
    copyCharArray(l_paletteComboBuffer, l_palette->getName(), 127);
    makePaletteMatrix();
    l_mgrPopupState = OPEN;
}

void PaletteMgr::manager(PIDStudio* app) {
    switch (l_mgrPopupState) {

    case KEEP_CLOSED:
        return;

    case OPEN:
        ImGuiEx::CenterNextWindow(MGR_WIDTH, MGR_HEIGHT);
        ImGui::OpenPopup(_("Palette manager"));
        l_mgrPopupState = KEEP_OPENED;
        // fall through

    case KEEP_OPENED: {
        if (ImGui::BeginPopupModal(_("Palette manager"), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {

            paletteCombo(app);
            palette();

            if (l_palette != app->defaultPalette) {
                colorEditPicker();
                l_colorMatrix[l_currentColorIndex] = l_currentColor;
            }

            if (ImGui::Button("Cancel")) {
                l_mgrPopupState = KEEP_CLOSED;
                ImGui::CloseCurrentPopup();
            }

            ImGui::SameLine();

            bool noPalettePath = l_palette->getPath().empty();

            if (noPalettePath)
                ImGui::BeginDisabled();

            if (ImGui::Button("Save"))
                savePaletteFromMatrix(app);

            if (noPalettePath)
                ImGui::EndDisabled();

            ImGui::SameLine();

            if (ImGui::Button("Export"))
                exportPaletteFromMatrix(app);

            ImGui::EndPopup();
        }
        break;
    }

    default:
        l_mgrPopupState = KEEP_CLOSED;
        return;
    }
}