// Look and formatting shared by the views.

#include "style.h"

#include "core/log.h"

#include <cmath>
#include <cstdio>

namespace ui {

// Light grey theme: ImGui's light style, with greys instead of white.
// @impl 0f87-6acd-2cff-8135
void ApplyTheme()
{
    ImGui::StyleColorsLight();
    ImGuiStyle &style = ImGui::GetStyle();
    ImVec4 *colours = style.Colors;
    colours[ImGuiCol_WindowBg] = ImVec4(0.80f, 0.81f, 0.82f, 1.0f);
    colours[ImGuiCol_ChildBg] = ImVec4(0.85f, 0.86f, 0.87f, 1.0f);
    colours[ImGuiCol_PopupBg] = ImVec4(0.88f, 0.89f, 0.90f, 1.0f);
    colours[ImGuiCol_MenuBarBg] = ImVec4(0.74f, 0.75f, 0.77f, 1.0f);
    colours[ImGuiCol_FrameBg] = ImVec4(0.92f, 0.92f, 0.93f, 1.0f);
    colours[ImGuiCol_TableHeaderBg] = ImVec4(0.74f, 0.75f, 0.77f, 1.0f);
    colours[ImGuiCol_TableRowBg] = ImVec4(0.85f, 0.86f, 0.87f, 1.0f);
    colours[ImGuiCol_TableRowBgAlt] = ImVec4(0.81f, 0.82f, 0.83f, 1.0f);
    colours[ImGuiCol_Border] = ImVec4(0.60f, 0.61f, 0.63f, 1.0f);
    style.WindowRounding = 0.0f;
    style.ChildRounding = 3.0f;
    style.FrameRounding = 3.0f;
}

// Default font plus the Phosphor icon font merged in, so icon glyphs
// (Private Use Area codepoints) can appear in any text. Explicit sizes on
// both: ImGui 1.92 merges and offsets glyphs only against an explicit size.
void LoadFonts(const std::string &icon_font)
{
    constexpr float kFontSize = 13.0f;
    ImGuiIO &io = ImGui::GetIO();
    ImFontConfig base;
    base.SizePixels = kFontSize;
    io.Fonts->AddFontDefault(&base);
    if (FILE *format = std::fopen(icon_font.c_str(), "rb")) {
        std::fclose(format);
        ImFontConfig icon_config;
        icon_config.MergeMode = true;
        icon_config.GlyphOffset = ImVec2(0.0f, 2.0f);
        static const ImWchar ranges[] = {0xE000, 0xEFFF, 0};
        io.Fonts->AddFontFromFileTTF(icon_font.c_str(), kFontSize, &icon_config, ranges);
    } else {
        core::LogLine("icon font missing: %s", icon_font.c_str());
    }
}

std::string FormatDuration(double seconds)
{
    if (std::isnan(seconds))
        return "-";
    const int total = static_cast<int>(seconds + 0.5);
    char text[32];
    std::snprintf(text, sizeof text, "%d:%02d:%02d", total / 3600, (total / 60) % 60, total % 60);
    return text;
}

std::string FormatNumber(double value, const char *format)
{
    if (std::isnan(value))
        return "-";
    char text[32];
    std::snprintf(text, sizeof text, format, value);
    return text;
}

ImU32 ColourU32(unsigned int rgb, int alpha)
{
    return IM_COL32((rgb >> 16) & 0xff, (rgb >> 8) & 0xff, rgb & 0xff, alpha);
}

// @impl 0f87-6acd-32ed-1258
float Splitter(const char *id, bool vertical, float length, float size)
{
    constexpr float kThickness = 6.0f;
    const ImVec2 bar_size = vertical ? ImVec2(kThickness, length) : ImVec2(length, kThickness);
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton(id, bar_size);
    const bool active = ImGui::IsItemActive();
    if (ImGui::IsItemHovered() || active)
        ImGui::SetMouseCursor(vertical ? ImGuiMouseCursor_ResizeEW : ImGuiMouseCursor_ResizeNS);
    const ImU32 colour = ImGui::GetColorU32(active                   ? ImGuiCol_SeparatorActive
                                         : ImGui::IsItemHovered() ? ImGuiCol_SeparatorHovered
                                                                  : ImGuiCol_Separator);
    ImDrawList *draw_list = ImGui::GetWindowDrawList();
    if (vertical)
        draw_list->AddLine(ImVec2(origin.x + kThickness / 2, origin.y), ImVec2(origin.x + kThickness / 2, origin.y + length), colour, 2.0f);
    else
        draw_list->AddLine(ImVec2(origin.x, origin.y + kThickness / 2), ImVec2(origin.x + length, origin.y + kThickness / 2), colour, 2.0f);
    if (!active)
        return -1.0f;
    const float delta = vertical ? ImGui::GetIO().MouseDelta.x : ImGui::GetIO().MouseDelta.y;
    return delta == 0.0f ? -1.0f : size + delta;
}

} // namespace ui
