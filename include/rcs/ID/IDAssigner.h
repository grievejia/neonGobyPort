#ifndef __ID_ASSIGNER_H
#define __ID_ASSIGNER_H

#include "llvm/Pass.h"
#include "llvm/IR/Instruction.h"
#include "llvm/ADT/DenseMap.h"

namespace rcs
{
class IDAssigner: public llvm::ModulePass
{
private:
  bool addValue(llvm::Value *V);
  bool addIns(llvm::Instruction *I);
  bool addFunction(llvm::Function *F);
  void extractValuesInUser(llvm::User *U);

  void printInstructions(llvm::raw_ostream &O, const llvm::Module *M) const;
  void printValues(llvm::raw_ostream &O, const llvm::Module *M) const;

  llvm::DenseMap<const llvm::Instruction *, unsigned> InsIDMapping;
  llvm::DenseMap<const llvm::Value *, unsigned> ValueIDMapping;
  llvm::DenseMap<unsigned, llvm::Instruction *> IDInsMapping;
  llvm::DenseMap<unsigned, llvm::Value *> IDValueMapping;
  llvm::DenseMap<const llvm::Function *, unsigned> FunctionIDMapping;
  llvm::DenseMap<unsigned, llvm::Function *> IDFunctionMapping;
public:
  static char ID;
  static const unsigned InvalidID;

  IDAssigner();
  void getAnalysisUsage(llvm::AnalysisUsage &AU) const override;
  bool runOnModule(llvm::Module& M) override;
  void print(llvm::raw_ostream &O, const llvm::Module *M) const override;

  unsigned getInstructionID(const llvm::Instruction *I) const;
  unsigned getValueID(const llvm::Value *V) const;
  // Returns whether <V> is in this module.
  bool exists(const llvm::Value *V) const {
    return getValueID(V) != InvalidID;
  }
  unsigned getFunctionID(const llvm::Function *F) const;
  llvm::Instruction *getInstruction(unsigned ID) const;
  llvm::Value *getValue(unsigned ID) const;
  llvm::Function *getFunction(unsigned ID) const;
  /** Requires IDs to be consecutive. */
  unsigned getNumValues() const { return ValueIDMapping.size(); }
  void printValue(llvm::raw_ostream &O, const llvm::Value *V) const;
};
}

#endif
