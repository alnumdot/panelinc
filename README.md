# window_grid

`window_grid` is a lightweight Windows window workspace for people who need several tools visible at once. Drag terminals, Pi/Codex sessions, VS Code windows, browsers, or other applications into a grid and use them directly where they are docked.

It is useful for project workflows that need multiple windows without the overhead of a full desktop manager. Each window remains a real native Windows application, so its normal input, rendering, and behavior are preserved while it is hosted inside a grid cell.

The project deliberately stays small:

- C++17
- raylib for the application shell and drawing
- raygui for lightweight controls
- Win32 for window discovery, embedding, focus, resizing, and restoration
- one `main.cpp` and two compact headers
- no tabs, services, databases, plugins, or heavyweight UI framework

## Why it is useful

- Keep several related tools organized in one view.
- Quickly switch between terminals, editors, documentation, and other project windows.
- Resize the main grid and let embedded windows follow it.
- Restore windows to their original state when finished.
- Save and reload a small layout file without adding a database or configuration system.

It is intended to be a practical, focused utility rather than a large window-management suite.

## Source layout

- `main.cpp` — application startup and main loop
- `window_grid.hpp` — Win32 window hosting, grid behavior, persistence, and drag handling
- `gui.hpp` — raygui theme, font setup, hamburger settings menu, and interface rendering

raygui uses its built-in default style as a base, then applies a custom dark blue-gray palette. The main view has a compact header and a three-line settings button. Tile, Restore, Save, Refresh, Rows, and Columns are available from the expandable settings panel. The font loader uses three fixed-size ASCII-only atlases: 12 px small, 16 px medium, and 22 px large. Text selects the nearest atlas and is rendered at that atlas's native size rather than scaling one font up or down. It tries Segoe UI, Tahoma, and Arial before falling back to raylib's default font. No additional font or UI assets are bundled.

## Features

- One grid; tabs are intentionally not included yet.
- Drag an external top-level window onto a cell; it is reparented as a native child of that cell and fills it.
- Each embedded cell displays the current external window title in a native title strip.
- Drag a managed cell to another cell, or outside the grid to detach and restore it.
- **Restore** detaches every managed window, returns its original styles and position, and clears all grid cells. The windows remain tracked and can be dragged into the grid again.
- Tile managed windows across the monitor work area with **Tile all**.
- Focus a managed window by clicking its cell.
- Detect closed windows.
- Save a compact `window_grid.layout` file and rematch windows by executable path/title.

## Dependencies

Install or build:

- [raylib](https://github.com/raysan5/raylib)
- [raygui](https://github.com/raysan5/raygui) (`raygui.h` only is needed)
- CMake 3.20 or newer

No dependencies are vendored in this repository.

## Build with CMake

```powershell
cmake -S . -B build -Draylib_DIR=C:/path/to/raylib/build/src -DRAYGUI_DIR=C:/path/to/raygui/src
cmake --build build --config Release
```

Run `build/Release/window_grid.exe` for a Visual Studio generator, or `build/window_grid.exe` for a single-configuration generator.

## Notes

The drag-in behavior is implemented with lightweight polling of the cursor, left mouse button, and the top-level Win32 window beneath the cursor. The original HWND is retained throughout the drag. On release, the program uses `SetParent` and Win32 style changes to host the external HWND as a child of a native cell HWND, then sizes the child to the cell client area. This is real native HWND hosting rather than a screenshot or DirectX capture.

Some applications deliberately reject reparenting, depend on top-level window behavior, or use a renderer that does not tolerate being made a child window. Those windows remain outside the grid and show a failure status. Capturing another process's DirectX surface with Windows Graphics Capture/DWM thumbnails would only produce a visual preview; it would not allow normal interaction and would require a separate input-routing layer, so it is not used here.

Windows running at a higher integrity level may reject focus or resize requests. The grid keeps those windows assigned and continues running.
