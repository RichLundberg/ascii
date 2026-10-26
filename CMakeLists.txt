cmake_minimum_required(VERSION 3.16)

project(ascii_art LANGUAGES CXX)

add_executable(ascii_art main.cpp)

target_include_directories(ascii_art PRIVATE "ext/")
target_compile_features(ascii_art PRIVATE cxx_std_17)