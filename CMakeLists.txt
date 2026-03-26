cmake_minimum_required(VERSION 3.31)
project(FirefighterGame)

set(CMAKE_CXX_STANDARD 20)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/CPM.cmake)

set(CPM_SOURCE_CACHE "${CMAKE_CURRENT_SOURCE_DIR}/packages")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

set(BUILD_SHARED_LIBS OFF)
set(SDL_SHARED OFF)
set(SDL_STATIC ON)
set(SDL_AUDIO OFF)
set(SDL_GPU OFF)
set(SDL_CAMERA OFF)
set(SDL_HAPTIC OFF)
set(SDL_POWER OFF)
set(SDL_SENSOR OFF)
set(SDL_DIALOG OFF)
set(SDL_TRAY OFF)

CPMAddPackage("gh:libsdl-org/SDL#release-3.4.2")
CPMAddPackage("gh:libsdl-org/SDL_mixer#release-3.2.0")
CPMAddPackage("gh:g-truc/glm#1.0.1")

add_executable(FirefighterGame main.cpp)

target_link_libraries(FirefighterGame PRIVATE SDL3::SDL3 SDL3_mixer::SDL3_mixer glm::glm)
target_precompile_headers(FirefighterGame PRIVATE <glm/glm.hpp>)
