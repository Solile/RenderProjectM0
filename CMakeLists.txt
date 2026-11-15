cmake_minimum_required(VERSION 3.25)
project(m0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

include(cmake/AgilitySDK.cmake)

add_executable(m0
  src/app/main.cpp
  src/platform/window.cpp
  src/rhi/device.cpp
  src/rhi/barriers.cpp
)

target_include_directories(m0 PRIVATE src)
target_compile_definitions(m0 PRIVATE
  UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX
  WINVER=0x0A00 _WIN32_WINNT=0x0A00
  M0_AGILITY_SDK_VERSION=${AGILITY_SDK_VERSION}
)
target_compile_options(m0 PRIVATE -Wall -Wextra)
# Static runtime: the exe runs without the MSYS2 DLLs on PATH.
target_link_options(m0 PRIVATE -static)
target_link_libraries(m0 PRIVATE d3d12 dxgi dxguid)

agility_sdk_deploy(m0)
