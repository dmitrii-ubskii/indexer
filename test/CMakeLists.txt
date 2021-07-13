add_executable(tests
    basic.cpp
    filesystem_watch.cpp
)
target_compile_features(tests PRIVATE cxx_std_20)

target_include_directories(tests SYSTEM
    PRIVATE
        ../3rdparty/Catch2/src
)
target_include_directories(tests
    PRIVATE
        .
        ../include
)
target_link_libraries(tests PRIVATE Catch2::Catch2WithMain indexer)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(tests PRIVATE --coverage)
    target_link_libraries(tests PRIVATE gcov)
endif()

