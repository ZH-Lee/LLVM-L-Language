clang++ -g main.cpp `llvm-config --cxxflags --ldflags --system-libs --libs core mcjit native all` -O3 -o main

./main
