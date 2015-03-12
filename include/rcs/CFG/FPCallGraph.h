#ifndef __RCS_FP_CALLGRAPH_H
#define __RCS_FP_CALLGRAPH_H

#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/IR/Module.h"

#include <vector>

namespace rcs
{

struct FPCallGraph: public llvm::ModulePass
{
private:
  using FuncList = std::vector<llvm::Function *>;
  using InstList = std::vector<llvm::Instruction *>;
  using SiteToFuncsMapTy = llvm::DenseMap<const llvm::Instruction *, FuncList>;
  using FuncToSitesMapTy = llvm::DenseMap<const llvm::Function *, InstList>;
  using FuncSet = llvm::DenseSet<llvm::Function *>;

  void processCallSite(const llvm::CallSite &CS, const FuncSet &AllFuncs);
  void simplifyCallGraph();

  SiteToFuncsMapTy SiteToFuncs;
  FuncToSitesMapTy FuncToSites;

  // Copied from CallGraph
  llvm::Module* M;
  using FunctionMapTy = std::map<const llvm::Function *, llvm::CallGraphNode *> ;
  FunctionMapTy FunctionMap;
  llvm::CallGraphNode *Root;
  llvm::CallGraphNode *ExternalCallingNode;
  llvm::CallGraphNode *CallsExternalNode;

public:
  // Copied from CallGraph
  using iterator = FunctionMapTy::iterator;
  using const_iterator = FunctionMapTy::const_iterator;
  llvm::Module &getModule() const { return *M; }
  iterator begin() { return FunctionMap.begin(); }
  iterator end() { return FunctionMap.end(); }
  const_iterator begin() const { return FunctionMap.begin(); }
  const_iterator end() const { return FunctionMap.end(); }

  const llvm::CallGraphNode *operator[](const llvm::Function *F) const
  {
    const_iterator I = FunctionMap.find(F);
    assert(I != FunctionMap.end() && "Function not in callgraph!");
    return I->second;
  }
  llvm::CallGraphNode *operator[](const llvm::Function *F)
  {
    const_iterator I = FunctionMap.find(F);
    assert(I != FunctionMap.end() && "Function not in callgraph!");
    return I->second;
  }
  llvm::Function *removeFunctionFromModule(llvm::CallGraphNode *CGN);
  llvm::CallGraphNode *getOrInsertFunction(const llvm::Function *F);

  static char ID;

  // Interfaces of ModulePass
  FPCallGraph();
  ~FPCallGraph();
  void getAnalysisUsage(llvm::AnalysisUsage &AU) const override;
  bool runOnModule(llvm::Module &M) override;
  void print(llvm::raw_ostream &O, const llvm::Module *M) const override;
  // This method is used when a pass implements
  // an analysis interface through multiple inheritance.  If needed, it
  // should override this to adjust the this pointer as needed for the
  // specified pass info.
  //virtual void *getAdjustedAnalysisPointer(AnalysisID PI);

  // Interfaces of CallGraph
  const llvm::CallGraphNode *getRoot() const { return Root; }
  llvm::CallGraphNode *getRoot() { return Root; }
  // SCC algorithm starts from this external calling node.
  llvm::CallGraphNode *getExternalCallingNode() const {
    return ExternalCallingNode;
  }
  llvm::CallGraphNode *getCallsExternalNode() const { return CallsExternalNode; }

  FuncList getCalledFunctions(const llvm::Instruction *Ins) const;
  InstList getCallSites(const llvm::Function *F) const;

 protected:
  void addCallEdge(const llvm::CallSite &CS, llvm::Function *Callee);
};
}

namespace llvm
{

template <>
struct GraphTraits<rcs::FPCallGraph *>: public GraphTraits<CallGraphNode *>
{
  static NodeType *getEntryNode(rcs::FPCallGraph *CGN)
  {
    return CGN->getExternalCallingNode(); // Start at the external node!
  }
  typedef std::pair<const Function *, CallGraphNode *> PairTy;
  typedef std::pointer_to_unary_function<PairTy, CallGraphNode &> DerefFun;

  // nodes_iterator/begin/end - Allow iteration over all nodes in the graph
  typedef mapped_iterator<rcs::FPCallGraph::iterator, DerefFun> nodes_iterator;
  static nodes_iterator nodes_begin(rcs::FPCallGraph *CG) {
    return map_iterator(CG->begin(), DerefFun(CGdereference));
  }
  static nodes_iterator nodes_end(rcs::FPCallGraph *CG) {
    return map_iterator(CG->end(), DerefFun(CGdereference));
  }

  static CallGraphNode &CGdereference(PairTy P) { return *P.second; }
};

template <>
struct GraphTraits<const rcs::FPCallGraph *> : public GraphTraits<const CallGraphNode *>
{
  static NodeType *getEntryNode(const rcs::FPCallGraph *CGN) {
    return CGN->getExternalCallingNode();
  }
  // nodes_iterator/begin/end - Allow iteration over all nodes in the graph
  typedef rcs::FPCallGraph::const_iterator nodes_iterator;
  static nodes_iterator nodes_begin(const rcs::FPCallGraph *CG) { return CG->begin(); }
  static nodes_iterator nodes_end(const rcs::FPCallGraph *CG) { return CG->end(); }
};
}

#endif
