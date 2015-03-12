// A very precise but unsound pointer analysis.
// The point-to relations are collected from a dynamic trace.

#ifndef __DYN_AA_DYNAMIC_POINTER_ANALYSIS_H
#define __DYN_AA_DYNAMIC_POINTER_ANALYSIS_H

#include "llvm/Pass.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"

#include "rcs/PointerAnalysis/PointerAnalysis.h"

#include "dyn-aa/IntervalTree.h"
#include "dyn-aa/LogProcessor.h"

using namespace llvm;

namespace neongoby {
struct DynamicPointerAnalysis: public ModulePass, public rcs::PointerAnalysis, public LogProcessor {
  static char ID;

  DynamicPointerAnalysis(): ModulePass(ID) {}
  bool runOnModule(Module &M) override;
  void getAnalysisUsage(AnalysisUsage &AU) const override;

  // Interfaces of PointerAnalysis.
  void getAllPointers(std::vector<llvm::Value*> &Pointers) override;
  bool getPointees(const Value *Pointer, std::vector<llvm::Value*> &Pointees) override;
  void *getAdjustedAnalysisPointer(AnalysisID PI) override;

  // Interfaces of LogProcessor.
  void processMemAlloc(const MemAllocRecord &Record) override;
  void processTopLevel(const TopLevelRecord &Record) override;

 private:
  // Returns the value ID of <Addr>'s allocator.
  // Possible allocators include malloc function calls, AllocaInsts, and
  // global variables.
  Value *lookupAddress(void *Addr) const;

  // Stores all addr-taken declarations.
  IntervalTree<Value *> MemAllocs;
  // Use DenseSet instead of vector, because they are usually lots of
  // duplicated edges.
  DenseMap<const Value *, llvm::DenseSet<llvm::Value*>> PointTos;
};
}

#endif
