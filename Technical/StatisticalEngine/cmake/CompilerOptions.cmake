# Compiler warnings and optimization flags

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(
        -Wall -Wextra -Wpedantic
        -Wno-unused-parameter
        -Wno-missing-field-initializers
        -Wa,-mbig-obj  # MinGW: allow large object files (Crow + nlohmann headers)
    )
    # Release optimization
    set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")
    set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")
elseif(MSVC)
    add_compile_options(/W4 /wd4100 /wd4458 /bigobj)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS _WIN32_WINNT=0x0601)
endif()

# Use M_PI etc. on Windows
if(WIN32)
    add_compile_definitions(_USE_MATH_DEFINES)
endif()
