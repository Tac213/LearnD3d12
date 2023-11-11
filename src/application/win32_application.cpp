#include "win32_application.h"
#include "../renderer/d3d12_renderer.h"
#include <winuser.h>

namespace learn_d3d12
{
    static LRESULT CALLBACK window_proc(HWND hwnd, uint32_t message, WPARAM w_param, LPARAM l_param)
    {
        switch (message)
        {
            case WM_CREATE: {
                // Save the DXSample* passed in to CreateWindow.
                LPCREATESTRUCT create_struct = reinterpret_cast<LPCREATESTRUCT>(l_param);
                SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create_struct->lpCreateParams));
            }
                return 0;
            case WM_DESTROY:
                PostQuitMessage(0);
                return 0;
        }
        // Handle any messages the switch statement didn't.
        return DefWindowProc(hwnd, message, w_param, l_param);
    }

    int Win32Application::exec(std::shared_ptr<D3d12Renderer> renderer)
    {
        HINSTANCE instance = GetModuleHandle(nullptr);
        // Initialize the window class
        WNDCLASSEX window_class = {};
        window_class.cbSize = sizeof(WNDCLASSEX);
        window_class.style = CS_HREDRAW | CS_VREDRAW;
        window_class.lpfnWndProc = window_proc;
        window_class.hInstance = instance;
        window_class.hCursor = LoadCursor(nullptr, IDC_ARROW);
        window_class.lpszClassName = "LearnD3d12Class";
        RegisterClassEx(&window_class);

        RECT window_rect = {0, 0, static_cast<LONG>(renderer->get_width()), static_cast<LONG>(renderer->get_height())};
        AdjustWindowRect(&window_rect, WS_OVERLAPPEDWINDOW, FALSE);
        SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

        // Create the window and store a handle to it.
        _hwnd = CreateWindow(
            window_class.lpszClassName,
            renderer->get_name(),
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            window_rect.right - window_rect.left,
            window_rect.bottom - window_rect.top,
            nullptr,  // We have no parent window.
            nullptr,  // We aren't using menuus.
            instance,
            renderer.get());

        ShowWindow(_hwnd, SW_SHOWDEFAULT);

        // Main loop.
        MSG msg = {};
        while (msg.message != WM_QUIT)
        {
            // Process any messages in the queue
            if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
        return 0;
    }
}  // namespace learn_d3d12
