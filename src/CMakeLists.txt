find_package(Qt5 5.7.0 REQUIRED COMPONENTS Core)
set(CMAKE_AUTOMOC ON)

add_library(indexer SHARED
    indexer.cpp
    filesystem_watcher.cpp
    filesystem_watcher_impl.h
)
target_compile_features(indexer PRIVATE cxx_std_20)
target_include_directories(indexer
    PRIVATE
        .
        ../include
)

target_link_libraries(indexer PUBLIC Qt5::Core)

if(NOT MSVC)
    target_link_libraries(indexer PUBLIC pthread)

    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_compile_options(indexer PRIVATE --coverage)
        target_link_libraries(indexer PRIVATE gcov)
    endif()
endif()

add_executable(repl repl.cpp)
target_compile_features(repl PRIVATE cxx_std_20)
target_include_directories(repl
    PRIVATE
        .
        ../include
)
target_link_libraries(repl indexer)
