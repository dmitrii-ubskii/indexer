add_executable(tests
    dummy.cpp
)

target_include_directories(tests SYSTEM
    PRIVATE
        ../3rdparty/Catch2/src
)
target_include_directories(tests
    PRIVATE
        .
        ../include
)
target_link_libraries(tests PRIVATE Catch2::Catch2WithMain)
