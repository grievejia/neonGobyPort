/**
 * TODO: Put it into folder <cfg>. <cfg> now requires <bc2bdd>, and we
 * don't want <mbb> heavy-weighted. 
 */

#include "rcs/CFG/MBB.h"

#include "llvm/IR/CallSite.h"

using namespace llvm;

namespace rcs
{

static inline bool isNonIntrinsicCall(const Instruction *I)
{
  ImmutableCallSite cs(I);
  if (!cs)
    return false;
  auto callee = cs.getCalledFunction();
  return (callee != nullptr) && (callee->isIntrinsic());
}

MicroBasicBlockBuilder::MicroBasicBlockBuilder(): ModulePass(ID) {}

MicroBasicBlock::iterator MicroBasicBlock::getFirstNonPHI()
{
  if (b == parent->begin())
    return parent->getFirstNonPHI();
  return b;
}

bool MicroBasicBlockBuilder::runOnModule(Module &M) {
  parentMbb.clear();
  mbbMap.clear();

  for (auto& f: M)
    for (auto& bb: f)
    {
      auto mbblist = new MBBListType();
      for (auto ib = bb.begin(); ib != bb.end(); )
      {
        auto ie = ib;
        while (bb.getTerminator() != ie && !isNonIntrinsicCall(ie))
          ++ie;
        // <ie> points to the last instruction of the MBB. 
        assert(ie != bb.end());
        ++ie;
        // <ie> points to the successor of the MBB. 
        auto mbb = new MicroBasicBlock(&bb, ib, ie);
        mbblist->push_back(mbb);
        // Reverse mapping from Instruction to its containing MBB. 
        for (BasicBlock::iterator ii = ib; ii != ie; ++ii)
          parentMbb[ii] = mbb;
        ib = ie;
      }
      mbbMap[&bb] = mbblist;
    }

  return false;
}

mbb_iterator MicroBasicBlockBuilder::begin(BasicBlock *bb)
{
  assert(exist(bb, mbbMap) && "not a basic block");
  return mbbMap[bb]->begin();
}

mbb_iterator MicroBasicBlockBuilder::end(BasicBlock *bb)
{
  assert(exist(bb, mbbMap) && "not a basic block");
  return mbbMap[bb]->end();
}

char MicroBasicBlockBuilder::ID = 0;
static RegisterPass<MicroBasicBlockBuilder> X("mbbb", "micro basic block builder", false, false);

}
