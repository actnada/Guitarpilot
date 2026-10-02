cmake_minimum_required(VERSION 3.22.1)
project(GuitarPilotNative)

add_library(guitarpilot SHARED GuitarEngine.cpp native-lib.cpp)

find_package(oboe REQUIRED CONFIG)

target_compile_features(guitarpilot PRIVATE cxx_std_17)
target_compile_options(guitarpilot PRIVATE -O3)

target_link_libraries(guitarpilot oboe::oboe log android)
