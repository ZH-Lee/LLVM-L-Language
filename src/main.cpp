////===----------------------------------------------------------------------===//
//// Main driver code.
////===----------------------------------------------------------------------===//
//#include "llvm/ADT/STLExtras.h"
//#include <algorithm>
//#include <cctype>
//#include <cstdio>
//#include <cstdlib>
//#include <map>
//#include <unordered_map>
//#include <memory>
//#include <string>
//#include <vector>
//#include "Parser.cpp"
//#include <stdio.h>
//
//using namespace llvm;
//int main() {
//    // Install standard binary operators.
//    // 1 is lowest precedence.
//    BinopPrecedence['='] = 5;
//    BinopPrecedence['<'] = 20;
//    BinopPrecedence['+'] = 30;
//    BinopPrecedence['-'] = 30;
//    BinopPrecedence['*'] = 40; // highest.
//    BinopPrecedence['/'] = 40;
//
//    // Prime the first token.
//    fprintf(stderr, ">>> ");
//
//    getNextToken();
//    InitializeModuleAndPassManager();
//
//    // Run the main "interpreter loop" now.
//    MainLoop();
//
//    InitializeAllTargetInfos();
//    InitializeAllTargets();
//    InitializeAllTargetMCs();
//    InitializeAllAsmParsers();
//    InitializeAllAsmPrinters();
//
//    auto TargetTriple = sys::getDefaultTargetTriple();
//    TheModule->setTargetTriple(TargetTriple);
//
//    std::string Error;
//    auto Target = TargetRegistry::lookupTarget(TargetTriple, Error);
//
//    // Print an error and exit if we couldn't find the requested target.
//    // This generally occurs if we've forgotten to initialise the
//    // TargetRegistry or we have a bogus target triple.
//    if (!Target) {
//        errs() << Error;
//        return 1;
//    }
//
//    auto CPU = "generic";
//    auto Features = "";
//
//    TargetOptions opt;
//    auto RM = Optional<Reloc::Model>();
//    auto TheTargetMachine =
//            Target->createTargetMachine(TargetTriple, CPU, Features, opt, RM);
//
//    TheModule->setDataLayout(TheTargetMachine->createDataLayout());
//
//    auto Filename = "output.o";
//    std::error_code EC;
//    raw_fd_ostream dest(Filename, EC, sys::fs::F_None);
//
//    if (EC) {
//        errs() << "Could not open file: " << EC.message();
//        return 1;
//    }
//
//    legacy::PassManager pass;
//    auto FileType = TargetMachine::CGFT_ObjectFile;
//
//    if (TheTargetMachine->addPassesToEmitFile(pass, dest, nullptr, FileType)) {
//        errs() << "TheTargetMachine can't emit a file of this type";
//        return 1;
//    }
//
//    pass.run(*TheModule);
//    dest.flush();
//
//    outs() << "Wrote " << Filename << "\n";
//
//    return 0;
//}

#include <iostream>

using namespace std;
int main () {
    char buffer[256];
    ifstream in("source_code.txt");
    if (! in.is_open())
    { cout << "Error opening file"; exit (1); }
    while (!in.eof() )
    {
        in.getline (buffer,100);
        cout << buffer << endl;
    }
    return 0;
}