//
// Created by lee on 2019-10-28.
//

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
#include <string>
#include <system_error>
#include <utility>
#include <vector>
#include "AST.cpp"
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
std::unique_ptr<ExprAST> ParseVarDefine();

/// numberexpr --> number
std::unique_ptr<ExprAST> ParseNumberExpr() {
    auto Result = llvm::make_unique<NumberExprAST>(NumVal);
    getNextToken(); // eat the number
    return std::move(Result);
}

/// parenexpr --> '(' expression ')'
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

/// identifierexpr -->
///     identifier
///   | identifier '(' expression* ')'
std::unique_ptr<ExprAST> ParseIdentifierExpr() {
    if(CurTok == tok_return) getNextToken(); // eat return;
    std::string IdName = IdentifierStr;
    getNextToken(); // eat identifier.

    if (CurTok != '('){ // Simple variable ref.
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

/// primary -->
///   | identifierexpr
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
        case tok_double:
            return ParseVarDefineExpr();
    }
}

/// binoprhs --> ('+' primary)*
/**
 *
 * @param ExprPrec
 * @param LHS
 * @return
 */
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
        if(CurTok != ';'){
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

/// expression
///   ::= primary binoprhs
std::unique_ptr<ExprAST> ParseExpression() {
    // This function will eat a ';' after expression.
    auto LHS = ParsePrimary();
    if (!LHS)
        return nullptr;
    return ParseBinOpRHS(0, std::move(LHS));
}

/// prototype
///   ::= id '(' id* ')'
std::unique_ptr<PrototypeAST> ParsePrototype() {
    if (CurTok != tok_identifier)
        return LogErrorP("Expected function name in prototype");

    std::string FnName = IdentifierStr; //get func name
    getNextToken();

    if (CurTok != '(')
        return LogErrorP("Expected '(' in prototype");

    std::vector<std::string> ArgNames;
    getNextToken();
    while (CurTok == tok_identifier){
        ArgNames.push_back(IdentifierStr);
        getNextToken(); // eat 'IdentifierStr'
        if(CurTok==')') break;
        getNextToken(); // eat ','
    }
    if (CurTok != ')')
        return LogErrorP("Expected ')' in prototype");
    // success.
    getNextToken(); // eat ')'.
    return llvm::make_unique<PrototypeAST>(FnName, std::move(ArgNames));
}

/// definition ::= 'def' prototype expression
std::unique_ptr<FunctionAST> ParseDefinition() {
    getNextToken(); // eat def.
    auto Proto = ParsePrototype();
    if (!Proto)
        return nullptr;
    if(CurTok != '{'){
        return LogErrorF("Expected '{' in prototype");
    }
    getNextToken(); // eat '{'
    std::vector<std::unique_ptr<ExprAST>> ExprList;
    while(true){
        auto E = ParseExpression();
        if(!E) return nullptr;
        ExprList.push_back(std::move(E));
        if (CurTok == ';' ) getNextToken(); // eat ';'
        if(CurTok == '}'){
            if(CurTok != '}'){
                return LogErrorF("Expected '}' in prototype");
            }
            getNextToken(); // eat '}'
            return llvm::make_unique<FunctionAST>(std::move(Proto), std::move(ExprList));
        }
    }
    return nullptr;
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

/// external ::= 'extern' prototype
std::unique_ptr<PrototypeAST> ParseExtern() {
    getNextToken(); // eat extern.
    return ParsePrototype();
}

std::unique_ptr<ExprAST> ParseVarDefineExpr(){
    getNextToken(); // eat 'double'
    std::vector<std::pair<std::string, std::unique_ptr<ExprAST>>> VarNames;
    if(CurTok != tok_identifier)
        return LogError("Expected identifier when define a new variable");

    std::string Name = IdentifierStr;
//    if(VarTable.count(IdentifierStr) != 0)
//        return LogError("Redefinition of var");
//    VarTable[IdentifierStr] = true;

    getNextToken(); // eat IdentifierStr
    std::unique_ptr<ExprAST> Init = nullptr;

    if(CurTok == '='){
        getNextToken(); // eat '='
        Init = ParseExpression();
        if(!Init)
            return nullptr;
    }
    VarNames.push_back(std::make_pair(Name, std::move(Init)));

    if(CurTok != ';')
        return LogError("Expected ';' for end the var definition ");

    return llvm::make_unique<VarDefineExprAST> (std::move(VarNames));
}


//===----------------------------------------------------------------------===//
// Top-Level parsing
//===----------------------------------------------------------------------===//


static void InitializeModuleAndPassManager() {
    // Open a new module.
    TheModule = llvm::make_unique<Module>("my cool jit", TheContext);
}

void HandleDefinition() {
    if (auto FnAST = ParseDefinition()) {
        if (auto *FnIR = FnAST->codegen()) {
            fprintf(stderr, "Read function definition:");
            FnIR->print(errs());
            fprintf(stderr, "\n");
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
        if (auto *FnIR = FnAST->codegen()) {
            fprintf(stderr, "Read top-level expression:");
            FnIR->print(errs());
            fprintf(stderr, "\n");
        }
    } else {
        // Skip token for error recovery.
        getNextToken();
    }
}

/// top ::= definition | external | expression | ';'
void MainLoop() {
    while (true) {
        fprintf(stderr, "ready> ");
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

