#pragma once

#define WIN32_LEAN_AND_MEAN
#define Rectangle Rectangle_win32
#define CloseWindow close_window_win32
#define ShowCursor show_cursor_win32
#define LoadImageA load_image_a_win32
#define LoadImageW load_image_w_win32
#include <windows.h>
#undef CloseWindow
#undef ShowCursor
#undef LoadImageA
#undef LoadImageW
#undef Rectangle
#ifdef DrawText
#undef DrawText
#endif
#ifdef DrawTextEx
#undef DrawTextEx
#endif
#ifdef LoadImage
#undef LoadImage
#endif

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <string>
#include <vector>

#include "raylib.h"

namespace window_grid {

constexpr int compact_toolbar_height = 72;
constexpr int host_title_height = 28;
constexpr int settings_toolbar_height = 292;
int ui_toolbar_height = compact_toolbar_height;
constexpr int min_grid_size = 1;
constexpr int max_grid_size = 6;

struct managed_window {
    HWND hwnd = nullptr;
    DWORD process_id = 0;
    std::string executable_path;
    std::string title;
    RECT restore_rect{};
    bool has_restore_rect = false;
    bool was_maximized = false;
    HWND host_hwnd = nullptr;
    HWND original_parent = nullptr;
    LONG_PTR original_style = 0;
    LONG_PTR original_exstyle = 0;
    DWORD input_thread_id = 0;
    bool input_attached = false;
    bool embedded = false;
};

struct window_candidate {
    HWND hwnd = nullptr;
    DWORD process_id = 0;
    std::string executable_path;
    std::string title;
};

struct grid_state {
    int rows = 2;
    int columns = 2;
    int gap = 8;
    int margin = 12;
    std::vector<managed_window> windows;
    std::vector<int> cells;
};

HWND manager_hwnd = nullptr;
grid_state grid;
std::vector<HWND> cell_hosts;
std::vector<RECT> cell_host_rects;
std::vector<bool> cell_host_visible;
std::string status_text = "Drag a window onto a cell to add it.";
bool external_dragging = false;
HWND dragged_hwnd = nullptr;
int dragged_cell = -1;
bool previous_left_down = false;
double next_validation_time = 0.0;

std::string window_title(HWND hwnd) {
    const int length = GetWindowTextLengthA(hwnd);
    if (length <= 0) return {};
    std::string result(static_cast<size_t>(length) + 1, '\0');
    GetWindowTextA(hwnd, result.data(), length + 1);
    result.resize(static_cast<size_t>(length));
    return result;
}

std::string executable_path(HWND hwnd) {
    DWORD process_id = 0;
    GetWindowThreadProcessId(hwnd, &process_id);
    if (!process_id) return {};

    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, process_id);
    if (!process) return {};

    char buffer[MAX_PATH]{};
    DWORD size = static_cast<DWORD>(sizeof(buffer));
    const BOOL ok = QueryFullProcessImageNameA(process, 0, buffer, &size);
    CloseHandle(process);
    return ok ? std::string(buffer, size) : std::string{};
}

bool same_text(const std::string& left, const std::string& right) {
    if (left.size() != right.size()) return false;
    for (size_t index = 0; index < left.size(); ++index) {
        if (std::tolower(static_cast<unsigned char>(left[index])) !=
            std::tolower(static_cast<unsigned char>(right[index]))) {
            return false;
        }
    }
    return true;
}

