cmake_minimum_required(VERSION 3.19)
project(LearnD3d12 VERSION 0.1.0)

add_subdirectory(thirdparty)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
  message(
    FATAL_ERROR
      "In-source builds not allowed. Please make a new directory (called a build directory) and run CMake from there."
  )
endif()

set(
  learn_d3d12_public_files
)

set(
  learn_d3d12_private_files
    ${CMAKE_CURRENT_SOURCE_DIR}/src/application/application.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/application/application.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/application/glfw_application.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/application/glfw_application.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/application/win32_application.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/application/win32_application.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/logging/log_macros.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/logging/log_manager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/logging/log_manager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/renderer/braynzar_constant_buffers.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/renderer/braynzar_constant_buffers.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/renderer/d3d12_helper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/renderer/d3d12_renderer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/renderer/d3d12_renderer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/renderer/hello_triangle.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/renderer/hello_triangle.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
)


add_executable(LearnD3d12
  ${learn_d3d12_public_files}
  ${learn_d3d12_private_files}
)

target_link_libraries(LearnD3d12
  PRIVATE
    spdlog::spdlog
    cxxopts::cxxopts
    glfw
)

if(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
  target_link_libraries(LearnD3d12
    PRIVATE
      d3d12.lib
      dxgi.lib
      d3dcompiler.lib
      dxguid.lib
      Microsoft::DirectX-Headers
  )
  if(${CMAKE_BUILD_TYPE} STREQUAL "Release")
    target_compile_definitions(LearnD3d12 PRIVATE
      LEARN_D3D12_DIST=1
    )
    set_target_properties(LearnD3d12 PROPERTIES
      WIN32_EXECUTABLE 1)
  endif()
endif()
