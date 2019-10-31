cmake_minimum_required(VERSION 3.13)
project(easy)

set(CMAKE_CXX_STANDARD 14)

add_executable(Easy-Language main.cpp Lexer.cpp AST.cpp Parser.cpp src/t.cpp)