#ifndef __RCS_POINTER_ANALYSIS_H
#define __RCS_POINTER_ANALYSIS_H

#include "rcs/ID/IDAssigner.h"

#include "llvm/IR/Value.h"

namespace rcs
{
struct PointerAnalysis
{
protected:
  using ValueList = std::vector<llvm::Value*>;
  PointerAnalysis() {}
public:
  // A must for an AnalysisGroup.
  static char ID;

  // We want to be subclassed. 
  virtual ~PointerAnalysis() {}
  // Returns true if we have point-to information for <Pointer>. 
  // Due to some limitations of underlying alias analyses, it is not always possible to capture all pointers. 
  virtual bool getPointees(const llvm::Value *Pointer, ValueList &Pointees) = 0;
  virtual void getAllPointers(ValueList &Pointers) = 0;
  // Print some stat information to <O>. 
  void printStats(llvm::raw_ostream &O);
};
}

#endif
