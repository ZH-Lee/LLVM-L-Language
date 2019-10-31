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
    VariableExprAST(const std::string &Name) : Name(Name){}
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
    std::vector<std::string > Args;

public:
    PrototypeAST(const std::string &Name, std::vector<std::string > Args)
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

/// IfElseAST - Expression for if/else.
class IfElseAST : public ExprAST{
    std::unique_ptr<ExprAST> Cond;
    std::vector<std::unique_ptr<ExprAST>>Then, Else;
public:
    IfElseAST(std::unique_ptr<ExprAST> Cond,
                std::vector<std::unique_ptr<ExprAST>>Then,
              std::vector<std::unique_ptr<ExprAST>> Else)
              : Cond(std::move(Cond)), Then(std::move(Then)),Else(std::move(Else)) {}
    Value *codegen() override;
};

/// BodyExpr - Expression for a set of expression around by braces.
class BodyExprAST : public ExprAST{
    std::vector<std::unique_ptr<ExprAST>> Body;
public:
    BodyExprAST(std::vector<std::unique_ptr<ExprAST>> Body)
            :   Body(std::move(Body)) {}
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




//----------------------------------------------------------------------//
// Code generation
//----------------------------------------------------------------------//

LLVMContext TheContext;
IRBuilder<> Builder(TheContext);
std::unique_ptr<Module> TheModule;

/// map the defined variable to Value*.
std::map<std::string, Value *> NamedValues;


/// CreateEntryBlockAlloca - Binding VarName with a new space, and insert into the begining of the block.
AllocaInst *CreateEntryBlockAlloca(Function *TheFunction,
                                   const std::string &VarName) {
    IRBuilder<> Tmp(&TheFunction->getEntryBlock(),
                     TheFunction->getEntryBlock().begin());
    return Tmp.CreateAlloca(Type::getDoubleTy(TheContext), nullptr, VarName);
}

Value *LogErrorV(const char *Str) {
    LogError(Str);
    return nullptr;
}


Value *NumberExprAST::codegen() {
    return ConstantFP::get(TheContext, APFloat(DoubleVal)); ///@todo Add more type here.
}

Value *VariableExprAST::codegen() {
    // Look this variable up in the function.
    Value *V = NamedValues[Name];
    if (!V)
        return LogErrorV("Unknown variable name");
    return Builder.CreateLoad(V, Name.c_str()); // return ref of variable.
}

Value *BinaryExprAST::codegen() {
    if (Op == '='){
        VariableExprAST *LHSE = static_cast<VariableExprAST*> (LHS.get());
        if(!LHSE)
            return LogErrorV("right side of '=' must be a variable");
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
            return Builder.CreateFAdd(L, R, "Faddtmp");
        case '-':
            return Builder.CreateFSub(L, R, "Fsubtmp");
        case '*':
            return Builder.CreateFMul(L, R, "Fmultmp");
        case '/':
            return Builder.CreateFDiv(L, R, "Fdivtmp");
        case '<':
            L = Builder.CreateFCmpULT(L, R, "Fcmpless");
            // Convert bool 0/1 to double 0.0 or 1.0
            return Builder.CreateUIToFP(L, Type::getDoubleTy(TheContext), "booltmp");
        case '>':
            L = Builder.CreateFCmpUGT(L,R,"FcmpGreater");
            // Convert bool 0/1 to double 0.0 or 1.0
            return Builder.CreateUIToFP(L, Type::getDoubleTy(TheContext), "booltmp");
        default:
            return LogErrorV("invalid binary operator");
    }
}
Value *CallExprAST::codegen() {
    // Look up the name in the global module table.
    Function *CalleeF = TheModule->getFunction(Callee);
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

Value *BodyExprAST::codegen() {
    for(unsigned i = 0; i < Body.size(); i++){
        Body[i]->codegen();
    }
    return nullptr;
}

Function *FunctionAST::codegen() {
    // First, check for an existing function from a previous 'extern' declaration.
    Function *TheFunction = TheModule->getFunction(Proto->getName());

    if (!TheFunction)
        TheFunction = Proto->codegen();

    if (!TheFunction)
        return nullptr;
    //printf("h1");
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
    //printf("h2");
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

Value *IfElseAST::codegen(){

    Value *CondV = Cond->codegen();
    if (!CondV)
        return nullptr;

    // Convert condition to a bool by comparing non-equal to 0.0.
    CondV = Builder.CreateFCmpONE(
            CondV, ConstantFP::get(TheContext, APFloat(0.0)), "ifcond");

    Function *TheFunction = Builder.GetInsertBlock()->getParent();

    // Create blocks for the then and else cases.  Insert the 'then' block at the
    // end of the function.
    BasicBlock *ThenBB = BasicBlock::Create(TheContext, "then", TheFunction);
    BasicBlock *ElseBB = BasicBlock::Create(TheContext, "else");
    BasicBlock *MergeBB = BasicBlock::Create(TheContext, "ifcont");

    Builder.CreateCondBr(CondV, ThenBB, ElseBB);  //创建一个if的条件分之

    // Emit then value.
    Builder.SetInsertPoint(ThenBB);

    Value *ThenV;
    for (unsigned i = 0; i < Then.size();i++){
        ThenV = Then[i]->codegen();
    }

    if (!ThenV)
        return nullptr;

    Builder.CreateBr(MergeBB);    // 每个基础块都必须有分之或者return。
    // Codegen of 'Then' can change the current block, update ThenBB for the PHI.
    ThenBB = Builder.GetInsertBlock();

    // Emit else block.
    TheFunction->getBasicBlockList().push_back(ElseBB);
    Builder.SetInsertPoint(ElseBB);

    Value *ElseV;
    for (unsigned i = 0; i < Else.size();i++){
        ElseV = Else[i]->codegen();
    }
    if (!ElseV)
        return nullptr;


    Builder.CreateBr(MergeBB);
    // Codegen of 'Else' can change the current block, update ElseBB for the PHI.
    ElseBB = Builder.GetInsertBlock();

    // Emit merge block.
    TheFunction->getBasicBlockList().push_back(MergeBB);
    Builder.SetInsertPoint(MergeBB);
    PHINode *PN = Builder.CreatePHI(Type::getDoubleTy(TheContext), 2, "iftmp");

    PN->addIncoming(ThenV, ThenBB);
    PN->addIncoming(ElseV, ElseBB);
    return PN;
}

