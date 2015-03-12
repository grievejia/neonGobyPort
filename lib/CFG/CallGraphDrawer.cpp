#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

static cl::opt<std::string> DotFileName("cg-dot", cl::desc("The output graph file name"));
static cl::opt<bool> ShouldPrintStats("cg-stats", cl::desc("Print stat info of CallGraph"));

namespace rcs
{

class CallGraphDrawer: public ModulePass
{
private:
  void printStats(raw_ostream &O, Module &M);
  void printToDot(raw_ostream &O, Module &M);
  void printCallEdgesFrom(raw_ostream &O, CallGraphNode *CallerNode);
  void printCallEdge(raw_ostream &O, Function *Caller, Function *Callee);
public:
  static char ID;

  CallGraphDrawer(): ModulePass(ID) {}
  void getAnalysisUsage(AnalysisUsage &AU) const override;
  bool runOnModule(Module &M) override;
};

void CallGraphDrawer::getAnalysisUsage(AnalysisUsage &AU) const
{
  AU.setPreservesAll();
  AU.addRequired<CallGraphWrapperPass>();
}

bool CallGraphDrawer::runOnModule(Module &M) {
  if (DotFileName != "") {
    std::error_code ErrorInfo;
    raw_fd_ostream DotFile(DotFileName.c_str(), ErrorInfo, sys::fs::F_None);
    printToDot(DotFile, M);
  }

  if (ShouldPrintStats)
  {
    printStats(errs(), M);
  }

  return false;
}

void CallGraphDrawer::printCallEdgesFrom(raw_ostream &O, CallGraphNode *CallerNode)
{
  assert(CallerNode);
  for (unsigned i = 0; i < CallerNode->size(); ++i)
  {
    auto CalleeNode = (*CallerNode)[i];
    printCallEdge(O, CallerNode->getFunction(), CalleeNode->getFunction());
  }
}

void CallGraphDrawer::printCallEdge(raw_ostream &O, Function *Caller, Function *Callee)
{
  assert((Caller || Callee) && "Both caller and callee are null");
  O << (Caller ? "\"F_" + Caller->getName() + "\"": "ExternCallingNode");
  O << " -> ";
  O << (Callee ? "\"F_" + Callee->getName() + "\"": "CallsExternNode");
  O << "\n";
}

void CallGraphDrawer::printToDot(raw_ostream &O, Module &M)
{
  CallGraphWrapperPass &CG = getAnalysis<CallGraphWrapperPass>();

  O << "strict digraph CallGraph {\n";

  // CallsExternNode and ExternCallingNode
  O << "CallsExternNode [label = bottom]\n";
  O << "ExternCallingNode [label = top]\n";
  // One node for each function. 
  for (Module::iterator F = M.begin(); F != M.end(); ++F) {
    O << "\"F_" << F->getName() << "\" ";
    O << "[label = \"" << F->getName() << "\"]\n";
  }

  // What does ExternCallingNode call? 
  printCallEdgesFrom(O, CG.getExternalCallingNode());
  // What does each function call? 
  for (Module::iterator F = M.begin(); F != M.end(); ++F)
    printCallEdgesFrom(O, CG[F]);
  
  O << "}\n";
}

void CallGraphDrawer::printStats(raw_ostream &O, Module &M)
{
  using Distribution = std::multimap<unsigned, Function *, std::greater<unsigned>>;

  CallGraphWrapperPass &CG = getAnalysis<CallGraphWrapperPass>();

  // We ignore edges coming from or going to the two external nodes. 
  unsigned numFunctions = 0, numCallEdges = 0;
  Distribution InDegreeDist, OutDegreeDist;
  for (auto& f: M)
  {
    auto node = CG[&f];
    assert(node);
    ++numFunctions;
    numCallEdges += node->size();
    InDegreeDist.insert(std::make_pair(node->getNumReferences(), &f));
    OutDegreeDist.insert(std::make_pair(node->size(), &f));
  }

  // Note that there might be multiple edges between two different
  // functions, because we count call edges for each call site. 
  O << "# of functions = " << numFunctions << "\n";
  O << "# of call edges = " << numCallEdges << "\n";

  O << "Top ten in degrees:\n";
  unsigned Limit = 10;
  for (Distribution::iterator I = InDegreeDist.begin();
       I != InDegreeDist.end() && Limit > 0; ++I, --Limit) {
    O << "  " << I->second->getName() << ": " << I->first << "\n";
  }

  O << "Top ten out degrees:\n";
  Limit = 10;
  for (Distribution::iterator I = OutDegreeDist.begin();
       I != OutDegreeDist.end() && Limit > 0; ++I, --Limit) {
    O << "  " << I->second->getName() << ": " << I->first << "\n";
  }
}

char CallGraphDrawer::ID = 0;
static RegisterPass<CallGraphDrawer> X("draw-cg", "Draw the call graph", false, true);

}