bool contains_text(const std::string& text, const std::string& part) {
    if (part.empty()) return true;
    std::string lower_text = text;
    std::string lower_part = part;
    std::transform(lower_text.begin(), lower_text.end(), lower_text.begin(),
                   [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
    std::transform(lower_part.begin(), lower_part.end(), lower_part.begin(),
                   [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
    return lower_text.find(lower_part) != std::string::npos;
}

bool is_usable_window(HWND hwnd) {
    if (!hwnd || hwnd == manager_hwnd || !IsWindow(hwnd) || !IsWindowVisible(hwnd)) return false;
    if (GetWindow(hwnd, GW_OWNER) != nullptr) return false;
    if (GetWindowLongPtrA(hwnd, GWL_EXSTYLE) & WS_EX_TOOLWINDOW) return false;
    return !window_title(hwnd).empty();
}

HWND root_window_at_cursor() {
    POINT point{};
    GetCursorPos(&point);
    HWND hwnd = WindowFromPoint(point);
    return hwnd ? GetAncestor(hwnd, GA_ROOT) : nullptr;
}

BOOL CALLBACK enumerate_windows_callback(HWND hwnd, LPARAM parameter) {
    auto* result = reinterpret_cast<std::vector<window_candidate>*>(parameter);
    if (!is_usable_window(hwnd)) return TRUE;

    DWORD process_id = 0;
    GetWindowThreadProcessId(hwnd, &process_id);
    result->push_back({hwnd, process_id, executable_path(hwnd), window_title(hwnd)});
    return TRUE;
}

std::vector<window_candidate> enumerate_windows() {
    std::vector<window_candidate> result;
    EnumWindows(enumerate_windows_callback, reinterpret_cast<LPARAM>(&result));
    return result;
}

void restore_window(managed_window& window);
void detach_window(managed_window& window);
void ensure_cell_hosts();
void update_cell_hosts();
void dock_window(managed_window& window, int cell);

int cell_at_point(Vector2 point) {
    const int width = GetScreenWidth();
    const int height = GetScreenHeight();
    const int grid_width = width - grid.margin * 2;
    const int grid_height = height - ui_toolbar_height - grid.margin;
    const float cell_width = static_cast<float>(grid_width - grid.gap * (grid.columns - 1)) / grid.columns;
    const float cell_height = static_cast<float>(grid_height - grid.gap * (grid.rows - 1)) / grid.rows;

    for (int row = 0; row < grid.rows; ++row) {
        for (int column = 0; column < grid.columns; ++column) {
            const Rectangle rectangle{
                grid.margin + column * (cell_width + grid.gap),
                ui_toolbar_height + row * (cell_height + grid.gap),
                cell_width,
                cell_height};
            if (CheckCollisionPointRec(point, rectangle)) return row * grid.columns + column;
        }
    }
    return -1;
}

Rectangle cell_rectangle(int cell) {
    const int width = GetScreenWidth();
    const int height = GetScreenHeight();
    const int grid_width = width - grid.margin * 2;
    const int grid_height = height - ui_toolbar_height - grid.margin;
    const float cell_width = static_cast<float>(grid_width - grid.gap * (grid.columns - 1)) / grid.columns;
    const float cell_height = static_cast<float>(grid_height - grid.gap * (grid.rows - 1)) / grid.rows;
    const int row = cell / grid.columns;
    const int column = cell % grid.columns;
    return {grid.margin + column * (cell_width + grid.gap),
            ui_toolbar_height + row * (cell_height + grid.gap), cell_width, cell_height};
}

managed_window* managed_window_for_host(HWND host) {
    for (managed_window& window : grid.windows)
        if (window.host_hwnd == host && window.embedded) return &window;
    return nullptr;
}

void resize_host_child(HWND host) {
    RECT client_rect{};
    GetClientRect(host, &client_rect);
    const int width = client_rect.right > client_rect.left ? client_rect.right - client_rect.left : 0;
    const int height = client_rect.bottom - client_rect.top > host_title_height
                           ? client_rect.bottom - client_rect.top - host_title_height
                           : 0;

    for (HWND child = GetWindow(host, GW_CHILD); child; child = GetWindow(child, GW_HWNDNEXT)) {
        SetWindowPos(child, nullptr, 0, host_title_height, width, height,
                     SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_SHOWWINDOW);
    }
}

void paint_host_title(HWND host) {
    PAINTSTRUCT paint{};
    HDC device_context = BeginPaint(host, &paint);
    RECT title_rect{};
    GetClientRect(host, &title_rect);
    title_rect.bottom = host_title_height;

    HBRUSH background_brush = CreateSolidBrush(RGB(27, 39, 53));
    FillRect(device_context, &title_rect, background_brush);
    DeleteObject(background_brush);

    managed_window* window = managed_window_for_host(host);
    const std::string title = window && !window->title.empty() ? window->title : "Embedded window";
    SetBkMode(device_context, TRANSPARENT);
    SetTextColor(device_context, RGB(225, 233, 242));
    HFONT font = CreateFontA(-14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                             CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
    HGDIOBJ previous_font = SelectObject(device_context, font);
    RECT text_rect = title_rect;
    text_rect.left += 10;
    text_rect.right -= 10;
    DrawTextA(device_context, title.c_str(), -1, &text_rect,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    SelectObject(device_context, previous_font);
    DeleteObject(font);
    EndPaint(host, &paint);
}

LRESULT CALLBACK cell_host_window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    (void)lparam;
    if (message == WM_ERASEBKGND) return 1;
    if (message == WM_PAINT) {
        paint_host_title(hwnd);
        return 0;
    }
    if (message == WM_MOUSEACTIVATE) return MA_ACTIVATE;
    if (message == WM_LBUTTONDOWN || message == WM_NCLBUTTONDOWN) {
        const HWND child = GetWindow(hwnd, GW_CHILD);
        if (child) SetFocus(child);
    }
    if (message == WM_SETFOCUS) {
        const HWND child = GetWindow(hwnd, GW_CHILD);
        if (child) SetFocus(child);
        return 0;
    }
    if (message == WM_SIZE) {
        resize_host_child(hwnd);
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }
    return DefWindowProcA(hwnd, message, wparam, lparam);
}

void ensure_cell_hosts() {
    if (!manager_hwnd) return;

    static bool class_registered = false;
    static const char* class_name = "window_grid_cell_host";
    if (!class_registered) {
        WNDCLASSA window_class{};
        window_class.lpfnWndProc = cell_host_window_proc;
        window_class.hInstance = GetModuleHandleA(nullptr);
        window_class.hCursor = LoadCursorA(nullptr, IDC_ARROW);
        window_class.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        window_class.lpszClassName = class_name;
        class_registered = RegisterClassA(&window_class) != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
    }
    if (!class_registered) return;

    const int count = grid.rows * grid.columns;
    while (static_cast<int>(cell_hosts.size()) < count) {
        const HWND host = CreateWindowExA(
            WS_EX_CONTROLPARENT, class_name, "",
            WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_TABSTOP,
            0, 0, 0, 0, manager_hwnd, nullptr, GetModuleHandleA(nullptr), nullptr);
        if (!host) break;
        cell_hosts.push_back(host);
        cell_host_rects.push_back({-1, -1, -1, -1});
        cell_host_visible.push_back(false);
    }
    while (static_cast<int>(cell_hosts.size()) > count) {
        const HWND host = cell_hosts.back();
        const int cell = static_cast<int>(cell_hosts.size()) - 1;
        if (cell < static_cast<int>(grid.cells.size())) {
            const int index = grid.cells[cell];
            if (index >= 0 && index < static_cast<int>(grid.windows.size()))
                restore_window(grid.windows[index]);
        }
        ShowWindow(host, SW_HIDE);
        DestroyWindow(host);
        cell_hosts.pop_back();
        cell_host_rects.pop_back();
        cell_host_visible.pop_back();
    }
}

void update_cell_hosts() {
    ensure_cell_hosts();
    if (!manager_hwnd) return;

    RECT client_rect{};
    GetClientRect(manager_hwnd, &client_rect);
    const int client_width = client_rect.right - client_rect.left;
    const int client_height = client_rect.bottom - client_rect.top;
    const int grid_width = client_width - grid.margin * 2;
    const int grid_height = client_height - ui_toolbar_height - grid.margin;
    const float cell_width = static_cast<float>(grid_width - grid.gap * (grid.columns - 1)) / grid.columns;
    const float cell_height = static_cast<float>(grid_height - grid.gap * (grid.rows - 1)) / grid.rows;

    HDWP deferred_position = BeginDeferWindowPos(static_cast<int>(cell_hosts.size()));
    std::vector<RECT> next_rects = cell_host_rects;
    for (int cell = 0; cell < static_cast<int>(cell_hosts.size()); ++cell) {
        const int row = cell / grid.columns;
        const int column = cell % grid.columns;
        const int x = static_cast<int>(grid.margin + column * (cell_width + grid.gap));
        const int y = static_cast<int>(ui_toolbar_height + row * (cell_height + grid.gap));
        const int width = cell_width > 0.0f ? static_cast<int>(cell_width) : 0;
        const int height = cell_height > 0.0f ? static_cast<int>(cell_height) : 0;
        RECT& previous_rect = cell_host_rects[cell];
        const bool changed = previous_rect.left != x || previous_rect.top != y ||
                             previous_rect.right != width || previous_rect.bottom != height;
        if (changed) {
            if (deferred_position) {
                deferred_position = DeferWindowPos(
                    deferred_position, cell_hosts[cell], nullptr, x, y, width, height,
                    SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOOWNERZORDER);
            } else {
                SetWindowPos(cell_hosts[cell], nullptr, x, y, width, height,
                             SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOOWNERZORDER);
            }
            next_rects[cell] = {x, y, width, height};
        }
    }
    if (deferred_position) EndDeferWindowPos(deferred_position);
    cell_host_rects.swap(next_rects);

    for (int cell = 0; cell < static_cast<int>(cell_hosts.size()); ++cell) {
        const bool occupied = cell < static_cast<int>(grid.cells.size()) && grid.cells[cell] >= 0;
        if (cell_host_visible[cell] != occupied) {
            ShowWindow(cell_hosts[cell], occupied ? SW_SHOWNOACTIVATE : SW_HIDE);
            cell_host_visible[cell] = occupied;
        }
    }
}

void ensure_cell_count() {
    const int count = grid.rows * grid.columns;
    if (static_cast<int>(grid.cells.size()) < count) grid.cells.resize(count, -1);
    if (static_cast<int>(grid.cells.size()) > count) {
        for (size_t index = static_cast<size_t>(count); index < grid.cells.size(); ++index) {
            const int window_index = grid.cells[index];
            if (window_index >= 0 && window_index < static_cast<int>(grid.windows.size()))
                restore_window(grid.windows[window_index]);
        }
        grid.cells.resize(count);
    }
}

int window_index(HWND hwnd) {
    for (int index = 0; index < static_cast<int>(grid.windows.size()); ++index)
        if (grid.windows[index].hwnd == hwnd) return index;
    return -1;
}

void remember_window(managed_window& window) {
    if (window.has_restore_rect || !IsWindow(window.hwnd)) return;
    GetWindowRect(window.hwnd, &window.restore_rect);
    window.was_maximized = IsZoomed(window.hwnd) != FALSE;
    window.has_restore_rect = true;
}

void detach_window(managed_window& window) {
    if (!window.embedded || !IsWindow(window.hwnd)) return;

    if (window.input_attached) {
        AttachThreadInput(GetCurrentThreadId(), window.input_thread_id, FALSE);
        window.input_attached = false;
        window.input_thread_id = 0;
    }
    SetParent(window.hwnd, window.original_parent);
    SetWindowLongPtrA(window.hwnd, GWL_STYLE, window.original_style);
    SetWindowLongPtrA(window.hwnd, GWL_EXSTYLE, window.original_exstyle);
    SetWindowPos(window.hwnd, nullptr, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE |
                 SWP_FRAMECHANGED);
    window.embedded = false;
    window.host_hwnd = nullptr;
}

void restore_window(managed_window& window) {
    if (!IsWindow(window.hwnd)) return;
    detach_window(window);
    if (!window.has_restore_rect) return;

    ShowWindow(window.hwnd, SW_RESTORE);
    SetWindowPos(window.hwnd, nullptr, window.restore_rect.left, window.restore_rect.top,
                 window.restore_rect.right - window.restore_rect.left,
                 window.restore_rect.bottom - window.restore_rect.top,
                 SWP_NOACTIVATE | SWP_NOZORDER | SWP_SHOWWINDOW);
    if (window.was_maximized) ShowWindow(window.hwnd, SW_MAXIMIZE);
    window.has_restore_rect = false;
}

void focus_window(HWND hwnd) {
    if (!IsWindow(hwnd)) return;

    const int index = window_index(hwnd);
    const bool embedded = index >= 0 && grid.windows[index].embedded;
    if (!embedded) ShowWindow(hwnd, SW_RESTORE);

    const DWORD current_thread = GetCurrentThreadId();
    const DWORD target_thread = GetWindowThreadProcessId(hwnd, nullptr);
    bool temporary_attach = false;
    if (target_thread && target_thread != current_thread &&
        !(index >= 0 && grid.windows[index].input_attached)) {
        temporary_attach = AttachThreadInput(current_thread, target_thread, TRUE) != FALSE;
    }

    const HWND activation_window = embedded ? manager_hwnd : hwnd;
    SetForegroundWindow(activation_window);
    SetActiveWindow(activation_window);
    EnableWindow(hwnd, TRUE);
    SetFocus(hwnd);
    BringWindowToTop(hwnd);

    if (temporary_attach) AttachThreadInput(current_thread, target_thread, FALSE);
}

void dock_window(managed_window& window, int cell) {
    if (!IsWindow(window.hwnd) || cell < 0 || cell >= static_cast<int>(grid.cells.size())) return;
    update_cell_hosts();
    if (cell >= static_cast<int>(cell_hosts.size()) || !IsWindow(cell_hosts[cell])) return;

    HWND host = cell_hosts[cell];
    if (!window.embedded) {
        remember_window(window);
        window.original_parent = GetParent(window.hwnd);
        window.original_style = GetWindowLongPtrA(window.hwnd, GWL_STYLE);
        window.original_exstyle = GetWindowLongPtrA(window.hwnd, GWL_EXSTYLE);
        LONG_PTR child_style = window.original_style;
        child_style &= ~(WS_POPUP | WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX |
                         WS_MAXIMIZEBOX | WS_SYSMENU);
        child_style |= WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
        SetWindowLongPtrA(window.hwnd, GWL_STYLE, child_style);
        SetWindowLongPtrA(window.hwnd, GWL_EXSTYLE,
                          window.original_exstyle & ~static_cast<LONG_PTR>(WS_EX_APPWINDOW));
        SetParent(window.hwnd, host);
        if (GetParent(window.hwnd) != host) {
            SetWindowLongPtrA(window.hwnd, GWL_STYLE, window.original_style);
            SetWindowLongPtrA(window.hwnd, GWL_EXSTYLE, window.original_exstyle);
            status_text = "This window refused to embed.";
            return;
        }
        SetWindowPos(window.hwnd, nullptr, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE |
                     SWP_FRAMECHANGED);
        window.input_thread_id = GetWindowThreadProcessId(window.hwnd, nullptr);
        if (window.input_thread_id && window.input_thread_id != GetCurrentThreadId()) {
            window.input_attached = AttachThreadInput(
                GetCurrentThreadId(), window.input_thread_id, TRUE) != FALSE;
        }
        window.embedded = true;
    } else if (window.host_hwnd != host) {
        SetParent(window.hwnd, host);
    }

    window.host_hwnd = host;
    EnableWindow(window.hwnd, TRUE);
    ShowWindow(window.hwnd, SW_SHOW);
    SetWindowTextA(host, window.title.c_str());
    resize_host_child(host);
    InvalidateRect(host, nullptr, FALSE);
    SetFocus(window.hwnd);
}

void remove_from_cell(int cell, bool restore) {
    if (cell < 0 || cell >= static_cast<int>(grid.cells.size())) return;
    const int index = grid.cells[cell];
    if (index < 0 || index >= static_cast<int>(grid.windows.size())) return;
    grid.cells[cell] = -1;
    if (restore) restore_window(grid.windows[index]);
}

void assign_window(HWND hwnd, int cell) {
    if (!is_usable_window(hwnd) || cell < 0 || cell >= static_cast<int>(grid.cells.size())) return;

    int index = window_index(hwnd);
    if (index < 0) {
        DWORD process_id = 0;
        GetWindowThreadProcessId(hwnd, &process_id);
        index = static_cast<int>(grid.windows.size());
        grid.windows.push_back({hwnd, process_id, executable_path(hwnd), window_title(hwnd)});
        remember_window(grid.windows.back());
    }

    int old_cell = -1;
    for (int current = 0; current < static_cast<int>(grid.cells.size()); ++current)
        if (grid.cells[current] == index) old_cell = current;

    if (old_cell == cell) {
        focus_window(hwnd);
        return;
    }

    const int displaced_index = grid.cells[cell];
    if (old_cell >= 0) {
        grid.cells[old_cell] = displaced_index;
        if (displaced_index >= 0) grid.windows[displaced_index].has_restore_rect = true;
    } else if (displaced_index >= 0 && displaced_index != index) {
        restore_window(grid.windows[displaced_index]);
    }
    grid.cells[cell] = index;
    dock_window(grid.windows[index], cell);
    status_text = "Window docked in cell " + std::to_string(cell + 1) + ".";
}

void tile_all() {
    ensure_cell_hosts();
    update_cell_hosts();
    int docked_count = 0;
    for (const int index : grid.cells)
        if (index >= 0 && index < static_cast<int>(grid.windows.size()) &&
            grid.windows[index].embedded && IsWindow(grid.windows[index].hwnd))
            ++docked_count;
    status_text = "Docked " + std::to_string(docked_count) + " window(s) into the grid.";
}

void restore_all() {
    for (managed_window& window : grid.windows) restore_window(window);
    std::fill(grid.cells.begin(), grid.cells.end(), -1);
    update_cell_hosts();
    status_text = "Restored windows and cleared the grid.";
}

void validate_windows() {
    for (int index = static_cast<int>(grid.windows.size()) - 1; index >= 0; --index) {
        managed_window& window = grid.windows[index];
        if (IsWindow(window.hwnd)) {
            const std::string current_title = window_title(window.hwnd);
            if (current_title != window.title) {
                window.title = current_title;
                if (window.host_hwnd) InvalidateRect(window.host_hwnd, nullptr, FALSE);
            }
            continue;
        }
        for (int& cell : grid.cells) {
            if (cell == index) cell = -1;
            else if (cell > index) --cell;
        }
        grid.windows.erase(grid.windows.begin() + index);
        status_text = "A managed window closed.";
    }
}

void save_layout() {
    std::ofstream file("window_grid.layout");
    if (!file) {
        status_text = "Could not save window_grid.layout.";
        return;
    }
    file << "window_grid 1\n" << grid.rows << ' ' << grid.columns << ' '
         << grid.gap << ' ' << grid.margin << "\n";
    for (int cell = 0; cell < static_cast<int>(grid.cells.size()); ++cell) {
        const int index = grid.cells[cell];
        if (index < 0 || index >= static_cast<int>(grid.windows.size())) continue;
        const managed_window& window = grid.windows[index];
        file << cell << ' ' << std::quoted(window.executable_path) << ' '
             << std::quoted(window.title) << '\n';
    }
    status_text = "Saved window_grid.layout.";
}

void load_layout() {
    std::ifstream file("window_grid.layout");
    if (!file) return;
    std::string header;
    int version = 0;
    if (!(file >> header >> version) || header != "window_grid") return;
    if (!(file >> grid.rows >> grid.columns >> grid.gap >> grid.margin)) return;
    grid.rows = std::clamp(grid.rows, min_grid_size, max_grid_size);
    grid.columns = std::clamp(grid.columns, min_grid_size, max_grid_size);
    ensure_cell_count();

    const std::vector<window_candidate> candidates = enumerate_windows();
    int cell = 0;
    std::string path;
    std::string title;
    while (file >> cell >> std::quoted(path) >> std::quoted(title)) {
        for (const window_candidate& candidate : candidates) {
            if (!same_text(candidate.executable_path, path) || !contains_text(candidate.title, title)) continue;
            assign_window(candidate.hwnd, cell);
            break;
        }
    }
    status_text = "Loaded layout; drag missing windows into cells.";
}

void change_grid_size(bool rows, int amount) {
    int& value = rows ? grid.rows : grid.columns;
    value = std::clamp(value + amount, min_grid_size, max_grid_size);
    ensure_cell_count();
}

void handle_global_drag() {
    const bool left_down = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    const bool pressed = left_down && !previous_left_down;
    const bool released = !left_down && previous_left_down;
    POINT screen_point{};
    GetCursorPos(&screen_point);
    POINT client_point = screen_point;
    ScreenToClient(manager_hwnd, &client_point);
    const Vector2 mouse{static_cast<float>(client_point.x), static_cast<float>(client_point.y)};
    const int hovered_cell = cell_at_point(mouse);

    if (pressed) {
        const HWND point_window = WindowFromPoint(screen_point);
        for (managed_window& window : grid.windows) {
            if (!window.embedded || !IsWindow(window.hwnd)) continue;
            if (point_window == window.hwnd || IsChild(window.hwnd, point_window)) {
                focus_window(window.hwnd);
                break;
            }
        }

        const HWND under_cursor = root_window_at_cursor();
        if (under_cursor != manager_hwnd && is_usable_window(under_cursor)) {
            external_dragging = true;
            dragged_hwnd = under_cursor;
            status_text = "Release over a cell to add the dragged window.";
        } else if (hovered_cell >= 0 && hovered_cell < static_cast<int>(grid.cells.size()) &&
                   grid.cells[hovered_cell] >= 0) {
            dragged_cell = hovered_cell;
            focus_window(grid.windows[grid.cells[hovered_cell]].hwnd);
        }
    }

    // The external application can retain mouse capture while its title bar is
    // being dragged. Keep the original HWND instead of looking it up again.
    if (external_dragging && left_down && dragged_hwnd && !IsWindow(dragged_hwnd)) {
        external_dragging = false;
        dragged_hwnd = nullptr;
        status_text = "The dragged window closed.";
    }

    if (released) {
        if (external_dragging) {
            if (hovered_cell >= 0) assign_window(dragged_hwnd, hovered_cell);
            else status_text = "Window was not dropped on a grid cell.";
            external_dragging = false;
            dragged_hwnd = nullptr;
        } else if (dragged_cell >= 0) {
            if (hovered_cell < 0) {
                remove_from_cell(dragged_cell, true);
                status_text = "Removed window and restored its original position.";
            } else if (hovered_cell != dragged_cell) {
                const int moved_index = grid.cells[dragged_cell];
                const int other_index = grid.cells[hovered_cell];
                grid.cells[hovered_cell] = moved_index;
                grid.cells[dragged_cell] = other_index;
                if (moved_index >= 0 && moved_index < static_cast<int>(grid.windows.size()))
                    dock_window(grid.windows[moved_index], hovered_cell);
                if (other_index >= 0 && other_index < static_cast<int>(grid.windows.size()))
                    dock_window(grid.windows[other_index], dragged_cell);
                status_text = "Moved and docked window to another cell.";
            }
            dragged_cell = -1;
        }
    }
    previous_left_down = left_down;
}


} // namespace window_grid
