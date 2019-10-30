#!/bin/bash
clang++ -g -O3 $*.cpp `llvm-config --cxxflags --ldflags --system-libs --libs core` -o $*
./$*