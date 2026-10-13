/// @file
/// Look and formatting shared by the views (module-level utilities).
#pragma once

#include <imgui.h>

#include <string>

namespace ui {

/// Applies the light grey theme to ImGui and ImPlot.
void ApplyTheme();

/// Loads the default font with the icon font merged in.
/// @param icon_font path of the Phosphor TTF; logged and skipped if missing
void LoadFonts(const std::string &icon_font);

/// "h:mm:ss", or "-" for NaN.
std::string FormatDuration(double seconds);

/// `value` formatted with the printf `format`, or "-" for NaN.
std::string FormatNumber(double value, const char *format);

/// ImGui colour of a 0xRRGGBB colour with `alpha` (0-255).
ImU32 ColourU32(unsigned int rgb, int alpha);

/// Colour of the `index`-th compared passage (a palette of ten, repeated),
/// the same on the map and in the graphs.
ImVec4 RunColour(size_t index);

/// A draggable separator of `length` pixels, placed at the cursor.
/// @param id ImGui identifier
/// @param vertical a vertical bar resizing horizontally
/// @param length bar length, pixels
/// @param size the current size of the area it resizes
/// @return the new size while dragged, -1 otherwise
float Splitter(const char *id, bool vertical, float length, float size);

} // namespace ui
