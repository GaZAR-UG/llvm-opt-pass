//===----------------------------------------------------------------------===//
//
// Philipp Schubert
//
//    Copyright (c) 2021
//    GaZAR UG (haftungsbeschränkt)
//    Bielefeld, Germany
//    philipp@gazar.eu
//
//===----------------------------------------------------------------------===//

#include <iostream>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>

#include "llvm/ADT/SetVector.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassInstrumentation.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/Verifier.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Pass.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/raw_ostream.h"

namespace {

class CallSiteFinderAnalysis
    : public llvm::AnalysisInfoMixin<CallSiteFinderAnalysis> {
public:
  explicit CallSiteFinderAnalysis() = default;
  ~CallSiteFinderAnalysis() = default;
  // Provide a unique key, i.e., memory address to be used by the LLVM's pass
  // infrastructure.
  static inline llvm::AnalysisKey Key;
  friend llvm::AnalysisInfoMixin<CallSiteFinderAnalysis>;

  // Specify the result type of this analysis pass.
  using Result = llvm::SetVector<llvm::CallBase *>;

  // Analyze the bitcode/IR in the given LLVM module.
  Result run(llvm::Module &M,
             [[maybe_unused]] llvm::ModuleAnalysisManager &MAM) {
    // The function name that we wish to find.
    const static llvm::StringRef TargetFunName = "_Z3foov";
    Result TargetCallSites;
    for (auto &F : M) {
      for (auto &BB : F) {
        for (auto &I : BB) {
          if (auto *CB = llvm::dyn_cast<llvm::CallBase>(&I)) {
            if (!CB->isIndirectCall()) {
              // Only find direct function calls.
              if (CB->getCalledFunction() &&
                  CB->getCalledFunction()->getName() == TargetFunName) {
                llvm::outs()
                    << "found a direct call to '" << TargetFunName << "'!\n";
                TargetCallSites.insert(CB);
              }
            }
          }
        }
      }
    }
    return TargetCallSites;
  }
};

class CallSiteReplacer : public llvm::PassInfoMixin<CallSiteReplacer> {
public:
  explicit CallSiteReplacer() = default;
  ~CallSiteReplacer() = default;

  // Transform the bitcode/IR in the given LLVM module.
  llvm::PreservedAnalyses run(llvm::Module &M,
                              llvm::ModuleAnalysisManager &MAM) {
    // Request the results of our 'CallSiteFinderAnalysis' analysis pass.
    // If the results are not yet available, because no other pass requested
    // them until now, they will be computed on-the-fly.
    auto TargetCallSites = MAM.getResult<CallSiteFinderAnalysis>(M);
    // The name of the function that we wish to call instead.
    const static llvm::StringRef ReplacementFunName = "_Z3bari";
    auto *ReplacementFun = M.getFunction(ReplacementFunName);
    llvm::outs() << "the analysis pass found " << TargetCallSites.size()
                 << " interesting target call sites...\n";
    llvm::outs() << "applying code transformation...\n";
    for (auto *TargetCallSite : TargetCallSites) {
      // Construct the callee for replacement.
      // auto *IntTy = llvm::IntegerType::get(M.getContext(), 32);
      // auto *FunTy = llvm::FunctionType::get(
          // llvm::Type::getVoidTy(M.getContext()), {IntTy}, false);
      // llvm::FunctionCallee NewCallee(FunTy, ReplacementFun);
      TargetCallSite->setCalledFunction(ReplacementFun);
            auto *ConstInt = llvm::ConstantInt::get(
          llvm::IntegerType::get(M.getContext(), 32), 42);
      // TargetCallSite->set
      TargetCallSite->setArgOperand(0, ConstInt);
      TargetCallSite->getFunctionType()->print(llvm::outs());
      llvm::outs() << '\n';
      // Produce an integer constant to be used as an argument.

      // TargetCallSite->replace;
      llvm::outs() << "arg ops: " << TargetCallSite->getNumArgOperands() << '\n';
      // TargetCallSite->setArgOperand(0, ConstInt);
    }
    return llvm::PreservedAnalyses::none();
  }
};

} // end anonymous namespace

// We could, of course, also compile the above code into a shared object library
// that we can then use as a plugin for LLVM's optimizer 'opt'. But instead,
// here we are going the full do-it-yourself route and set up everything
// ourselves.
int main(int argc, char **argv) {
  if (argc != 2) {
    llvm::outs() << "usage: <prog> <IR file>\n";
    return 1;
  }
  // Parse an LLVM IR file.
  llvm::SMDiagnostic Diag;
  llvm::LLVMContext CTX;
  std::unique_ptr<llvm::Module> M = llvm::parseIRFile(argv[1], Diag, CTX);
  // Check if the module is valid.
  bool BrokenDbgInfo = false;
  if (llvm::verifyModule(*M, &llvm::errs(), &BrokenDbgInfo)) {
    llvm::errs() << "error: invalid module\n";
    return 1;
  }
  if (BrokenDbgInfo) {
    llvm::errs() << "caution: debug info is broken\n";
  }
  llvm::PassBuilder PB;
  llvm::ModuleAnalysisManager MAM;
  llvm::ModulePassManager MPM;
  CallSiteFinderAnalysis CSF;
  // Register our analysis pass.
  MAM.registerPass([&]() { return std::move(CSF); });
  PB.registerModuleAnalyses(MAM);
  // Add our transformation pass.
  MPM.addPass(CallSiteReplacer());
  // Just to be sure that none of the passes messed up the module.
  MPM.addPass(llvm::VerifierPass());
  // Run our transformation pass.
  MPM.run(*M, MAM);
  llvm::outs() << "transformed program:\n";
  llvm::outs() << *M;
  llvm::outs() << '\n';
  return 0;
}