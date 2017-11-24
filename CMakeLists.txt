cmake_minimum_required(VERSION 3.8)
project(LangTest)

set(CMAKE_CXX_STANDARD 14)

add_executable(vm vm.cpp)
add_executable(asm asm.cpp)
add_executable(badlang badlang.cpp)