//
// Created by lee on 2019-10-28.
//

#include "Codegen.cpp"
#include "Lexer.cpp"

using namespace llvm;

/// CurTok/getNextToken - Provide a simple token buffer.  CurTok is the current
/// token the parser is looking at.  getNextToken reads another token from the
/// lexer and updates CurTok with its results.
static int CurTok;

static int getNextToken() { return CurTok = gettok(); }


/// BinopPrecedence - This holds the precedence for each binary operator that is
/// defined.
std::map<char, int> BinopPrecedence;

/// GetTokPrecedence - Get the precedence of the pending binary operator token.
int GetTokPrecedence() {
    if (!isascii(CurTok))
        return -1;

    // Make sure it's a declared binop.
    int TokPrec = BinopPrecedence[CurTok];
    if (TokPrec <= 0)
        return -1;
    return TokPrec;
}


std::unique_ptr<ExprAST> ParseExpression();

std::unique_ptr<ExprAST> ParseVarDefineExpr();

/// numberexpr ::= number
std::unique_ptr<ExprAST> ParseNumberExpr() {
    auto Result = llvm::make_unique<NumberExprAST>(NumVal);
    getNextToken(); // eat the number
    return std::move(Result);
}

/// parenexpr ::= '(' expression ')' and eat '(' and ')'
std::unique_ptr<ExprAST> ParseParenExpr() {
    getNextToken(); // eat '('
    auto V = ParseExpression();
    if (!V)
        return nullptr;
    if (CurTok != ')')
        return LogError("expected ')'");
    getNextToken(); // eat ')'
    return V;
}

/// identifierexpr ::=
///     identifier
///   | identifier '(' expression* ')'
std::unique_ptr<ExprAST> ParseIdentifierExpr() {
    if (CurTok == tok_return) getNextToken(); // eat return;
    std::string IdName = IdentifierStr;
    getNextToken(); // eat identifier.

    if (CurTok != '(') { // Simple variable ref.
        return llvm::make_unique<VariableExprAST>(IdName);
    }

    // Call.
    getNextToken(); // eat '('
    std::vector<std::unique_ptr<ExprAST>> Args;
    if (CurTok != ')') {
        while (true) {
            if (auto Arg = ParseExpression())
                Args.push_back(std::move(Arg));
            else
                return nullptr;
            if (CurTok == ')')
                break;
            if (CurTok != ',')
                return LogError("Expected ',' in argument list");
            getNextToken();
        }
    }

    getNextToken(); // eat ')'.
    return llvm::make_unique<CallExprAST>(IdName, std::move(Args));
}

/// BodyExpr ::= '{' (primary expr)* '}'
/// consume a set of expression inside the brace
/// and eat '{' and '}'
std::vector<std::unique_ptr<ExprAST>> ParseBodyExpr() {
    getNextToken(); // eat '{'

    std::vector<std::unique_ptr<ExprAST>> body;
    while (CurTok != '}') {
        auto E = ParseExpression();
        body.push_back(std::move(E));
        if (CurTok == ';')
            getNextToken(); // eat ';'
    }
    getNextToken(); // eat '}'

    return body;

}

/// IfElseexpr ::=
///       if parenexpr bodyexpr (else bodyexpr)*
///     | if parenexpr bodyexpr

std::unique_ptr<ExprAST> ParseIfElseExpr() { ///@todo Add recursive if expr.
    getNextToken(); // eat if

    auto Cond = ParseParenExpr();

    if (!Cond)
        return nullptr;
    if (CurTok != '{')
        return LogError("Expected '{' after if condition");

    auto thenv = ParseBodyExpr();
    if (CurTok == tok_else) {
        getNextToken(); // eat else
        auto elsev = ParseBodyExpr();
        return llvm::make_unique<IfElseAST>(std::move(Cond), std::move(thenv), std::move(elsev));
    } else
        return llvm::make_unique<IfElseAST>(std::move(Cond), std::move(thenv));
}

/// Forexpr ::=
///         for identifier in (start, end, step) bodyexpr
std::unique_ptr<ExprAST> ParseForExpr() {
    getNextToken(); // eat for

    std::string IdName = IdentifierStr;
    getNextToken(); // eat id

    getNextToken(); // eat in
    getNextToken(); // eat '('

    auto start = ParseExpression();
    if(!start)
        return nullptr;
    getNextToken(); // eat ','

    auto end = ParseExpression();
    if(!end)
        return nullptr;
    std::unique_ptr<ExprAST> step = nullptr;
    if (CurTok == ',') {
        getNextToken(); // eat ','
        step = ParseExpression();
        if (!step)
            return nullptr;
    }
    getNextToken(); // eat ')'

    auto body = ParseBodyExpr();
    return llvm::make_unique<ForExprAST>(IdName, std::move(start), std::move(end),
                                         std::move(step), std::move(body));

}
/// primary ::=
///     identifierexpr
///   | numberexpr
///   | parenexpr

