#ifndef __DYN_AA_DYNAMIC_ALIAS_ANALYSIS_H
#define __DYN_AA_DYNAMIC_ALIAS_ANALYSIS_H

#include "rcs/typedefs.h"

#include "dyn-aa/IntervalTree.h"
#include "dyn-aa/LogRecord.h"
#include "dyn-aa/LogProcessor.h"

#include "llvm/Pass.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/Analysis/AliasAnalysis.h"

#include <stack>

namespace neongoby
{

class DynamicAliasAnalysis: public llvm::ModulePass, public llvm::AliasAnalysis, public LogProcessor
{
private:
  using Location = std::pair<void *, unsigned>;
  // ValueID + Version#
  using Definition = std::pair<unsigned, unsigned>;

  // Returns the current version of <Addr>.
  unsigned lookupAddress(void *Addr) const;
  void updateVersion(void *Start, unsigned long Bound, unsigned Version);
  void removePointsTo(unsigned InvocationID);
  void removePointsTo(Definition Ptr);
  // Helper function called by removePointsTo.
  void removePointedBy(Definition Ptr, Location Loc);
  void addPointsTo(Definition Ptr, Location Loc);
  // A convenient wrapper for a batch of reports.
  void addAliasPairs(Definition P, const llvm::DenseSet<Definition> &Qs);
  // Adds two values to DidAlias if their contexts match.
  void addAliasPair(Definition P, Definition Q);
  // Report <V1, V2> as an alias pair.
  // This function canonicalizes the pair, so that <V1, V2> and
  // <V2, V1> are considered the same.
  void addAliasPair(llvm::Value *V1, llvm::Value *V2);

  // An interval tree that maps addresses to version numbers.
  // We need store version numbers because pointing to the same address is not enough to claim two pointers alias.
  IntervalTree<unsigned> AddressVersion;
  unsigned CurrentVersion;
  // 2-way mapping indicating the current address of each pointer
  llvm::DenseMap<Location, llvm::DenseSet<Definition>> PointedBy;
  llvm::DenseMap<Definition, Location> PointsTo;
  // Stores all alias pairs.
  llvm::DenseSet<std::pair<const llvm::Value*, const llvm::Value*>> Aliases;
  // Pointers that ever point to unversioned addresses.
  llvm::DenseSet<llvm::Value*> PointersVersionUnknown;
  // Addresses whose version is unknown.
  llvm::DenseSet<void *> AddressesVersionUnknown;
  // This global variable gets incremented each time a function is called. Each pointer will be associated with the invocation ID to gain context-sensitivity.
  unsigned NumInvocations;
  // Thread-specific call stack.
  std::stack<unsigned> CallStack;
  // Pointers in PointsTo and PointedBy. Indexed by invocation ID so that we can quickly find out what pointers to delete given a function.
  llvm::DenseMap<unsigned, std::vector<unsigned>> ActivePointers;
  // Outdated contexts of a function.
  llvm::DenseMap<unsigned, llvm::DenseSet<unsigned>> OutdatedContexts;
public:

  static char ID;
  static const unsigned UnknownVersion;

  DynamicAliasAnalysis(): llvm::ModulePass(ID) {}
  bool runOnModule(llvm::Module &M) override;
  void getAnalysisUsage(llvm::AnalysisUsage &AU) const override;

  // Interfaces of AliasAnalysis.
  llvm::AliasAnalysis::AliasResult alias(const llvm::AliasAnalysis::Location &L1, const llvm::AliasAnalysis::Location &L2) override;
  void *getAdjustedAnalysisPointer(llvm::AnalysisID PI) override;

  // Interfaces of LogProcessor.
  void processMemAlloc(const MemAllocRecord &Record) override;
  void processTopLevel(const TopLevelRecord &Record) override;
  void processEnter(const EnterRecord &Record) override;
  void processReturn(const ReturnRecord &Record) override;
  void initialize() override;

  auto& getAllAliases() const { return Aliases; }
};
}

#endif
