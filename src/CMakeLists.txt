add_library(indexer SHARED
    indexer.cpp
)
target_compile_features(indexer PRIVATE cxx_std_20)
target_include_directories(indexer
    PRIVATE
        .
        ../include
)

if(MSVC)
    set_property(TARGET indexer APPEND PROPERTY SOURCES windows_filesystem_watcher.cpp)
else()
    set_property(TARGET indexer APPEND PROPERTY SOURCES inotify_filesystem_watcher.cpp)
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
