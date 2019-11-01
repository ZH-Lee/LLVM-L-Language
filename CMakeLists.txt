cmake_minimum_required(VERSION 3.13)
project(LLVM-L-Language)

set(CMAKE_CXX_STANDARD 14)

add_executable(LLVM-L-Language src/main.cpp src/Lexer.cpp src/AST.cpp src/Parser.cpp src/Codegen.cpp)