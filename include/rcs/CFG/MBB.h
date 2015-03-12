#ifndef __MBB_H
#define __MBB_H

#include "llvm/IR/Module.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/Pass.h"
#include "llvm/ADT/DenseMap.h"

namespace rcs {
/**
 * An MBB ends with a TerminatorInst or a CallInst. 
 * Note that an InvokeInst is a TerminatorInst. 
 *
 * begin = the first instruction in the MBB. 
 * end = the successor of its last instruction. 
 *
 * e.g. The following BB has two MBBs. 
 * MBB1: %3 = %1 + %2
 *       call foo()
 * MBB2: invoke bar(%3)
 *
 * MBB1.begin() == %3, MBB1.end() == invoke
 * MBB2.begin() == invoke, MBB2.end() == BB.end()
 */
class MicroBasicBlock: public llvm::ilist_node<MicroBasicBlock>
{
public:
  using iterator = llvm::BasicBlock::iterator;
  using const_iterator = llvm::BasicBlock::const_iterator;
private:
  llvm::BasicBlock* parent;
  iterator b, e;
public:
  MicroBasicBlock(): parent(nullptr) {}
  MicroBasicBlock(llvm::BasicBlock* p, iterator bb, iterator ee):
      parent(p), b(bb), e(ee)
  {
    assert(p && b != e);
  }

  iterator begin() { return b; }
  iterator end() { return e; }
  const_iterator begin() const { return b; }
  const_iterator end() const { return e; }
  llvm::Instruction &front() { return *b; }
  const llvm::Instruction &front() const { return *b; }
  llvm::Instruction &back() {
    iterator i = e;
    return *(--i);
  }
  const llvm::Instruction &back() const {
    const_iterator i = e;
    return *(--i);
  }

  llvm::BasicBlock *getParent() { return parent; }
  const llvm::BasicBlock *getParent() const { return parent; }

  iterator getFirstNonPHI();
};

struct MicroBasicBlockBuilder: public llvm::ModulePass
{
public:
  using MBBListType = llvm::iplist<MicroBasicBlock>;
private:
  using MBBMapType = llvm::DenseMap<llvm::BasicBlock*, MBBListType*>;

  MBBMapType mbbMap;
  llvm::DenseMap<const llvm::Instruction *, MicroBasicBlock *> parentMbb;
public:
  static char ID;

  MicroBasicBlockBuilder();
  void getAnalysisUsage(llvm::AnalysisUsage &AU) const override
  { 
    AU.setPreservesAll(); 
  }
  bool runOnModule(llvm::Module &M) override;

  MBBListType::iterator begin(llvm::BasicBlock *bb);
  MBBListType::iterator end(llvm::BasicBlock *bb);
  MBBListType::iterator parent(const llvm::Instruction *ins)
  {
    return parentMbb.lookup(ins);
  }
};

using mbb_iterator = MicroBasicBlockBuilder::MBBListType::iterator;
}

#endif
