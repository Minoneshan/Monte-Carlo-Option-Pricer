cmake_minimum_required(VERSION 3.16)
project(MonteCarloOptionPricer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(OpenMP REQUIRED)

add_library(montecarlo STATIC
    src/monte_carlo_pricer.cpp
    src/black_scholes.cpp
    src/surface_recalibrator.cpp
)

target_include_directories(montecarlo
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(montecarlo
    PUBLIC
        OpenMP::OpenMP_CXX
)

target_compile_features(montecarlo PUBLIC cxx_std_17)

add_executable(pricer src/main.cpp)
target_link_libraries(pricer PRIVATE montecarlo)

add_executable(pricer_tests tests/test_pricer.cpp)
target_link_libraries(pricer_tests PRIVATE montecarlo)

add_executable(surface_tests tests/test_surface.cpp)
target_link_libraries(surface_tests PRIVATE montecarlo)
