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

#include "llvm/ADT/SetVector.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Demangle/Demangle.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassInstrumentation.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/Verifier.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Pass.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"

namespace {

//===----------------------------------------------------------------------===//
/// This class implements an LLVM module analysis pass.
/// The CallSiteFinderAnalysis retrieves all call sites at which direct calls
/// to the "void foo()" function are found.
///
class CallSiteFinderAnalysis
    : public llvm::AnalysisInfoMixin<CallSiteFinderAnalysis> {
public:
  explicit CallSiteFinderAnalysis() = default;
  ~CallSiteFinderAnalysis() = default;
  // Provide a unique key, i.e., memory address to be used by the LLVM's pass
  // infrastructure.
  static inline llvm::AnalysisKey Key; // NOLINT
  friend llvm::AnalysisInfoMixin<CallSiteFinderAnalysis>;

  // Specify the result type of this analysis pass.
  using Result = llvm::SetVector<llvm::CallBase *>;

  // Analyze the bitcode/IR in the given LLVM module.
  static Result run(llvm::Module &M,
                    [[maybe_unused]] llvm::ModuleAnalysisManager &MAM) {
    // The demangled(!) function name that we wish to find.
    const static llvm::StringRef TargetFunName = "foo()";
    Result TargetCallSites;
    llvm::outs() << "running code analysis...\n";
    for (auto &F : M) {
      for (auto &BB : F) {
        for (auto &I : BB) {
          if (auto *CB = llvm::dyn_cast<llvm::CallBase>(&I)) {
            // Only find direct function calls.
            if (!CB->isIndirectCall() && CB->getCalledFunction() &&
                llvm::demangle(CB->getCalledFunction()->getName().str()) ==
                    TargetFunName) {
              llvm::outs() << "found a direct call to '" << TargetFunName
                           << "'!\n";
              TargetCallSites.insert(CB);
            }
          }
        }
      }
    }
    return TargetCallSites;
  }
};

//===----------------------------------------------------------------------===//
/// This class implements an LLVM module transformation pass.
/// The CallSiteReplacer queries the analysis pass in the above and replaces
/// all direct calls to the "void foo()" that have been found by the
/// CallSiteFinderAnalysis pass with calls to "void bar(int)". As a parameter to
/// the "void bar(int)" function it provides a counter variables that counts the
/// number of replacements that took place.
///
class CallSiteReplacer : public llvm::PassInfoMixin<CallSiteReplacer> {
public:
  explicit CallSiteReplacer() = default;
  ~CallSiteReplacer() = default;

  // Transform the bitcode/IR in the given LLVM module.
  static llvm::PreservedAnalyses run(llvm::Module &M,
                                     llvm::ModuleAnalysisManager &MAM) {
    // Request the results of our CallSiteFinderAnalysis analysis pass.
    // If the results are not yet available, because no other pass requested
    // them until now, they will be computed on-the-fly.
    auto TargetCallSites = MAM.getResult<CallSiteFinderAnalysis>(M);
    // The name of the function that we wish to call instead.
    const static llvm::StringRef ReplacementFunName = "_Z3bari";
    auto *ReplacementFun = M.getFunction(ReplacementFunName);
    static unsigned ReplacementCounter = 1;
    llvm::outs() << "applying code transformation...\n";
    for (auto *TargetCallSite : TargetCallSites) {
      // Create an LLVM constant int from our replacement counter.
      auto *ConstInt = llvm::ConstantInt::get(
          llvm::IntegerType::get(M.getContext(), 32 /* bits */),
          ReplacementCounter);
      // Construct the new call site.
      auto *NewCallSite = llvm::CallInst::Create(
          llvm::FunctionCallee(ReplacementFun), {ConstInt});
      // Replace the target call site with the new call site.
      llvm::ReplaceInstWithInst(TargetCallSite, NewCallSite);
      ++ReplacementCounter;
    }
    // We are lazy here and just claim that this transformation pass invalidates
    // the results of all other analysis passes.
    return llvm::PreservedAnalyses::none();
  }
};

} // end anonymous namespace

// We could, of course, also compile the above code into a shared object library
// that we can then use as a plugin for LLVM's optimizer 'opt'. But instead,
// here we are going the full do-it-yourself route and set up everything
// ourselves.
int main(int Argc, char **Argv) {
  if (Argc != 2) {
    llvm::outs() << "usage: <prog> <IR file>\n";
    return 1;
  }
  // Parse an LLVM IR file.
  llvm::SMDiagnostic Diag;
  llvm::LLVMContext CTX;
  std::unique_ptr<llvm::Module> M =
      llvm::parseIRFile(Argv[1], Diag, CTX); // NOLINT
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
  MAM.registerPass([&]() { return CSF; });
  PB.registerModuleAnalyses(MAM);
  // Add our transformation pass.
  MPM.addPass(CallSiteReplacer());
  // Just to be sure that none of the passes messed up the module.
  MPM.addPass(llvm::VerifierPass());
  // Run our transformation pass.
  MPM.run(*M, MAM);
  llvm::outs() << "the transformed program:\n"
               << "------------------------\n";
  llvm::outs() << *M;
  return 0;
}
