#!/bin/bash
clang++ -O3 -c $(llvm-config --cxxflags) $*.cpp -o $*.o
clang++ $*.o $(llvm-config --ldflags --libs) -lpthread -lncurses
./a.out
