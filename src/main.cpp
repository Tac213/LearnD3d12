#include "application/application.h"
#include "logging/log_manager.h"
#include "renderer/d3d12_renderer.h"
#include <cxxopts.hpp>
#include <format>
#include <iostream>
#include <memory>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX  // Avoid compile error
#endif
#include <windows.h>
#endif

#ifdef LEARN_D3D12_DIST
_Use_decl_annotations_ int WINAPI WinMain(HINSTANCE instance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)  // NOLINT(readability-identifier-naming)
#else
int main(int argc, char** argv)
#endif
{
    // Parse arguments
#ifdef LEARN_D3D12_DIST
    int argc = __argc;
    char** argv = __argv;
#endif
    cxxopts::Options options("LearnD3d12", "A D3D12 learning program.");
    // clang-format off
    options.add_options()
        ("p,platform", "Application platform, win32 or glfw.", cxxopts::value<std::string>()->default_value("glfw"))
        ("v,variant", "Renderer variant.", cxxopts::value<std::string>()->default_value("HelloTriangle"));
    // clang-format on
    cxxopts::ParseResult result;
    try
    {
        result = options.parse(argc, argv);
    }
    catch (const cxxopts::exceptions::parsing& e)
    {
        std::cerr << "LearnD3d12: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    learn_d3d12::LogManager::get_instance().initialize();
    auto variant = result["variant"].as<std::string>();
    auto platform = result["platform"].as<std::string>();
    auto renderer = learn_d3d12::D3d12Renderer::create(variant, 1600, 900, std::format("[{}] Learn D3D12: {}", platform, variant));
    if (!renderer)
    {
        return EXIT_FAILURE;
    }
    auto app = learn_d3d12::Application::create(platform);
    auto return_code = app->exec(renderer);
    renderer.reset();
    app.reset();
    learn_d3d12::LogManager::get_instance().finalize();
    return return_code;
}
