#ifndef __ID_TAGGER_H
#define __ID_TAGGER_H

#include "llvm/Pass.h"

namespace rcs
{
struct IDTagger: public llvm::ModulePass
{
  static char ID;

  IDTagger();
  void getAnalysisUsage(llvm::AnalysisUsage &AU) const override;
  bool runOnModule(llvm::Module &M) override;
};
}

#endif
