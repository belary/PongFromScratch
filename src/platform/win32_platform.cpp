
#include <windows.h>

#include "platform.h"
#include "game/game.h"
#include "renderer/vk_renderer.cpp"

global_variable bool running = true;
global_variable HWND window;
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CLOSE:
        running = false;
        break;
    }
    return DefWindowProcA(hwnd, uMsg, wParam, lParam);
}

bool platform_create_window()
{
    HINSTANCE hInstance = GetModuleHandleA(0);
    WNDCLASSA wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = "vulkan_engine_class";

    if (!RegisterClassA(&wc))
    {
        MessageBoxA(nullptr, "Failed to register window class!", "Error", MB_ICONERROR | MB_OK);
        return false;
    }

    window =
        CreateWindowExA(WS_EX_APPWINDOW, "vulkan_engine_class", "Pong Game",
                        WS_CAPTION | WS_MINIMIZEBOX | WS_SYSMENU | WS_MAXIMIZEBOX | WS_OVERLAPPED,
                        100, 100, 800, 600, 0, 0, hInstance, 0);

    if (window == nullptr)
    {
        MessageBoxA(nullptr, "Failed to create window!", "Error", MB_ICONERROR | MB_OK);
        return false;
    }

    ShowWindow(window, SW_SHOW);
    return true;
}

void platform_update_window(HWND objWindow)
{
    MSG msg;
    while (PeekMessageA(&msg, objWindow, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
}

int main()
{
    VkContext vkContext = {};
    GameState gameState = {};
    if (!platform_create_window())
    {
        CAKEZ_FATAL("Failed to open a window");
        return -2;
    }
    if (!init_game(&gameState))
    {
        CAKEZ_FATAL("Failed to init game");
        return -1;
    }
    if (!vk_init(&vkContext, window))
    {
        return -3;
    }

    while (running)
    {
        platform_update_window(window);
        update_game(&gameState);
        if (!vk_render(&vkContext, &gameState))
        {
            CAKEZ_FATAL("Failed to render vulken");
            return -4;
        }
    }
    return 0;
}

void platform_get_window_size(uint32_t* width, uint32_t* height)
{
    RECT rect;
    GetClientRect(window, &rect);
    *width = rect.right - rect.left;
    *height = rect.bottom - rect.top;
}

char* platform_read_file(char* path, uint32_t* length)
{
    char* result = 0;
    HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
    if (file != INVALID_HANDLE_VALUE)
    {
        LARGE_INTEGER size;
        if (GetFileSizeEx(file, &size))
        {
            *length = size.QuadPart;
            result = new char[*length];

            DWORD bytesRead;
            if (ReadFile(file, result, *length, &bytesRead, 0) && bytesRead == *length)
            {
            }
            else
            {
                CAKEZ_ERROR("Fail to read file ");
            }
        }
    }
    else
    {
        CAKEZ_ERROR("Fail to open file ");
    }
    return result;
}

void platform_log(const char* msg, TextColor color)
{
    HANDLE consoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);

    uint32_t colorBits = 0;

    switch (color)
    {
    // 三原色 合成白色
    case TEXT_COLOR_WHITE:
        colorBits = FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED;
        break;
    case TEXT_COLOR_GREEN:
        colorBits = FOREGROUND_GREEN;
        break;
    case TEXT_COLOR_YELLOW:
        colorBits = FOREGROUND_RED | FOREGROUND_GREEN;
        break;
    case TEXT_COLOR_RED:
        colorBits = FOREGROUND_RED;
        break;
    case TEXT_COLOR_LIGHT_RED:
        colorBits = FOREGROUND_RED | FOREGROUND_INTENSITY;
        break;
    }
    SetConsoleTextAttribute(consoleHandle, colorBits);

#ifdef DEBUG
    OutputDebugStringA(msg);
#endif
    WriteConsoleA(consoleHandle, msg, strlen(msg), 0, 0);
}