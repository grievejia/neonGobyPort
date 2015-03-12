#include "rcs/CFG/ICFGBuilder.h"
#include "rcs/CFG/FPCallGraph.h"

#include "llvm/IR/CFG.h"

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

static inline bool isReturn(const Instruction *I)
{
  return isa<ReturnInst>(I) || isa<ResumeInst>(I);
}

ICFGBuilder::ICFGBuilder(): ModulePass(ID) {}

void ICFGBuilder::getAnalysisUsage(AnalysisUsage &AU) const
{
  AU.setPreservesAll();
  AU.addRequired<MicroBasicBlockBuilder>();
  AU.addRequired<FPCallGraph>();
}

bool ICFGBuilder::runOnModule(Module &M)
{
  MicroBasicBlockBuilder &MBBB = getAnalysis<MicroBasicBlockBuilder>();
  FPCallGraph &CG = getAnalysis<FPCallGraph>();

  for (auto& f: M)
    for (auto& bb: f)
    {
      for (mbb_iterator mi = MBBB.begin(&bb), E = MBBB.end(&bb); mi != E; ++mi)
        getOrInsertMBB(mi);
    }

  for (auto& f: M)
    for (auto& bb: f)
    {
      for (mbb_iterator mi = MBBB.begin(&bb), E = MBBB.end(&bb); mi != E; ++mi) {
        // The ICFG will not contain any inter-thread edge. 
        // It's also difficult to handle them. How to deal with the return edges? They are supposed to go to the pthread_join sites. 
        if (mi->end() != bb.end() && !isPthreadCreate(mi->end()))
        {
          FuncList callees = CG.getCalledFunctions(mi->end());
          bool calls_decl = false;
          for (auto callee : callees)
          {
            if (callee->isDeclaration()) {
              calls_decl = true;
            } else {
              MicroBasicBlock *entry_mbb = MBBB.begin(callee->begin());
              addEdge(mi, entry_mbb);
            }
          }
          if (calls_decl) {
            mbb_iterator next_mbb = mi; ++next_mbb;
            addEdge(mi, next_mbb);
          }
        } else {
          for (succ_iterator si = succ_begin(&bb); si != succ_end(&bb); ++si) {
            MicroBasicBlock *succ_mbb = MBBB.begin(*si);
            addEdge(mi, succ_mbb);
          }
          TerminatorInst *ti = bb.getTerminator();
          if (isReturn(ti)) {
            InstList call_sites = CG.getCallSites(bb.getParent());
            for (auto call_site : call_sites) {
              
              // Ignore inter-thread edges. 
              if (isPthreadCreate(call_site))
                continue;
              MicroBasicBlock *next_mbb;
              if (isa<CallInst>(call_site)) {
                BasicBlock::iterator next = call_site;
                ++next;
                next_mbb = MBBB.parent(next);
              } else {
                assert(isa<InvokeInst>(call_site));
                InvokeInst *inv = dyn_cast<InvokeInst>(call_site);
                if (isa<ReturnInst>(ti)) {
                  next_mbb = MBBB.begin(inv->getNormalDest());
                } else {
                  next_mbb = MBBB.begin(inv->getUnwindDest());
                }
              }
              addEdge(mi, next_mbb);
            }
          }
        }
      }
    }
  return false;
}

char ICFGBuilder::ID = 0;
static RegisterPass<ICFGBuilder> X("icfg", "Build inter-procedural control flow graph", false, true);

}