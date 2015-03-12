#ifndef __DYN_AA_UTILS_H
#define __DYN_AA_UTILS_H

#include <cstdint>

#include "llvm/ADT/StringRef.h"
#include "llvm/IR/Value.h"
#include "llvm/Pass.h"

namespace neongoby {
struct DynAAUtils {
  static const llvm::StringRef MemAllocHookName;
  static const llvm::StringRef MainArgsAllocHookName;
  static const llvm::StringRef TopLevelHookName;
  static const llvm::StringRef EnterHookName;
  static const llvm::StringRef StoreHookName;
  static const llvm::StringRef CallHookName;
  static const llvm::StringRef ReturnHookName;
  static const llvm::StringRef GlobalsAllocHookName;
  static const llvm::StringRef BasicBlockHookName;
  static const llvm::StringRef MemHooksIniterName;
  static const llvm::StringRef AfterForkHookName;
  static const llvm::StringRef BeforeForkHookName;
  static const llvm::StringRef VAStartHookName;
  static const llvm::StringRef SlotsName;

  static void PrintProgressBar(uint64_t Old, uint64_t Now, uint64_t Total);
  static bool PointerIsDereferenced(const llvm::Value *V);
  static void PrintValue(llvm::raw_ostream &O, const llvm::Value *V);
  static bool IsMalloc(const llvm::Function *F);
  static bool IsMallocCall(const llvm::Value *V);
  static bool IsIntraProcQuery(const llvm::Value *V1, const llvm::Value *V2);
  static bool IsReallyIntraProcQuery(const llvm::Value *V1,
                                     const llvm::Value *V2);
  static const llvm::Function *GetContainingFunction(const llvm::Value *V);
};
}

#endif
