#define DEBUG_TYPE "dyn-aa"

#include "rcs/ID/IDAssigner.h"

#include "dyn-aa/BaselineAliasAnalysis.h"
#include "dyn-aa/DynamicAliasAnalysis.h"
#include "dyn-aa/Utils.h"

#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/CommandLine.h"

using namespace llvm;
using namespace rcs;

static cl::opt<bool> IntraProc("intra", cl::desc("Whether the checked AA supports intra-procedural queries only"));
static cl::opt<bool> PrintValueInReport("print-value-in-report", cl::desc("print values in the report"), cl::init(true));

STATISTIC(NumDynamicAliases, "Number of dynamic aliases");

namespace neongoby
{

static std::pair<const Function*, const Function*> GetContainingFunctionPair(
    const std::pair<const Value*, const Value*> &VP)
{
  return std::make_pair(DynAAUtils::GetContainingFunction(VP.first), DynAAUtils::GetContainingFunction(VP.second));
}

class AliasAnalysisChecker: public ModulePass
{
private:
  using ValuePair = std::pair<const Value*, const Value*>;

  void collectDynamicAliases(DenseSet<ValuePair> &DynamicAliases);
  void collectMissingAliases(const DenseSet<ValuePair> &DynamicAliases);
  void reportMissingAliases();

  std::vector<ValuePair> MissingAliases;
public:
  static char ID;

  AliasAnalysisChecker(): ModulePass(ID) {}
  void getAnalysisUsage(AnalysisUsage &AU) const override;
  bool runOnModule(Module &M) override;
};

void AliasAnalysisChecker::getAnalysisUsage(AnalysisUsage &AU) const
{
  AU.setPreservesAll();

  AU.addRequired<DynamicAliasAnalysis>();
  AU.addRequired<AliasAnalysis>();
  AU.addRequired<BaselineAliasAnalysis>();
  AU.addRequired<IDAssigner>();
}

void AliasAnalysisChecker::collectDynamicAliases(
    DenseSet<ValuePair> &DynamicAliases)
{
  DynamicAliases.clear();
  auto& DAA = getAnalysis<DynamicAliasAnalysis>();
  DynamicAliases.insert(DAA.getAllAliases().begin(), DAA.getAllAliases().end());
}

// Collects missing aliases to <MissingAliases>.
void AliasAnalysisChecker::collectMissingAliases(
    const DenseSet<ValuePair> &DynamicAliases)
{
  AliasAnalysis &AA = getAnalysis<AliasAnalysis>();
  AliasAnalysis &BaselineAA = getAnalysis<BaselineAliasAnalysis>();

  MissingAliases.clear();
  for (auto const& pair: DynamicAliases) {
    auto V1 = pair.first, V2 = pair.second;
    if (IntraProc && !DynAAUtils::IsIntraProcQuery(V1, V2)) {
      continue;
    }

    if (BaselineAA.alias(V1, V2) != AliasAnalysis::NoAlias &&
        AA.alias(V1, V2) == AliasAnalysis::NoAlias) {
      MissingAliases.push_back(std::make_pair(V1, V2));
    }
  }
}

bool AliasAnalysisChecker::runOnModule(Module &M)
{
  DenseSet<ValuePair> DynamicAliases;
  collectDynamicAliases(DynamicAliases);
  NumDynamicAliases = DynamicAliases.size();
  collectMissingAliases(DynamicAliases);
  reportMissingAliases();

  return false;
}

void AliasAnalysisChecker::reportMissingAliases() {
  IDAssigner &IDA = getAnalysis<IDAssigner>();

  std::vector<ValuePair> ReportedMissingAliases;
  for (size_t i = 0; i < MissingAliases.size(); ++i) {
    auto V1 = MissingAliases[i].first, V2 = MissingAliases[i].second;

    // Do not report BitCasts and PhiNodes. The reports on them are typically redundant.
    if (isa<BitCastInst>(V1) || isa<BitCastInst>(V2))
      continue;
    if (isa<PHINode>(V1) || isa<PHINode>(V2))
      continue;

    ReportedMissingAliases.push_back(MissingAliases[i]);
  }
  std::sort(
    ReportedMissingAliases.begin(),
    ReportedMissingAliases.end(),
    [] (const ValuePair &VP1, const ValuePair &VP2)
    {
      return GetContainingFunctionPair(VP1) < GetContainingFunctionPair(VP2);
    }
  );

  for (size_t i = 0; i < ReportedMissingAliases.size(); ++i)
  {
    auto V1 = ReportedMissingAliases[i].first;
    auto V2 = ReportedMissingAliases[i].second;

    errs().changeColor(raw_ostream::RED);
    errs() << "Missing alias:";
    errs().resetColor();

    // Print some features of this missing alias.
    errs() << (DynAAUtils::IsIntraProcQuery(V1, V2) ? " (intra)" : " (inter)");
    if (DynAAUtils::PointerIsDereferenced(V1) &&
        DynAAUtils::PointerIsDereferenced(V2)) {
      errs() << " (deref)";
    } else {
      errs() << " (non-deref)";
    }
    errs() << "\n";

    errs() << "[" << IDA.getValueID(V1) << "] ";
    if (PrintValueInReport)
      DynAAUtils::PrintValue(errs(), V1);
    errs() << "\n";

    errs() << "[" << IDA.getValueID(V2) << "] ";
    if (PrintValueInReport)
      DynAAUtils::PrintValue(errs(), V2);
    errs() << "\n";
  }

  if (ReportedMissingAliases.empty()) {
    errs().changeColor(raw_ostream::GREEN);
    errs() << "Congrats! You passed all the tests.\n";
    errs().resetColor();
  } else {
    errs().changeColor(raw_ostream::RED);
    errs() << "Detected " << ReportedMissingAliases.size() <<
        " missing aliases.\n";
    errs().resetColor();
  }
}

char AliasAnalysisChecker::ID = 0;
static RegisterPass<AliasAnalysisChecker> X("check-aa", "Check whether the alias analysis is sound", false, true);

}
