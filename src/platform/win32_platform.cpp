#include "renderer/vk_renderer.cpp"
#include <windows.h>

static bool running = true;
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CLOSE:
        running = false;
        break;
    }
    return DefWindowProcA(hwnd, uMsg, wParam, lParam);
}

bool platform_create_window(HWND& outWindow) {
    HINSTANCE hInstance = GetModuleHandleA(0);
    WNDCLASSA wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = "vulkan_engine_class";

    if (!RegisterClassA(&wc)) {
        MessageBoxA(nullptr, "Failed to register window class!", "Error", MB_ICONERROR | MB_OK);
        return false;
    }

    outWindow =
        CreateWindowExA(WS_EX_APPWINDOW, "vulkan_engine_class", "Pong Game",
                        WS_CAPTION | WS_MINIMIZEBOX | WS_SYSMENU | WS_MAXIMIZEBOX | WS_OVERLAPPED,
                        100, 100, 799, 600, 0, 0, hInstance, 0);

    if (outWindow == nullptr) {
        MessageBoxA(nullptr, "Failed to create window!", "Error", MB_ICONERROR | MB_OK);
        return false;
    }

    ShowWindow(outWindow, SW_SHOW);
    return true;
}

int main() {
    HWND hwnd;
    VkContext vkContext = {};
    if (!platform_create_window(hwnd)) {
        return -2;
    }
    if (!initWin32VkInstance(&vkContext)) {
        return -3;
    }
    while (running) {
        MSG msg;
        while (PeekMessageA(&msg, hwnd, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
    }
    return 0;
}