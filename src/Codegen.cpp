//
// Created by lee on 2019-11-01.
//
#include "LJIT.h"
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
#include "llvm/Support/TargetSelect.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Transforms/InstCombine/InstCombine.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Scalar/GVN.h"
#include "AST.cpp"
#include <string>

LLVMContext TheContext;
IRBuilder<> Builder(TheContext);
std::unique_ptr<Module> TheModule;
std::unique_ptr<legacy::FunctionPassManager> TheFPM;
std::unique_ptr<llvm::orc::KaleidoscopeJIT> TheJIT;
std::map<std::string, std::unique_ptr<PrototypeAST>> FunctionProtos;
/// map the defined variable to Value*.
std::map<std::string, Value *> NamedValues;

Function *getFunction(std::string Name) {
    // First, see if the function has already been added to the current module.
    if (auto *F = TheModule->getFunction(Name))
        return F;

    // If not, check whether we can codegen the declaration from some existing
    // prototype.
    auto FI = FunctionProtos.find(Name);
    if (FI != FunctionProtos.end())
        return FI->second->codegen();

    // If no existing prototype exists, return null.
    return nullptr;
}

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
    Function *CalleeF = getFunction(Callee);
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

    auto &P = *Proto;
    FunctionProtos[Proto->getName()] = std::move(Proto);

    // Look up the function, see if the function has been added to the current module.
    Function *TheFunction = getFunction(P.getName());

    // If not, see if we can generate code from exist prototype and body.
    if (!TheFunction)
        TheFunction = Proto->codegen();

    // If cannot, return nullptr;
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

    // generating code
    for(unsigned i = 0; i < Body.size() - 1; i++){
        Body[i]->codegen();
    }

    if (Value *RetVal = Body.back()->codegen()) {

        // Finish off the function.
        Builder.CreateRet(RetVal);

        // Validate the generated code, checking for consistency.
        verifyFunction(*TheFunction);
        TheFPM->run(*TheFunction);
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




