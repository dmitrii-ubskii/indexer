add_executable(repl repl.cpp)
target_compile_features(repl PRIVATE cxx_std_20)
target_include_directories(repl
    PRIVATE
        .
        ../include
)
target_link_libraries(repl pthread)
