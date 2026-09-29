#pragma once

#include "window_grid.hpp"

#define RAYGUI_IMPLEMENTATION
#include "raygui.h"

namespace window_grid {

// Use three rasterized font atlases instead of scaling one atlas to many sizes.
// Scaling a bitmap atlas is what causes the fuzzy or broken glyphs here.
Font ui_font_small{};
Font ui_font_medium{};
Font ui_font_large{};
bool owns_ui_fonts = false;
bool settings_open = false;

Font& font_for_size(float requested_size) {
    if (requested_size <= 14.0f) return ui_font_small;
    if (requested_size <= 18.0f) return ui_font_medium;
    return ui_font_large;
}

void draw_text(const char* text, float x, float y, float requested_size, Color color) {
    // The custom atlases contain ASCII only. External titles and paths can
    // contain arbitrary bytes, so replace unsupported values with '?'.
    std::string safe_text;
    if (text) {
        for (const unsigned char value : std::string(text)) {
            safe_text += (value >= 32 && value <= 126) ? static_cast<char>(value) : '?';
        }
    }

    Font& font = font_for_size(requested_size);
    const float raster_size = font.baseSize > 0 ? static_cast<float>(font.baseSize) : requested_size;
    DrawTextEx(font, safe_text.c_str(), {x, y}, raster_size, 0.0f, color);
}

void initialize_gui_style() {
    GuiLoadStyleDefault();

    GuiSetStyle(DEFAULT, TEXT_SIZE, 16);
    GuiSetStyle(DEFAULT, TEXT_SPACING, 1);
    GuiSetStyle(DEFAULT, BORDER_COLOR_NORMAL, 0x303B4AFF);
    GuiSetStyle(DEFAULT, BASE_COLOR_NORMAL, 0x202833FF);
    GuiSetStyle(DEFAULT, TEXT_COLOR_NORMAL, 0xDCE5F0FF);
    GuiSetStyle(DEFAULT, BORDER_COLOR_FOCUSED, 0x5688B8FF);
    GuiSetStyle(DEFAULT, BASE_COLOR_FOCUSED, 0x2A3746FF);
    GuiSetStyle(DEFAULT, TEXT_COLOR_FOCUSED, 0xFFFFFFFF);
    GuiSetStyle(DEFAULT, BORDER_COLOR_PRESSED, 0x6AA8E8FF);
    GuiSetStyle(DEFAULT, BASE_COLOR_PRESSED, 0x385575FF);
    GuiSetStyle(DEFAULT, TEXT_COLOR_PRESSED, 0xFFFFFFFF);
    GuiSetStyle(DEFAULT, BORDER_COLOR_DISABLED, 0x30353DFF);
    GuiSetStyle(DEFAULT, BASE_COLOR_DISABLED, 0x202329FF);
    GuiSetStyle(DEFAULT, TEXT_COLOR_DISABLED, 0x777F89FF);
    GuiSetStyle(DEFAULT, BACKGROUND_COLOR, 0x161B22FF);
    GuiSetStyle(DEFAULT, TEXT_LINE_SPACING, 20);
    GuiSetStyle(DEFAULT, TEXT_ALIGNMENT, TEXT_ALIGN_CENTER);
    GuiSetStyle(BUTTON, BORDER_WIDTH, 1);
    GuiSetStyle(BUTTON, TEXT_PADDING, 10);

    // Keep the atlas deliberately small and ASCII-only. All application labels
    // are ASCII, so this avoids missing-glyph and fallback-font artifacts.
    const char* font_paths[] = {
        "C:/Windows/Fonts/segoeui.ttf",
        "C:/Windows/Fonts/tahoma.ttf",
        "C:/Windows/Fonts/arial.ttf"};
    int codepoints[95]{};
    for (int index = 0; index < 95; ++index) codepoints[index] = 32 + index;

    for (const char* font_path : font_paths) {
        if (!FileExists(font_path)) continue;
        ui_font_small = LoadFontEx(font_path, 12, codepoints, 95);
        ui_font_medium = LoadFontEx(font_path, 16, codepoints, 95);
        ui_font_large = LoadFontEx(font_path, 22, codepoints, 95);
        owns_ui_fonts = ui_font_small.texture.id != 0 &&
                        ui_font_medium.texture.id != 0 &&
                        ui_font_large.texture.id != 0 &&
                        ui_font_small.glyphCount >= 95 &&
                        ui_font_medium.glyphCount >= 95 &&
                        ui_font_large.glyphCount >= 95;
        if (owns_ui_fonts) {
            SetTextureFilter(ui_font_small.texture, TEXTURE_FILTER_POINT);
            SetTextureFilter(ui_font_medium.texture, TEXTURE_FILTER_POINT);
            SetTextureFilter(ui_font_large.texture, TEXTURE_FILTER_POINT);
            break;
        }
        if (ui_font_small.texture.id) UnloadFont(ui_font_small);
        if (ui_font_medium.texture.id) UnloadFont(ui_font_medium);
        if (ui_font_large.texture.id) UnloadFont(ui_font_large);
    }

    if (!owns_ui_fonts) {
        ui_font_small = GetFontDefault();
        ui_font_medium = GetFontDefault();
        ui_font_large = GetFontDefault();
    }
    // raygui itself uses the medium rasterized atlas and never scales it.
    GuiSetFont(ui_font_medium);
}

void shutdown_gui_style() {
    GuiSetFont(GetFontDefault());
    if (owns_ui_fonts) {
        UnloadFont(ui_font_small);
        UnloadFont(ui_font_medium);
        UnloadFont(ui_font_large);
    }
    owns_ui_fonts = false;
}

void draw_grid() {
    const Vector2 mouse = GetMousePosition();
    const int hovered_cell = cell_at_point(mouse);
    for (int cell = 0; cell < static_cast<int>(grid.cells.size()); ++cell) {
        const Rectangle rectangle = cell_rectangle(cell);
        const bool hovered = cell == hovered_cell;
        const bool occupied = grid.cells[cell] >= 0;
        const Color fill = occupied ? Color{41, 68, 92, 255} : Color{31, 35, 42, 255};
        DrawRectangleRec(rectangle, fill);
        DrawRectangleLinesEx(rectangle, hovered ? 3.0f : 1.0f,
                             hovered ? SKYBLUE : Color{90, 98, 108, 255});

        const std::string number = "Cell " + std::to_string(cell + 1);
        draw_text(number.c_str(), static_cast<float>(static_cast<int>(rectangle.x) + 12), static_cast<float>(static_cast<int>(rectangle.y) + 10), static_cast<float>(16), LIGHTGRAY);
        if (!occupied) {
            draw_text("Drop a window here", static_cast<float>(static_cast<int>(rectangle.x) + 12), static_cast<float>(static_cast<int>(rectangle.y + rectangle.height / 2)), static_cast<float>(18), GRAY);
            continue;
        }

        const managed_window& window = grid.windows[grid.cells[cell]];
        const std::string title = window.title.empty() ? "(untitled)" : window.title;
        draw_text(title.c_str(), static_cast<float>(static_cast<int>(rectangle.x) + 12), static_cast<float>(static_cast<int>(rectangle.y + rectangle.height / 2)), static_cast<float>(18), RAYWHITE);
        draw_text(window.executable_path.c_str(), static_cast<float>(static_cast<int>(rectangle.x) + 12), static_cast<float>(static_cast<int>(rectangle.y + rectangle.height - 28)), static_cast<float>(12), LIGHTGRAY);
    }
}

void draw_hamburger(Rectangle bounds, Color color) {
    const float left = bounds.x + bounds.width * 0.32f;
    const float right = bounds.x + bounds.width * 0.68f;
    const float center_y = bounds.y + bounds.height * 0.5f;
    for (int line = -1; line <= 1; ++line) {
        const float y = center_y + line * 6.0f;
        DrawLineEx({left, y}, {right, y}, 2.0f, color);
    }
}

void draw_ui() {
    ui_toolbar_height = settings_open ? settings_toolbar_height : compact_toolbar_height;

    const Color background{14, 18, 24, 255};
    const Color toolbar{22, 29, 39, 255};
    const Color accent{65, 128, 190, 255};
    ClearBackground(background);
    DrawRectangle(0, 0, GetScreenWidth(), ui_toolbar_height, toolbar);

    draw_text("WINDOW GRID", 18.0f, 13.0f, 20.0f, RAYWHITE);
    draw_text("Drag windows into cells - native embedding", 18.0f, 42.0f, 13.0f,
              Color{153, 169, 187, 255});

    const Rectangle settings_button{GetScreenWidth() - 60.0f, 12.0f, 44.0f, 44.0f};
    const Vector2 mouse = GetMousePosition();
    const bool settings_hovered = CheckCollisionPointRec(mouse, settings_button);
    if (settings_open && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
        !settings_hovered && !CheckCollisionPointRec(mouse,
            {16, 72, GetScreenWidth() - 32.0f, settings_toolbar_height - 84.0f})) {
        settings_open = false;
        ui_toolbar_height = compact_toolbar_height;
        update_cell_hosts();
    }
    DrawRectangleRec(settings_button, settings_hovered ? Color{44, 59, 77, 255} : Color{30, 40, 53, 255});
    DrawRectangleLinesEx(settings_button, 1.0f, Color{64, 81, 101, 255});
    draw_hamburger(settings_button, RAYWHITE);
    if (settings_hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        settings_open = !settings_open;
        ui_toolbar_height = settings_open ? settings_toolbar_height : compact_toolbar_height;
        update_cell_hosts();
    }

    if (settings_open) {
        DrawRectangle(16, 72, GetScreenWidth() - 32, settings_toolbar_height - 84,
                      Color{18, 24, 32, 255});
        DrawRectangleLinesEx({16, 72, GetScreenWidth() - 32.0f,
                              settings_toolbar_height - 84.0f}, 1.0f,
                             Color{48, 61, 77, 255});
        draw_text("SETTINGS", 32.0f, 88.0f, 12.0f, Color{139, 163, 188, 255});
        if (GuiButton({32, 112, 112, 36}, "Tile all")) tile_all();
        if (GuiButton({152, 112, 112, 36}, "Restore")) restore_all();
        if (GuiButton({272, 112, 92, 36}, "Save")) save_layout();
        if (GuiButton({372, 112, 112, 36}, "Refresh")) {
            validate_windows();
            status_text = "Refreshed managed windows.";
        }

        draw_text("GRID SIZE", 32.0f, 174.0f, 12.0f, Color{139, 163, 188, 255});
        draw_text("Rows", 32.0f, 207.0f, 15.0f, LIGHTGRAY);
        if (GuiButton({106, 198, 34, 34}, "-")) change_grid_size(true, -1);
        draw_text(std::to_string(grid.rows).c_str(), 153.0f, 205.0f, 17.0f, RAYWHITE);
        if (GuiButton({178, 198, 34, 34}, "+")) change_grid_size(true, 1);
        draw_text("Columns", 238.0f, 207.0f, 15.0f, LIGHTGRAY);
        if (GuiButton({326, 198, 34, 34}, "-")) change_grid_size(false, -1);
        draw_text(std::to_string(grid.columns).c_str(), 373.0f, 205.0f, 17.0f, RAYWHITE);
        if (GuiButton({398, 198, 34, 34}, "+")) change_grid_size(false, 1);
        draw_text(status_text.c_str(), 32.0f, 250.0f, 13.0f,
                  Color{184, 199, 214, 255});
    }

    DrawRectangle(0, ui_toolbar_height - 2, GetScreenWidth(), 2, accent);
    draw_grid();
    if (external_dragging) {
        DrawRectangle(0, GetScreenHeight() - 30, GetScreenWidth(), 30, Color{31, 91, 132, 245});
        draw_text("Release over a cell to embed the external window", 16.0f,
                  static_cast<float>(GetScreenHeight() - 23), 15.0f, RAYWHITE);
    }
}


} // namespace window_grid
