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

using namespace llvm;


//----------------------------------------------------------------------
// Expression class node
//----------------------------------------------------------------------

/// ExprAST - Virutal base class for all expression nodes.
class ExprAST {
public:
    virtual ~ExprAST() = default;

    virtual Value *codegen() = 0;
};


/// NumberExprAST - Expression class for numeric literals like "1.0".
class NumberExprAST : public ExprAST {
    double DoubleVal;
public:
    NumberExprAST(double DoubleVal) : DoubleVal(DoubleVal) {}

    Value *codegen() override;
};

/// VariableExprAST - Expression class for referencing a variable, like "a".
class VariableExprAST : public ExprAST {
    std::string Name;

public:
    VariableExprAST(const std::string &Name) : Name(Name) {}

    const std::string &getName() const { return Name; }

    Value *codegen() override;
};


/// BinaryExprAST - Expression class for a binary operator.
class BinaryExprAST : public ExprAST {
    char Op;
    std::unique_ptr<ExprAST> LHS, RHS;

public:
    BinaryExprAST(char Op, std::unique_ptr<ExprAST> LHS,
                  std::unique_ptr<ExprAST> RHS)
            : Op(Op), LHS(std::move(LHS)), RHS(std::move(RHS)) {}

    Value *codegen() override;
};

/// VarDefineExprAST - Expression class for defining a new variable.
class VarDefineExprAST : public ExprAST {
    std::vector<std::pair<std::string, std::unique_ptr<ExprAST>>> Varnames;
public:
    VarDefineExprAST(std::vector<std::pair<std::string, std::unique_ptr<ExprAST>>> Varnames) :
            Varnames(std::move(Varnames)) {}

    Value *codegen() override;
};

/// CallExprAST - Expression class for function calls.
class CallExprAST : public ExprAST {
    std::string Callee;
    std::vector<std::unique_ptr<ExprAST>> Args;
public:
    CallExprAST(const std::string &Callee,
                std::vector<std::unique_ptr<ExprAST>> Args)
            : Callee(Callee), Args(std::move(Args)) {}

    Value *codegen() override;
};

/// PrototypeAST - This class represents the "prototype" for a function,
/// which captures its name, and its argument names (thus implicitly the number
/// of arguments the function takes).
class PrototypeAST {
    std::string Name;
    std::vector<std::string> Args;

public:
    PrototypeAST(const std::string &Name, std::vector<std::string> Args)
            : Name(Name), Args(std::move(Args)) {}

    Function *codegen();

    const std::string &getName() const { return Name; }
};

/// FunctionAST - This class represents a function definition itself.
class FunctionAST {
    std::unique_ptr<PrototypeAST> Proto;
    std::vector<std::unique_ptr<ExprAST>> Body;

public:
    FunctionAST(std::unique_ptr<PrototypeAST> Proto,
                std::vector<std::unique_ptr<ExprAST>> Body)
            : Proto(std::move(Proto)), Body(std::move(Body)) {}

    Function *codegen();
};

///// ReturnAST - This class return a expression or null.
//class ReturnAST{
//    std::unique_ptr<ExprAST> Expr;
//public:
//    ReturnAST(std::unique_ptr<ExprAST> Expr)
//            : Expr(std::move(Expr)) {}
//
//};


/// IfElseAST - Expression for if/else.
class IfElseAST : public ExprAST {
    std::unique_ptr<ExprAST> Cond;
    std::vector<std::unique_ptr<ExprAST>> Then, Else;

public:
    IfElseAST(std::unique_ptr<ExprAST> Cond,
              std::vector<std::unique_ptr<ExprAST>> Then,
              std::vector<std::unique_ptr<ExprAST>> Else)
            : Cond(std::move(Cond)), Then(std::move(Then)), Else(std::move(Else)) {}

    IfElseAST(std::unique_ptr<ExprAST> Cond,
              std::vector<std::unique_ptr<ExprAST>> Then)
            : Cond(std::move(Cond)), Then(std::move(Then)) {}

    Value *codegen() override;
};

/// ForExprAST - Expression class for for/in
class ForExprAST : public ExprAST {
    std::string VarName;
    std::unique_ptr<ExprAST> Start, End, Step;
    std::vector<std::unique_ptr<ExprAST>> Body;

public:
    ForExprAST(const std::string &VarName, std::unique_ptr<ExprAST> Start,
               std::unique_ptr<ExprAST> End, std::unique_ptr<ExprAST> Step,
               std::vector<std::unique_ptr<ExprAST>> Body)
            : VarName(VarName), Start(std::move(Start)), End(std::move(End)),
              Step(std::move(Step)), Body(std::move(Body)) {}

    Value *codegen() override;
};

/// BodyExpr - Expression for a set of expression around by braces.
class BodyExprAST : public ExprAST {
    std::vector<std::unique_ptr<ExprAST>> Body;
public:
    BodyExprAST(std::vector<std::unique_ptr<ExprAST>> Body)
            : Body(std::move(Body)) {}

    Value *codegen() override;
};

/// LogError* - These are little helper functions for error handling.
std::unique_ptr<ExprAST> LogError(const char *Str) {
    fprintf(stderr, "Error: %s\n", Str);
    return nullptr;
}

std::unique_ptr<PrototypeAST> LogErrorP(const char *Str) {
    LogError(Str);
    return nullptr;
}

std::unique_ptr<FunctionAST> LogErrorF(const char *Str) {
    LogError(Str);
    return nullptr;
}
