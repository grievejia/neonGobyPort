#include "Dynamic/Analysis/DynamicAliasAnalysis.h"
#include "Dynamic/Instrument/IDAssigner.h"

#include <llvm/Analysis/BasicAliasAnalysis.h>
#include <llvm/Analysis/CFLAliasAnalysis.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/PassManager.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/raw_ostream.h>

using namespace dynamic;
using namespace llvm;

enum class AAType
{
    CFLAA
};

cl::opt<std::string> InputFilename(cl::Positional, cl::desc("<bitcode file>"));
cl::opt<std::string> LogFilename(cl::Positional, cl::desc("<log file>"));
cl::opt<AAType> AA(cl::Positional, cl::desc("<alias-analysis>"),
                   cl::values(clEnumValN(AAType::CFLAA, "cfl-aa", "CFL-AA"),
                              clEnumValEnd));

void checkAAResult(AAResults& aaResult, const DenseSet<AliasPair>& aliasSet,
                   const IDAssigner& idMap) {
    for (auto const& pair : aliasSet) {
        auto valA = idMap.getValue(pair.getFirst());
        auto valB = idMap.getValue(pair.getSecond());
        if (valA == nullptr || valB == nullptr)
            continue;

        auto aliasResult =
            aaResult.alias(MemoryLocation(valA), MemoryLocation(valB));
        if (aliasResult == NoAlias) {
            outs() << "\nFIND AA BUG:\n";
            outs() << "  ValA = " << *valA << '\n';
            outs() << "  ValB = " << *valB << '\n';
            outs() << "  DynamicAA said DidAlias but the tested AA said "
                      "NoAliasn\n";
        }
    }
}

int main(int argc, char** argv) {
    cl::ParseCommandLineOptions(argc, argv);

    LLVMContext context;
    SMDiagnostic error;
    auto module = parseIRFile(InputFilename, error, context);
    if (!module) {
        error.print(InputFilename.data(), errs());
        return -1;
    }

    // Perform dynamic alias analysis and get all DidAlias pairs
    DynamicAliasAnalysis dynAA(LogFilename.data());
    dynAA.runAnalysis();

    // Set up aa pipeline
    FunctionAnalysisManager funManager;
    funManager.registerPass([] { return TargetLibraryAnalysis(); });
    funManager.registerPass([] { return CFLAA(); });

    AAManager aaManager;
    switch (AA) {
        case AAType::CFLAA:
            aaManager.registerFunctionAnalysis<CFLAA>();
            break;
    }

    IDAssigner idMap(*module);
    for (auto& f : *module) {
        if (auto id = idMap.getID(f)) {
            if (auto aliasSet = dynAA.getAliasPairs(*id)) {
                auto result = aaManager.run(f, funManager);
                checkAAResult(result, *aliasSet, idMap);
            }
        }
    }
}