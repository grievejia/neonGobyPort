// Rename values to their value IDs.

#include "rcs/ID/IDAssigner.h"

#include "llvm/Pass.h"
#include "llvm/ADT/Twine.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/DerivedTypes.h"

#include <string>

using namespace llvm;

namespace rcs
{

static bool should_rename(const Value *V)
{
  if (isa<Function>(V))
    return false;
  if (isa<InlineAsm>(V))
    return false;
  if (isa<IntegerType>(V->getType()) || isa<PointerType>(V->getType()))
    return true;
  return false;
}

struct ValueRenaming: public ModulePass
{
  static char ID;

  ValueRenaming();
  virtual void getAnalysisUsage(AnalysisUsage &AU) const;
  virtual bool runOnModule(Module &M);
};

void ValueRenaming::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.setPreservesCFG();
  AU.addRequired<IDAssigner>();
}

ValueRenaming::ValueRenaming(): ModulePass(ID) {}

bool ValueRenaming::runOnModule(Module &M)
{
  IDAssigner &IDA = getAnalysis<IDAssigner>();

  for (unsigned VID = 0; VID < IDA.getNumValues(); ++VID) {
    Value *V = IDA.getValue(VID); assert(V);
    if (should_rename(V)) {
      V->setName("x" + std::to_string(VID));
    }
  }

  return true;
}

char ValueRenaming::ID = 0;
static RegisterPass<ValueRenaming> X("rename-values", "Rename values to their value IDs", false, false);

}
