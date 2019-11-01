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
    fprintf(stderr, ">>> ");

    getNextToken();
    TheJIT = llvm::make_unique<llvm::orc::KaleidoscopeJIT>();
    InitializeModuleAndPassManager();

    // Run the main "interpreter loop" now.
    MainLoop();

    return 0;
}