#pragma once

#include <cstdint>
#include <string>

namespace learn_d3d12
{
    class D3d12Renderer
    {
    public:
        explicit D3d12Renderer(uint32_t width, uint32_t height, std::string name)
            : width(width)
            , height(height)
            , name(name)
        {}
        virtual ~D3d12Renderer() = default;

        // Accessors
        uint32_t get_width() const { return width; }
        uint32_t get_height() const { return height; }
        const char* get_name() const { return name.c_str(); }

    protected:
        uint32_t width;
        uint32_t height;
        std::string name;
    };
}  // namespace learn_d3d12
