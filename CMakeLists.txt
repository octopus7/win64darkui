cmake_minimum_required(VERSION 3.24)

project(win64darkui VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(win64darkui WIN32
  src/layout.cpp
  src/main.cpp
)

target_include_directories(win64darkui PRIVATE src)

target_compile_definitions(win64darkui PRIVATE
  UNICODE
  _UNICODE
  WIN32_LEAN_AND_MEAN
  NOMINMAX
  WINVER=0x0A00
  _WIN32_WINNT=0x0A00
)

if(MSVC)
  target_compile_options(win64darkui PRIVATE /W4 /permissive- /EHsc /Zc:__cplusplus)
else()
  target_compile_options(win64darkui PRIVATE -Wall -Wextra -Wpedantic)
endif()

target_link_libraries(win64darkui PRIVATE
  d2d1
  dwrite
  windowscodecs
)
