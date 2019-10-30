//
// Created by lee on 2019-10-28.
//
#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/Optional.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Host.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetOptions.h"
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
#include <iostream>
#include <unordered_map>
using namespace llvm;

std::unordered_map<std::string, bool> VarTable;

/// ExprAST - Base class for all expression nodes.
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

/// VariableExprAST - Expression class for referencing a variable, like "a". //变量
class VariableExprAST : public ExprAST {
    std::string Name;

public:
    VariableExprAST(const std::string &Name) : Name(Name){}
    Value *codegen() override;
    const std::string &getName() const { return Name; }
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

class VarDefineExprAST : public ExprAST {
    std::vector<std::pair<std::string, std::unique_ptr<ExprAST>>> Varnames;
public:
    VarDefineExprAST(std::vector<std::pair<std::string, std::unique_ptr<ExprAST>>> Varnames) :
            Varnames(std::move(Varnames)) {}
    Value *codegen() override;
};

/// CallExprAST - Expression class for function calls. 函数调用
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
/// of arguments the function takes). 获取函数的名字，以及参数列表
class PrototypeAST {
    std::string Name;
    std::vector<std::string > Args;

public:
    PrototypeAST(const std::string &Name, std::vector<std::string > Args)
            : Name(Name), Args(std::move(Args)) {}
    Function *codegen();
    const std::string &getName() const { return Name; }
};

/// FunctionAST - This class represents a function definition itself. 函数定义
class FunctionAST {
    std::unique_ptr<PrototypeAST> Proto;
    std::vector<std::unique_ptr<ExprAST>> Body;

public:
    FunctionAST(std::unique_ptr<PrototypeAST> Proto,
                std::vector<std::unique_ptr<ExprAST>> Body)
            : Proto(std::move(Proto)), Body(std::move(Body)) {}
    Function *codegen();
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

//----------------------------------------------------------------------//
// Code generation
//----------------------------------------------------------------------//

LLVMContext TheContext;
IRBuilder<> Builder(TheContext);
std::unique_ptr<Module> TheModule;
std::map<std::string, Value *> NamedValues;

AllocaInst *CreateEntryBlockAlloca(Function *TheFunction,
                                   const std::string &VarName) {
    IRBuilder<> TmpB(&TheFunction->getEntryBlock(),
                     TheFunction->getEntryBlock().begin());
    return TmpB.CreateAlloca(Type::getDoubleTy(TheContext), nullptr, VarName);
}

Value *LogErrorV(const char *Str) {
    LogError(Str);
    return nullptr;
}

Value *NumberExprAST::codegen() {
    return ConstantFP::get(TheContext, APFloat(DoubleVal));
}


Value *VariableExprAST::codegen() {
    // Look this variable up in the function.
    Value *V = NamedValues[Name];
    if (!V)
        return LogErrorV("Unknown variable name");
    return Builder.CreateLoad(V, Name.c_str()); // 返回变量的引用
}

Value *BinaryExprAST::codegen() {
    if (Op == '='){
        VariableExprAST *LHSE = static_cast<VariableExprAST*> (LHS.get());
        if(!LHSE)
            return LogErrorV("destination of '=' must be a variable");
        Value *Val = RHS->codegen();
        if(!Val){
            return nullptr;
        }
        Value *Variable = NamedValues[LHSE->getName()];
        if(!Variable)
            return LogErrorV("Unknown variable name");
        Builder.CreateStore(Val, Variable);
        return Val;
    }
    Value *L = LHS->codegen(); // ExprAST 的codegen 可以是父类的codegen，可以产生任何类型的codegen
    Value *R = RHS->codegen();

    if (!L || !R)
        return nullptr;

    switch (Op) {
        case '+':
            return Builder.CreateFAdd(L, R, "addtmp");
        case '-':
            return Builder.CreateFSub(L, R, "subtmp");
        case '*':
            return Builder.CreateFMul(L, R, "multmp");
        case '/':
            return Builder.CreateFDiv(L, R, "divtmp");
        case '<':
            L = Builder.CreateFCmpULT(L, R, "cmptmp");
            // Convert bool 0/1 to double 0.0 or 1.0
            return Builder.CreateUIToFP(L, Type::getDoubleTy(TheContext), "booltmp");
        default:
            return LogErrorV("invalid binary operator");
    }
}
Value *CallExprAST::codegen() {
    // Look up the name in the global module table.
    Function *CalleeF = TheModule->getFunction(Callee); //获取函数名
    if (!CalleeF)
        return LogErrorV("Unknown function referenced");

    // If argument mismatch error.
    if (CalleeF->arg_size() != Args.size())
        return LogErrorV("Incorrect # arguments passed");

    std::vector<Value *> ArgsV;
    for (unsigned i = 0, e = Args.size(); i != e; ++i) {
        ArgsV.push_back(Args[i]->codegen());
        if (!ArgsV.back())
            return nullptr;
    }

    return Builder.CreateCall(CalleeF, ArgsV, "calltmp");
}

Function *PrototypeAST::codegen() {
    // Make the function type:  double(double,double) etc.
    std::vector<Type *> Doubles(Args.size(), Type::getDoubleTy(TheContext));
    FunctionType *FT =
            FunctionType::get(Type::getDoubleTy(TheContext), Doubles, false);

    Function *F =
            Function::Create(FT, Function::ExternalLinkage, Name, TheModule.get());

    // Set names for all arguments.
    unsigned Idx = 0;
    for (auto &Arg : F->args())
        Arg.setName(Args[Idx++]);

    return F;
}


Function *FunctionAST::codegen() {
    // First, check for an existing function from a previous 'extern' declaration.
    Function *TheFunction = TheModule->getFunction(Proto->getName());

    if (!TheFunction)
        TheFunction = Proto->codegen();

    if (!TheFunction)
        return nullptr;

    // Create a new basic block to start insertion into.
    BasicBlock *BB = BasicBlock::Create(TheContext, "entry", TheFunction);
    Builder.SetInsertPoint(BB);

    // Record the function arguments in the NamedValues map.

    NamedValues.clear();
    for (auto &Arg : TheFunction->args()){
        AllocaInst *Alloca = CreateEntryBlockAlloca(TheFunction, Arg.getName());
        Builder.CreateStore(&Arg, Alloca);
        NamedValues[Arg.getName()] = Alloca;
    }

    for(unsigned i = 0; i < Body.size() - 1; i++){
        Body[i]->codegen();

    }

    if (Value *RetVal = Body.back()->codegen()) {

        // Finish off the function.
        Builder.CreateRet(RetVal);

        // Validate the generated code, checking for consistency.
        verifyFunction(*TheFunction);

        return TheFunction;
    }

    // Error reading body, remove function.
    TheFunction->eraseFromParent();
    return nullptr;
}

Value *VarDefineExprAST::codegen(){
    Function *TheFunction = Builder.GetInsertBlock()->getParent();
    Value *InitVal;

    for(unsigned i = 0, e = Varnames.size();i!=e;i++){
        const std::string &Varname = Varnames[i].first;

        ExprAST *Init = Varnames[i].second.get();

        if(Init) {
            InitVal = Init->codegen();
            if (!InitVal)
                return nullptr;
        }else
            InitVal = ConstantFP::get(TheContext, APFloat(0.0));
        AllocaInst *Alloca = CreateEntryBlockAlloca(TheFunction, Varname);
        Builder.CreateStore(InitVal, Alloca);
        NamedValues[Varname] = Alloca;
    }
    return InitVal;
}