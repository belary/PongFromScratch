
#include <windows.h>
#include "renderer/vk_renderer.cpp"

static bool running = true;
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

bool platform_create_window(HWND* outWindow)
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

    *outWindow =
        CreateWindowExA(WS_EX_APPWINDOW, "vulkan_engine_class", "Pong Game",
                        WS_CAPTION | WS_MINIMIZEBOX | WS_SYSMENU | WS_MAXIMIZEBOX | WS_OVERLAPPED,
                        100, 100, 799, 600, 0, 0, hInstance, 0);

    if (outWindow == nullptr)
    {
        MessageBoxA(nullptr, "Failed to create window!", "Error", MB_ICONERROR | MB_OK);
        return false;
    }

    ShowWindow(*outWindow, SW_SHOW);
    return true;
}

void platform_update_window(HWND* window)
{
    MSG msg;
    while (PeekMessageA(&msg, *window, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
}

int main()
{
    HWND hwnd = 0;
    VkContext vkContext = {};
    if (!platform_create_window(&hwnd))
    {
        return -2;
    }
    if (!vk_init(&vkContext, hwnd))
    {
        return -3;
    }

    while (running)
    {
        platform_update_window(&hwnd);
        vk_render(&vkContext);
    }
    return 0;
}