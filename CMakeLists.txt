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
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
)


add_executable(LearnD3d12
  ${learn_d3d12_public_files}
  ${learn_d3d12_private_files}
)

target_link_libraries(LearnD3d12
  PRIVATE
    spdlog::spdlog
    glfw
)
