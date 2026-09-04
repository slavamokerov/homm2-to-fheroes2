cmake_minimum_required(VERSION 3.16)
project(homm2-to-fheroes2 VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

find_package(ZLIB REQUIRED)

add_library(h2core STATIC
    src/homm2_save.cpp
    src/fheroes2_save.cpp
    src/convert.cpp
)
target_include_directories(h2core PUBLIC src)
target_link_libraries(h2core PUBLIC ZLIB::ZLIB)

add_executable(homm2-to-fheroes2 src/main.cpp)
target_link_libraries(homm2-to-fheroes2 PRIVATE h2core)

enable_testing()
add_executable(core_test tests/core_test.cpp)
target_link_libraries(core_test PRIVATE h2core)
target_compile_definitions(core_test PRIVATE
    FIXTURES_DEFAULT_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures")
add_test(NAME core_test COMMAND core_test)
