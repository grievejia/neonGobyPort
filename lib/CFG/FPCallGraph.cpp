// A call-graph builder considering function pointers.
// The targets of function pointers are identified by alias analysis.
// Users may specify which alias analysis she wants to run this pass with.

#include "rcs/CFG/FPCallGraph.h"

#include "llvm/Support/CommandLine.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/Analysis/AliasAnalysis.h"

using namespace llvm;

namespace rcs
{

static inline bool isPthreadCreate(const Instruction *I)
{
  ImmutableCallSite cs(I);
  // Not even a call/invoke. 
  if (!cs)
    return false;
  auto callee = cs.getCalledFunction();
  if (callee == nullptr)
    return false;
  if (callee->getName() == "pthread_create")
    return cs.arg_size() == 4;
  else if (callee->getName() == "tern_wrap_pthread_create")
    return cs.arg_size() == 6;
  else
    return false;
}

static inline const Value* getPthreadCreateCallee(const Instruction *I)
{
  assert(isPthreadCreate(I));
  ImmutableCallSite cs(I);
  return cs.getArgument(cs.arg_size() == 4 ? 2 : 4);
}

template <typename T>
static void uniquifyVector(std::vector<T> &V)
{
  std::sort(V.begin(), V.end());
  V.erase(std::unique(V.begin(), V.end()), V.end());
}

void FPCallGraph::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.setPreservesAll();
  AU.addRequired<AliasAnalysis>();
}

/*void *FPCallGraph::getAdjustedAnalysisPointer(AnalysisID PI) {
  if (PI == &CallGraph::ID)
    return (CallGraph*)this;
  return this;
}*/

FPCallGraph::FPCallGraph(): ModulePass(ID), M(NULL), Root(NULL), ExternalCallingNode(NULL), CallsExternalNode(NULL) {
}

FPCallGraph::~FPCallGraph() {
  // <CallsExternalNode> is not in the function map, delete it explicitly.
  //CallsExternalNode->allReferencesDropped();
  delete CallsExternalNode;
  CallsExternalNode = NULL;
  
  for (FunctionMapTy::iterator I = FunctionMap.begin(), E = FunctionMap.end();
       I != E; ++I)
    delete I->second;
}

void FPCallGraph::addCallEdge(const CallSite &Site, Function *Callee) {
  Instruction *Ins = Site.getInstruction();
  assert(Ins);
  SiteToFuncs[Ins].push_back(Callee);
  FuncToSites[Callee].push_back(Ins);
  // Update CallGraph as well.
  CallGraphNode *Node = getOrInsertFunction(Ins->getParent()->getParent());
  Node->addCalledFunction(Site, getOrInsertFunction(Callee));
}

FPCallGraph::FuncList FPCallGraph::getCalledFunctions(
    const Instruction *Ins) const {
  SiteToFuncsMapTy::const_iterator I = SiteToFuncs.find(Ins);
  if (I == SiteToFuncs.end())
    return FuncList();
  return I->second;
}

FPCallGraph::InstList FPCallGraph::getCallSites(
    const Function *F) const {
  FuncToSitesMapTy::const_iterator I = FuncToSites.find(F);
  if (I == FuncToSites.end())
    return InstList();
  return I->second;
}

void FPCallGraph::processCallSite(const CallSite &CS, const FuncSet &AllFuncs) {
  AliasAnalysis &AA = getAnalysis<AliasAnalysis>();

  if (Function *Callee = CS.getCalledFunction()) {
    // Ignore calls to intrinsic functions.
    // CallGraph would throw assertion failures.
    if (!Callee->isIntrinsic()) {
      addCallEdge(CS, Callee);
      const Instruction *Ins = CS.getInstruction();
      if (isPthreadCreate(Ins)) {
        // Add edge: Ins => the thread function
        Value *Target = const_cast<Value*>(getPthreadCreateCallee(Ins));
        if (Function *ThrFunc = dyn_cast<Function>(Target)) {
          // pthread_create with a known function
          addCallEdge(CS, ThrFunc);
        } else {
          // Ask AA which functions <target> may point to.
          for (FuncSet::const_iterator I = AllFuncs.begin();
               I != AllFuncs.end(); ++I) {
            if (AA.alias(Target, *I))
              addCallEdge(CS, *I);
          }
        }
      }
    }
  } else {
    Value *FP = CS.getCalledValue();
    assert(FP && "Cannot find the function pointer");
    // Ask AA which functions <fp> may point to.
    for (FuncSet::const_iterator I = AllFuncs.begin();
         I != AllFuncs.end(); ++I) {
      if (AA.alias(FP, *I))
        addCallEdge(CS, *I);
    }
  }
}

CallGraphNode *FPCallGraph::getOrInsertFunction(const Function *F) {
  CallGraphNode *&CGN = FunctionMap[F];
  if (CGN)
    return CGN;

  assert((!F || F->getParent() == M) && "Function not in current module!");
  return CGN = new CallGraphNode(const_cast<Function*>(F));
}