std::unique_ptr<ExprAST> ParsePrimary() {
    switch (CurTok) {
        default:

            return LogError("unknown token when expecting an expression");
        case tok_identifier:
            return ParseIdentifierExpr();
        case tok_number:
            return ParseNumberExpr();
        case '(':
            return ParseParenExpr();
        case tok_return:
            return ParseIdentifierExpr();
        case tok_var:
            return ParseVarDefineExpr();
        case tok_if:
            return ParseIfElseExpr();
        case tok_for:
            return ParseForExpr();
    }
}

/// binoprhs ::= ('+' primary)*
std::unique_ptr<ExprAST> ParseBinOpRHS(int ExprPrec,
                                       std::unique_ptr<ExprAST> LHS) {
    // If this is a binop, find its precedence.
    while (true) {
        int TokPrec = GetTokPrecedence();

        // If this is a binop that binds at least as tightly as the current binop,
        // consume it, otherwise we are done. e.g. if this expression is a signle identifier,
        //  then the ExprPrec equals to 0, and TokPrec now is -1, so return LHS.
        if (TokPrec < ExprPrec)
            return LHS;

        // Okay, we know this is a binop.
        int BinOp = CurTok;

        getNextToken(); // eat binop

        // Parse the primary expression after the binary operator.
        auto RHS = ParsePrimary();
        if (!RHS)
            return nullptr;

        // If BinOp binds less tightly with RHS than the operator after RHS, let
        // the pending operator take RHS as its LHS.
        if (CurTok != ';') {
            int NextPrec = GetTokPrecedence();
            if (TokPrec < NextPrec) {
                RHS = ParseBinOpRHS(TokPrec + 1, std::move(RHS));
                if (!RHS)
                    return nullptr;
            }
        }
        // Merge LHS/RHS.

        LHS = llvm::make_unique<BinaryExprAST>(BinOp, std::move(LHS), std::move(RHS));
    }
}

/// expression ::= primary binoprhs, not eat ';'
std::unique_ptr<ExprAST> ParseExpression() {

    auto LHS = ParsePrimary();
    if (!LHS)
        return nullptr;
    return ParseBinOpRHS(0, std::move(LHS));
}

/// prototype ::= id '(' id* ')'
std::unique_ptr<PrototypeAST> ParsePrototype() {
    if (CurTok != tok_identifier)
        return LogErrorP("Expected function name in prototype");

    std::string FnName = IdentifierStr; //get func name
    getNextToken();

    if (CurTok != '(')
        return LogErrorP("Expected '(' in prototype");

    std::vector<std::string> ArgNames;
    getNextToken();
    while (CurTok == tok_identifier) {
        ArgNames.push_back(IdentifierStr);
        getNextToken(); // eat 'IdentifierStr'
        if (CurTok == ')') break;
        getNextToken(); // eat ','
    }
    if (CurTok != ')')
        return LogErrorP("Expected ')' in prototype");
    // success.
    getNextToken(); // eat ')'.
    return llvm::make_unique<PrototypeAST>(FnName, std::move(ArgNames));
}

/// function definition ::= 'def' prototype expression
std::unique_ptr<FunctionAST> ParseDefinition() {
    getNextToken(); // eat def.
    auto Proto = ParsePrototype();
    if (!Proto)
        return nullptr;
    if (CurTok != '{') {
        return LogErrorF("Expected '{' in prototype");
    }
    auto E = ParseBodyExpr();
    return llvm::make_unique<FunctionAST>(std::move(Proto), std::move(E));
}

/// toplevelexpr ::= expression
std::unique_ptr<FunctionAST> ParseTopLevelExpr() {
    if (auto E = ParseExpression()) {
        // Make an anonymous proto.
        auto Proto = llvm::make_unique<PrototypeAST>("__anon_expr",
                                                     std::vector<std::string>());
        std::vector<std::unique_ptr<ExprAST>> ExprList;
        ExprList.push_back(std::move(E));
        return llvm::make_unique<FunctionAST>(std::move(Proto), std::move(ExprList));
    }
    return nullptr;
}

/// externalexpr ::= 'extern' prototype
std::unique_ptr<PrototypeAST> ParseExtern() {
    getNextToken(); // eat extern.
    return ParsePrototype();
}

