//===----------------------------------------------------------------------===//
// Main driver code.
//===----------------------------------------------------------------------===//
#include "Parser.cpp"

using namespace llvm;
int main() {
    InitializeNativeTarget();
    InitializeNativeTargetAsmPrinter();
    InitializeNativeTargetAsmParser();
    // Install standard binary operators.
    // 1 is lowest precedence.
    BinopPrecedence['='] = 1;
    BinopPrecedence['<'] = 2;
    BinopPrecedence['>'] = 2;
    BinopPrecedence['+'] = 3;
    BinopPrecedence['-'] = 3;
    BinopPrecedence['*'] = 4; // highest.
    BinopPrecedence['/'] = 4;

    // Prime the first token.
    fprintf(stderr, "Welcome to use L language on LLVM\n");
    fprintf(stderr, "L 1.0 (Oct 27 2019) \n");
    fprintf(stderr, "Want more information, please click: \n");
    fprintf(stderr, "https://github.com/ZH-Lee/LLVM-L-Language\n");

    fprintf(stderr, ">>> ");

    getNextToken();
    TheJIT = llvm::make_unique<llvm::orc::KaleidoscopeJIT>();
    InitializeModuleAndPassManager();

    // Run the main "interpreter loop" now.
    MainLoop();

    return 0;
}