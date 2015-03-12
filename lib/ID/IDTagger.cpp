#define DEBUG_TYPE "rcs-id"

#include "rcs/ID/IDTagger.h"

#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"

using namespace llvm;

namespace rcs
{

IDTagger::IDTagger(): ModulePass(ID) {}

void IDTagger::getAnalysisUsage(AnalysisUsage &AU) const
{
  AU.setPreservesCFG();
}

bool IDTagger::runOnModule(Module &M) {
  auto IntType = IntegerType::get(M.getContext(), 32);
  auto numInstructions = 0u;
  for (auto& f: M)
  {
    for (auto& bb: f)
    {
      for (auto& inst: bb)
      {
        auto InsID = ConstantInt::get(IntType, numInstructions);
        inst.setMetadata("ins_id", MDNode::get(M.getContext(), ConstantAsMetadata::get(InsID)));
        ++numInstructions;
      }
    }
  }
  return true;
}

char IDTagger::ID = 0;
static RegisterPass<IDTagger> X("tag-id", "Assign each instruction a unique ID", false, false);

}