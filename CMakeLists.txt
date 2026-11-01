cmake_minimum_required(VERSION 3.16)
project(UnifiedEngineeringWorkbench VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Optimization and warning flags
if(MSVC)
    add_compile_options(/W4 /WX- /EHsc /O2 /Oi /Ot /utf-8)
    # Statically link C/C++ runtime so Launch.exe has zero missing DLL dependencies
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
else()
    add_compile_options(-Wall -Wextra -O3 -pthread)
endif()

# Core static library containing all numerical and domain modules
add_library(workbench_core STATIC
    src/parser/stl_parser.cpp
    src/parser/ansys_parser.cpp
    src/fea/fea_solver.cpp
    src/multicore/batch_solver.cpp
    src/embedded/embedded_mock.cpp
    src/backprop/backpropagation.cpp
)

target_include_directories(workbench_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

if(WIN32)
    target_link_libraries(workbench_core PUBLIC ws2_32)
endif()

# Unit and verification test executable
add_executable(run_tests
    tests/main_tests.cpp
)
target_link_libraries(run_tests PRIVATE workbench_core)

# Standalone Application and Server executable (Launch.exe target)
add_executable(Launch
    src/server/main_server.cpp
)
target_link_libraries(Launch PRIVATE workbench_core)
if(WIN32)
    target_link_libraries(Launch PRIVATE ws2_32 shell32)
endif()
