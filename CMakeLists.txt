cmake_minimum_required(VERSION 3.16)

project(indexer VERSION 0.0.0
    DESCRIPTION "File indexing library and REPL"
    LANGUAGES CXX)

### Require out-of-source builds
file(TO_CMAKE_PATH "${PROJECT_BINARY_DIR}/CMakeLists.txt" LOC_PATH)
if(EXISTS "${LOC_PATH}")
    message(FATAL_ERROR "You cannot build in a source directory (or any directory with a CMakeLists.txt file). Please make a build subdirectory. Feel free to remove CMakeCache.txt and CMakeFiles.")
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS 1)

add_subdirectory(3rdparty/Catch2)

set(default_build_type "Release")
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS "Setting build type to '${default_build_type}' as none was specified.")
    set(CMAKE_BUILD_TYPE "${default_build_type}" CACHE
        STRING "Choose the type of build." FORCE)
    # Set the possible values of build type for cmake-gui
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
        "Debug" "Profile" "Release")
endif()

add_compile_options(
        -Wall -Wextra -Wshadow -Wnon-virtual-dtor -Wold-style-cast -Wcast-align -Wunused
        -Woverloaded-virtual -Wpedantic -Wconversion -Wsign-conversion
        -Wmisleading-indentation -Wnull-dereference -Wdouble-promotion -Wformat=2
        -Wduplicated-cond -Wduplicated-branches -Wlogical-op -Wuseless-cast)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/lib)

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_compile_options(-s)
elseif(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-Og -ggdb3 -fsanitize=undefined)
    add_link_options(-fsanitize=undefined)
elseif(CMAKE_BUILD_TYPE STREQUAL "Profile")
    add_compile_options(-g -pg -Og)
    add_link_options(-g -pg -Og)
endif()

add_subdirectory(src)
add_subdirectory(test)


