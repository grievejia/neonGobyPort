// IDManager should be used with IDTagger.
// IDManager builds the ID mapping from the ins_id metadata embedded
// in the program.

#ifndef __IDMANAGER_H
#define __IDMANAGER_H

#include "llvm/Pass.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/IR/Instruction.h"

#include <vector>

namespace rcs
{
struct IDManager: public llvm::ModulePass
{
private:
  using InstList = std::vector<llvm::Instruction*>;
  llvm::DenseMap<unsigned, InstList> IDMapping;
public:
  static char ID;
  static const unsigned INVALID_ID = (unsigned)-1;

  IDManager();
  void getAnalysisUsage(llvm::AnalysisUsage &AU) const override;
  bool runOnModule(llvm::Module &M) override;
  void print(llvm::raw_ostream &O, const llvm::Module *M) const override;

  unsigned size() const { return IDMapping.size(); }
  unsigned getInstructionID(const llvm::Instruction *I) const;
  llvm::Instruction *getInstruction(unsigned InsID) const;
  InstList getInstructions(unsigned InsID) const;
};
}

#endif
