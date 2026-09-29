#include "window_grid.hpp"
#include "gui.hpp"

using namespace window_grid;

int main() {

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(1100, 700, "window grid");
    SetTargetFPS(60);
    manager_hwnd = static_cast<HWND>(GetWindowHandle());
    initialize_gui_style();
    ensure_cell_count();
    load_layout();

    while (!WindowShouldClose()) {
        if (GetTime() >= next_validation_time) {
            validate_windows();
            next_validation_time = GetTime() + 1.0;
        }
        ensure_cell_count();
        update_cell_hosts();
        handle_global_drag();
        BeginDrawing();
        draw_ui();
        EndDrawing();
    }

    save_layout();
    restore_all();
    shutdown_gui_style();
    CloseWindow();
    return 0;
}
