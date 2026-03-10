cmake_minimum_required(VERSION 3.31)
project(FirefighterGame)

set(CMAKE_CXX_STANDARD 20)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/CPM.cmake)

set(CPM_SOURCE_CACHE "${CMAKE_CURRENT_SOURCE_DIR}/packages")

CPMAddPackage("gh:libsdl-org/SDL#release-3.4.2")
CPMAddPackage("gh:libsdl-org/SDL_image#release-3.4.0")
CPMAddPackage("gh:libsdl-org/SDL_ttf#release-3.2.2")
CPMAddPackage("gh:g-truc/glm#1.0.1")

add_executable(FirefighterGame main.cpp)

target_link_libraries(FirefighterGame PRIVATE SDL3::SDL3 SDL3_image::SDL3_image SDL3_ttf::SDL3_ttf glm::glm)
