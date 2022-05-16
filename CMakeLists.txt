cmake_minimum_required(VERSION 3.16)
project(obelisk VERSION 1.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

# Optimization and warning flags
add_compile_options(-Wall -Wextra -Wpedantic -O3 -g)

# Platform architecture detection
if(APPLE)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64" OR CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
        add_definitions(-DOBELISK_TARGET_ARM64=1)
    else()
        add_definitions(-DOBELISK_TARGET_X86_64=1)
    endif()
elseif(UNIX)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64" OR CMAKE_SYSTEM_PROCESSOR MATCHES "AMD64")
        add_definitions(-DOBELISK_TARGET_X86_64=1)
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64" OR CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
        add_definitions(-DOBELISK_TARGET_ARM64=1)
    endif()
endif()

include_directories(include)

# Core library sources
set(OBELISK_SOURCES
    src/vm/vm.c
    src/vm/tape.c
    src/compiler/ir.c
    src/compiler/optimizer.c
    src/compiler/parser.c
    src/compiler/codegen.c
    src/jit/jit_arm64.c
    src/jit/jit_x86_64.c
    src/jit/jit_exec.c
    src/aot/aot_transpiler.c
    src/server/server.c
    src/server/session.c
    src/server/ring_buffer.c
)

add_library(obelisk_core STATIC ${OBELISK_SOURCES})

# Main CLI binary
add_executable(obelisk src/main.c)
target_link_libraries(obelisk PRIVATE obelisk_core pthread)

# Unit test binaries
enable_testing()

add_executable(test_vm tests/unit/test_vm.c)
target_link_libraries(test_vm PRIVATE obelisk_core pthread)
add_test(NAME test_vm COMMAND test_vm)

add_executable(test_optimizer tests/unit/test_optimizer.c)
target_link_libraries(test_optimizer PRIVATE obelisk_core pthread)
add_test(NAME test_optimizer COMMAND test_optimizer)

add_executable(test_jit tests/unit/test_jit.c)
target_link_libraries(test_jit PRIVATE obelisk_core pthread)
add_test(NAME test_jit COMMAND test_jit)

add_executable(test_ring_buffer tests/unit/test_ring_buffer.c)
target_link_libraries(test_ring_buffer PRIVATE obelisk_core pthread)
add_test(NAME test_ring_buffer COMMAND test_ring_buffer)

# Benchmark binary
add_executable(bench_vm bench/bench_vm.c)
target_link_libraries(bench_vm PRIVATE obelisk_core pthread)
