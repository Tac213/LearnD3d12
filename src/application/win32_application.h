#pragma once

#include "application.h"
#ifndef NOMINMAX
#define NOMINMAX  // Avoid compile error
#endif
#include <windows.h>

namespace learn_d3d12
{
    class Win32Application : public Application
    {
    public:
        virtual ~Win32Application() = default;
        virtual int exec(std::shared_ptr<D3d12Renderer> renderer) override;

    private:
        HWND _hwnd;
    };
}  // namespace learn_d3d12
