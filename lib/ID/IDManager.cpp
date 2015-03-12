#include "rcs/ID/IDManager.h"

#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace rcs
{

void IDManager::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.setPreservesAll();
}

IDManager::IDManager(): ModulePass(ID) {}

bool IDManager::runOnModule(Module &M) {
  IDMapping.clear();
  for (auto& f: M)
  {
    for (auto& bb: f)
    {
      for (auto& inst: bb)
      {
        unsigned InsID = getInstructionID(&inst);
        if (InsID != INVALID_ID)
          IDMapping[InsID].push_back(&inst);
      }
    }
  }
  if (size() == 0)
    errs() << "[Warning] No ID information in this program.\n";

  return false;
}

unsigned IDManager::getInstructionID(const Instruction *I) const {
  auto mdNode = I->getMetadata("ins_id");
  if (!mdNode)
    return INVALID_ID;
  assert(mdNode->getNumOperands() == 1);
  auto constMd = cast<ConstantAsMetadata>(mdNode->getOperand(0));
  return cast<ConstantInt>(constMd->getValue())->getZExtValue();
}

Instruction *IDManager::getInstruction(unsigned InsID) const
{
  auto itr = IDMapping.find(InsID);
  if (itr == IDMapping.end())
    return nullptr;
  auto const& insts = itr->second;
  if (insts.size() == 0 || insts.size() > 1)
    return nullptr;

  return insts[0];
}

IDManager::InstList IDManager::getInstructions(unsigned InsID) const
{
  return IDMapping.lookup(InsID);
}

void IDManager::print(raw_ostream &O, const Module *M) const {
  /*vector<pair<unsigned, Instruction *> > Entries;
  for (DenseMap<unsigned, Instruction *>::const_iterator I = IDMapping.begin();
       I != IDMapping.end(); ++I) {
    Entries.push_back(*I);
  }
  sort(Entries.begin(), Entries.end());
  for (size_t i = 0; i < Entries.size(); ++i) {
    Instruction *Ins = Entries[i].second;
    BasicBlock *BB = Ins->getParent();
    Function *F = BB->getParent();
    // Print the function name if <ins> is the function entry. 
    if (Ins == F->getEntryBlock().begin()) 
      O << "\nFunction " << F->getName() << ":\n";
    if (Ins == BB->begin())
      O << "\nBB " << F->getName() << "." << BB->getName() << ":\n";
    O << Entries[i].first << ":\t" << *Ins << "\n";
  }*/

}

char IDManager::ID = 0;
static RegisterPass<IDManager> X("manage-id", "Find the instruction with a particular ID; Lookup the ID of an instruction", false, true);

}