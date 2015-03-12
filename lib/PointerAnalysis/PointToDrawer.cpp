#include "rcs/ID/IDAssigner.h"
#include "rcs/PointerAnalysis/PointerAnalysis.h"

#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/raw_ostream.h"

#include <unordered_set>

using namespace llvm;

static cl::opt<std::string> DotFileName("pointer-dot", cl::desc("The output graph file name"));
static cl::opt<bool> ShouldPrintStat("pointer-stats", cl::desc("Print stat info of PointerAnalysis"));

namespace rcs
{
struct PointToDrawer: public ModulePass
{
  static char ID;

  PointToDrawer(): ModulePass(ID) {}
  void getAnalysisUsage(AnalysisUsage &AU) const override;
  bool runOnModule(Module &M) override;

 private:
  // Print the point-to graph as a .dot file. 
  void printToDot(raw_ostream &O);
};

void PointToDrawer::getAnalysisUsage(AnalysisUsage &AU) const
{
  AU.setPreservesAll();
  AU.addRequired<IDAssigner>();
  AU.addRequired<PointerAnalysis>();
}

void PointToDrawer::printToDot(raw_ostream &O) {
  IDAssigner &IDA = getAnalysis<IDAssigner>();
  PointerAnalysis &PA = getAnalysis<PointerAnalysis>();

  O << "strict digraph PointTo {\n";

  std::unordered_set<unsigned> PointerVids, PointeeVids;
  std::vector<Value*> Pointers;
  PA.getAllPointers(Pointers);
  for (size_t i = 0; i < Pointers.size(); ++i) {
    Value *Pointer = Pointers[i];
    assert(Pointer != NULL);
    assert(Pointer->getType()->isPointerTy());

    unsigned PointerVid = IDA.getValueID(Pointer);
    assert(PointerVid != IDAssigner::InvalidID);
    PointerVids.insert(PointerVid);

    std::vector<Value*> Pointees;
    PA.getPointees(Pointer, Pointees);

    for (size_t j = 0; j < Pointees.size(); ++j) {
      Value *Pointee = Pointees[j];
      assert(Pointee != NULL);

      unsigned PointeeVid = IDA.getValueID(Pointee);
      assert(PointeeVid != IDAssigner::InvalidID);
      PointeeVids.insert(PointeeVid);
      
      O << "TopLevel" << PointerVid << " -> AddrTaken" << PointeeVid << "\n";
    }
  }
  
  for (auto i: PointerVids) {
    O << "TopLevel" << i << " ";
    O << "[label = " << i << "]\n";
  }

  for (auto i: PointeeVids) {
    O << "AddrTaken" << i << " ";
    O << "[label = " << i << ", ";
    O << "style = filled, ";
    O << "fillcolor = yellow]\n";
  }

  O << "}\n";
}

bool PointToDrawer::runOnModule(Module &M) {
  PointerAnalysis &PA = getAnalysis<PointerAnalysis>();

  if (DotFileName != "") {
    std::error_code ErrorInfo;
    raw_fd_ostream DotFile(DotFileName.c_str(), ErrorInfo, sys::fs::F_None);
    printToDot(DotFile);
  }

  if (ShouldPrintStat) {
    PA.printStats(errs());
  }

  return false;
}

char PointToDrawer::ID = 0;
static RegisterPass<PointToDrawer> X("draw-point-to", "Draw point-to graphs", false, true);

}