/// VarDefineexpr  ::= var Identifer '=' expression
std::unique_ptr<ExprAST> ParseVarDefineExpr() {
    getNextToken(); // eat 'var'
    std::vector<std::pair<std::string, std::unique_ptr<ExprAST>>> VarNames;
    if (CurTok != tok_identifier)
        return LogError("Expected identifier when define a new variable");

    std::string Name = IdentifierStr;
    getNextToken(); // eat IdentifierStr
    std::unique_ptr<ExprAST> Init = nullptr;

    if (CurTok == '=') {
        getNextToken(); // eat '='
        Init = ParseExpression();
        if (!Init)
            return nullptr;
    }
    VarNames.push_back(std::make_pair(Name, std::move(Init)));

    if (CurTok != ';')
        return LogError("Expected ';' for end the var definition ");

    return llvm::make_unique<VarDefineExprAST>(std::move(VarNames));
}

//===----------------------------------------------------------------------===//
// Top-Level parsing
//===----------------------------------------------------------------------===//

static void InitializeModuleAndPassManager() {
    // Open a new module.
    TheModule = llvm::make_unique<Module>("my cool jit", TheContext);
    TheModule->setDataLayout(TheJIT->getTargetMachine().createDataLayout());

    // Create a new pass manager attached to it.
    TheFPM = llvm::make_unique<legacy::FunctionPassManager>(TheModule.get());

    // Do simple "peephole" optimizations and bit-twiddling optzns.
    TheFPM->add(createInstructionCombiningPass());
    // Reassociate expressions.
    TheFPM->add(createReassociatePass());
    // Eliminate Common SubExpressions.
    TheFPM->add(createGVNPass());
    // Simplify the control flow graph (deleting unreachable blocks, etc).
    TheFPM->add(createCFGSimplificationPass());

    TheFPM->doInitialization();
}

void HandleDefinition() {

    if (auto FnAST = ParseDefinition()) {

        if (auto *FnIR = FnAST->codegen()) {

            fprintf(stderr, "Read function definition:");
            FnIR->print(errs());
            fprintf(stderr, "\n");
            TheJIT->addModule(std::move(TheModule));
            InitializeModuleAndPassManager();
        }
    } else {
        // Skip token for error recovery.
        getNextToken();
    }
}

void HandleExtern() {
    if (auto ProtoAST = ParseExtern()) {
        if (auto *FnIR = ProtoAST->codegen()) {
            fprintf(stderr, "Read extern: ");
            FnIR->print(errs());
            fprintf(stderr, "\n");
        }
    } else {
        // Skip token for error recovery.
        getNextToken();
    }
}

void HandleTopLevelExpression() {
    // Evaluate a top-level expression into an anonymous function.
    if (auto FnAST = ParseTopLevelExpr()) {
        if (FnAST->codegen()) {
            // JIT the module containing the anonymous expression, keeping a handle so
            // we can free it later.
            auto H = TheJIT->addModule(std::move(TheModule));
            InitializeModuleAndPassManager();

            // Search the JIT for the __anon_expr symbol.
            auto ExprSymbol = TheJIT->findSymbol("__anon_expr");
            assert(ExprSymbol && "Function not found");

            // Get the symbol's address and cast it to the right type (takes no
            // arguments, returns a double) so we can call it as a native function.
            double (*FP)() = (double (*)()) (intptr_t) cantFail(ExprSymbol.getAddress());
            fprintf(stderr, "%f\n", FP());

            // Delete the anonymous expression module from the JIT.
            TheJIT->removeModule(H);
        }
    } else {
        // Skip token for error recovery.
        getNextToken();
    }
}

/// top ::= definition | external | expression | ';'
void MainLoop() {
    while (true) {
        fprintf(stderr, ">>> ");
        switch (CurTok) {
            case tok_eof:
                return;
            case ';': // ignore top-level semicolons.
                getNextToken();
                break;
            case tok_def:
                HandleDefinition();
                break;
            case tok_extern:
                HandleExtern();
                break;
            default:
                HandleTopLevelExpression();
                break;
        }
    }
}

#ifdef _WIN32
#define DLLEXPORT __declspec(dllexport)
#else
#define DLLEXPORT
#endif

/// putchard - putchar that takes a double and returns 0.
extern "C" DLLEXPORT double putchard(double X) {
    fputc((char) X, stderr);
    return 0;
}

/// printd - printf that takes a double prints it as "%f\n", returning 0.
extern "C" DLLEXPORT double printd(double X) {
    fprintf(stderr, "%f\n", X);
    return 0;
}