Function *FPCallGraph::removeFunctionFromModule(CallGraphNode *CGN) {
  assert(CGN->empty() && "Cannot remove function from call "
         "graph if it references other functions!");
  Function *F = CGN->getFunction(); // Get the function for the call graph node
  delete CGN;                       // Delete the call graph node for this func
  FunctionMap.erase(F);             // Remove the call graph node from the map

  M->getFunctionList().remove(F);
  return F;
}

bool FPCallGraph::runOnModule(Module &Mod) {
  // Initialize super class CallGraph.
  M = &Mod;

  // Use getOrInsertFunction(NULL) so that
  // ExternalCallingNode->getFunction() returns NULL.
  ExternalCallingNode = getOrInsertFunction(NULL);
  CallsExternalNode = new CallGraphNode(NULL);

  // Every function need to have a corresponding CallGraphNode.
  for (Module::iterator F = M->begin(); F != M->end(); ++F)
    getOrInsertFunction(F);

  /*
   * Get the set of all defined functions.
   * Will be used as a candidate set for point-to analysis.
   * FIXME: Currently we have to skip external functions, otherwise
   * bc2bdd would fail. Don't ask me why.
   */
  FuncSet AllFuncs;
  for (Module::iterator F = M->begin(); F != M->end(); ++F) {
    if (!F->isDeclaration())
      AllFuncs.insert(F);
  }

  /* Get Root (main function) */
  unsigned NumMains = 0;
  for (Module::iterator F = M->begin(); F != M->end(); ++F) {
    if (!F->hasLocalLinkage() && F->getName() == "main") {
      NumMains++;
      Root = getOrInsertFunction(F);
    }
  }
  // No root if no main function or more than one main functions.
  if (NumMains != 1)
    Root = ExternalCallingNode;

  // Connect <ExternalCallingNode>
  if (Root != ExternalCallingNode) {
    ExternalCallingNode->addCalledFunction(CallSite(), Root);
  } else {
    for (Module::iterator F = M->begin(); F != M->end(); ++F) {
      if (!F->hasLocalLinkage()) {
        ExternalCallingNode->addCalledFunction(CallSite(),
                                             getOrInsertFunction(F));
      }
    }
  }

  // Connect <CallsExternalNode>.
  for (Module::iterator F = M->begin(); F != M->end(); ++F) {
    if (F->isDeclaration())
      getOrInsertFunction(F)->addCalledFunction(CallSite(), CallsExternalNode);
  }

  // Build the call graph.
  SiteToFuncs.clear();
  FuncToSites.clear();
  for (Module::iterator F = M->begin(); F != M->end(); ++F) {
    for (Function::iterator BB = F->begin(); BB != F->end(); ++BB) {
      for (BasicBlock::iterator Ins = BB->begin(); Ins != BB->end(); ++Ins) {
        CallSite CS(Ins);
        if (CS.getInstruction())
          processCallSite(CS, AllFuncs);
      }
    }
  }

  // Simplify the call graph.
  simplifyCallGraph();

  return false;
}

void FPCallGraph::simplifyCallGraph() {
  // Remove duplicated items in each vector.
  for (SiteToFuncsMapTy::iterator I = SiteToFuncs.begin();
       I != SiteToFuncs.end(); ++I) {
    uniquifyVector(I->second);
  }
  for (FuncToSitesMapTy::iterator I = FuncToSites.begin();
       I != FuncToSites.end(); ++I) {
    uniquifyVector(I->second);
  }
}

void FPCallGraph::print(llvm::raw_ostream &O, const Module *M) const {
  O << "Caller - Callee:\n";
  for (Module::const_iterator F = M->begin(); F != M->end(); ++F) {
    // All called functions inside <F>.
    FuncList AllCallees;
    for (Function::const_iterator BB = F->begin(); BB != F->end(); ++BB) {
      for (BasicBlock::const_iterator Ins = BB->begin();
           Ins != BB->end(); ++Ins) {
        if (isa<CallInst>(Ins) || isa<InvokeInst>(Ins)) {
          const FuncList &CalledFunctions = getCalledFunctions(Ins);
          for (FuncList::const_iterator I = CalledFunctions.begin();
               I != CalledFunctions.end(); ++I)
            AllCallees.push_back(*I);
        }
      }
    }
    uniquifyVector(AllCallees);
    if (!AllCallees.empty())
    {
      O << "\t" << F->getName() << " calls:\n";
      for (auto& I: AllCallees)
        O << "\t\t" << I->getName() << "\n";
    }
  }
  O << "Callee - Caller:\n";
  for (Module::const_iterator F = M->begin(); F != M->end(); ++F) {
    // All calling functions to <F>.
    const InstList &CallSites = getCallSites(F);
    FuncList AllCallers;
    for (InstList::const_iterator I = CallSites.begin();
         I != CallSites.end(); ++I)
      AllCallers.push_back((*I)->getParent()->getParent());
    uniquifyVector(AllCallers);
    if (!AllCallers.empty()) {
      O << "\t" << F->getName() << " is called by:\n";
      for (FuncList::iterator I = AllCallers.begin();
           I != AllCallers.end(); ++I) {
        O << "\t\t" << (*I)->getName() << "\n";
      }
    }
  }
}

char FPCallGraph::ID = 0;
static RegisterPass<FPCallGraph> X("fpcg", "Call graph that recognizes function pointers", false, true);

}