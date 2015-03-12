// The default implementation of PointerAnalysis. 

#include "rcs/ID/IDAssigner.h"
#include "rcs/PointerAnalysis/PointerAnalysis.h"

#include "llvm/Pass.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/IR/Argument.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Debug.h"

#include <list>
#include <string>

using namespace llvm;

namespace rcs
{

static bool shouldFilterOut(Value *V)
{
  if (Argument *Arg = dyn_cast<Argument>(V)) {
    if (Arg->getParent()->isDeclaration())
      return true;
  }
  return false;
}

static bool isMalloc(Function *F)
{
  auto funName = F->getName();
  return (funName == "malloc") || (funName == "calloc") || (funName == "valloc") || (funName == "realloc") || (funName == "memalign") || (funName == "_Znwm") || (funName == "_Znaj") || (funName == "_Znam");
}

static bool isMallocCall(Value *V)
{
  Instruction *I = dyn_cast<Instruction>(V);
  if (I == nullptr)
    return false;

  CallSite CS(I);
  if (CS.getInstruction() == nullptr)
    return false;

  Function *Callee = CS.getCalledFunction();
  return Callee && isMalloc(Callee);
}

struct BasicPointerAnalysis: public ModulePass, public PointerAnalysis
{
  static char ID;

  BasicPointerAnalysis();
  void getAnalysisUsage(AnalysisUsage &AU) const override;
  bool runOnModule(Module &M) override;

  void getAllPointers(ValueList &Pointers) override;
  bool getPointees(const Value *Pointer, ValueList &Pointees) override;

  // A very important function. Otherwise getAnalysis<PointerAnalysis> would
  // not be able to return BasicPointerAnalysis. 
  void *getAdjustedAnalysisPointer(AnalysisID PI) override;
  
 private:

  // Leader[V] is the leader of the equivalence class <V> belongs to. 
  // We could use llvm::EquivalenceClasses here. 
  DenseMap<const Value *, const Value *> Leader;
  // Allocators[L] is the set of all allocators in the set leaded by L. 
  // Allocators[V] does not make sense for a value that's not a leader. 
  DenseMap<const Value *, ValueList> Allocators;
};

void BasicPointerAnalysis::getAnalysisUsage(AnalysisUsage &AU) const
{
  AU.setPreservesAll();
  AU.addRequired<IDAssigner>();
  AU.addRequired<AliasAnalysis>();
}

BasicPointerAnalysis::BasicPointerAnalysis(): ModulePass(ID)
{
}

bool BasicPointerAnalysis::runOnModule(Module &M)
{
  AliasAnalysis &AA = getAnalysis<AliasAnalysis>();

  ValueList AllPointers;
  getAllPointers(AllPointers);
  std::list<Value *> RemainingPointers(AllPointers.begin(), AllPointers.end());

  // Note that pointers and pointees are all of PointerType. 
  while (!RemainingPointers.empty()) {
    Value *TheLeader = RemainingPointers.front();
    for (auto I = RemainingPointers.begin();
         I != RemainingPointers.end(); ) {
      Value *V = *I;
      // alias(V1, 0, V2, 0) would always return NoAlias, because the ranges
      // are zero-size and thus disjoint. 
      if (AA.alias(TheLeader, 1, V, 1) != AliasAnalysis::NoAlias)
      {
        Leader[V] = TheLeader;
        if (isa<GlobalValue>(V) || isa<AllocaInst>(V) || isMallocCall(V))
          Allocators[TheLeader].push_back(V); 
        auto ToDelete = I;
        ++I;
        RemainingPointers.erase(ToDelete);
      }
      else
      {
        ++I;
      }
    }
  }
  dbgs() << "# of equivalence classes = " << Allocators.size() << "\n";
  
  return false;
}

void BasicPointerAnalysis::getAllPointers(ValueList &Pointers) {
  IDAssigner &IDA = getAnalysis<IDAssigner>();

  Pointers.clear();
  for (unsigned i = 0; i < IDA.getNumValues(); ++i) {
    Value *V = IDA.getValue(i);
    if (V->getType()->isPointerTy() && !shouldFilterOut(V))
      Pointers.push_back(V);
  }
}

bool BasicPointerAnalysis::getPointees(const Value *Pointer, ValueList &Pointees) {
  assert(Pointer->getType()->isPointerTy() && "<Pointer> is not a pointer");

  if (!Leader.count(Pointer))
    return false;
  const Value *TheLeader = Leader.lookup(Pointer);

  Pointees.clear();
  DenseMap<const Value *, ValueList>::const_iterator I =
      Allocators.find(TheLeader);
  if (I != Allocators.end())
    Pointees = I->second;

  return true;
}

void *BasicPointerAnalysis::getAdjustedAnalysisPointer(AnalysisID PI) {
  if (PI == &PointerAnalysis::ID)
    return (PointerAnalysis *)this;
  return this;
}

char BasicPointerAnalysis::ID = 0;
static RegisterPass<BasicPointerAnalysis> X("basic-pa", "Basic Pointer Analysis", false, true);
static RegisterAnalysisGroup<PointerAnalysis, true> Y(X);
}