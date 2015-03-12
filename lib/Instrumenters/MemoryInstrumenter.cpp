#define DEBUG_TYPE "dyn-aa"

#include "rcs/ID/IDAssigner.h"
#include "dyn-aa/Passes.h"
#include "dyn-aa/Utils.h"

#include "llvm/ADT/DenseSet.h"
#include "llvm/IR/CallSite.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Target/TargetLibraryInfo.h"
#include "llvm/Transforms/Utils/BuildLibCalls.h"

using namespace llvm;
using namespace rcs;

static cl::opt<bool> Diagnose("diagnose", cl::desc("Instrument for test case reduction and trace slicing"));
static cl::list<std::string> OfflineWhiteList(
    "offline-white-list", cl::desc("Functions which should be hooked"));

namespace neongoby {
struct MemoryInstrumenter: public ModulePass
{
private:
  static bool IsWhiteListed(const Function &F);

  void instrumentInstructionIfNecessary(Instruction *I);
  void instrumentBasicBlock(BasicBlock *BB);
  // Emit code to handle memory allocation.
  // If <Success>, range [<Start>, <Start> + <Size>) is allocated.
  void instrumentMemoryAllocation(Value *Start, Value *Size, Value *Success, Instruction *Loc);
  void instrumentFork(const CallSite &CS);
  void instrumentMalloc(const CallSite &CS);
  void instrumentAlloca(AllocaInst *AI);
  void instrumentStoreInst(StoreInst *SI);
  void instrumentReturnInst(Instruction *I);
  void instrumentCallSite(CallSite CS);
  void instrumentPointer(Value *ValueOperand, Value *PointerOperand, Instruction *DefLoc);
  void instrumentPointerInstruction(Instruction *I);
  void instrumentPointerParameters(Function& F);
  void instrumentGlobals(Module &M);
  void instrumentMainArgs(Module &M);
  void instrumentVarArgFunction(Function& F);
  void instrumentEntry(Function &F);

  IntrinsicInst *findAnyVAStart(Function& F);
  void checkFeatures(const Module &M);
  void setupScalarTypes(const Module &M);
  void setupHooks(Module &M);
  void lowerGlobalCtors(Module &M);

  // hooks
  Function *MemAllocHook, *TopLevelHook, *EnterHook, *StoreHook;
  Function *MainArgsAllocHook;
  Function *CallHook, *ReturnHook;
  Function *GlobalsAllocHook;
  Function *BasicBlockHook;
  Function *MemHooksIniter;
  Function *AfterForkHook, *BeforeForkHook;
  Function *VAStartHook;
  // the main function
  Function *Main;
  // types
  IntegerType *CharType, *LongType, *IntType;
  PointerType *CharStarType;
  Type *VoidType;
public:
  static char ID;

  MemoryInstrumenter();
  void getAnalysisUsage(AnalysisUsage &AU) const override;
  bool runOnModule(Module &M) override;
};

ModulePass *createMemoryInstrumenterPass()
{
  return new MemoryInstrumenter();
}

void MemoryInstrumenter::getAnalysisUsage(AnalysisUsage &AU) const
{
  AU.addRequired<DataLayoutPass>();
  AU.addRequired<IDAssigner>();
  AU.addRequired<TargetLibraryInfo>();
}

MemoryInstrumenter::MemoryInstrumenter(): ModulePass(ID)
{
  MemAllocHook = nullptr;
  MainArgsAllocHook = nullptr;
  TopLevelHook = nullptr;
  EnterHook = nullptr;
  StoreHook = nullptr;
  CallHook = nullptr;
  ReturnHook = nullptr;
  GlobalsAllocHook = nullptr;
  BasicBlockHook = nullptr;
  VAStartHook = nullptr;
  MemHooksIniter = nullptr;
  Main = nullptr;
  CharType = LongType = IntType = nullptr;
  CharStarType = nullptr;
  VoidType = nullptr;
}

void MemoryInstrumenter::instrumentMainArgs(Module &M) {
  IDAssigner &IDA = getAnalysis<IDAssigner>();

  assert(Main);
  assert(Main->arg_size() == 0 || Main->arg_size() == 2);

  if (Main->arg_size() == 0)
    return;

  Value *Arg1 = Main->arg_begin();
  assert(Arg1->getType() == IntType);
  Value *Arg2 = ++Main->arg_begin();
  assert(Arg2->getType()->isPointerTy());
  assert(cast<PointerType>(Arg2->getType())->getElementType() == CharStarType);

  Value *Args[3] = { Arg1, Arg2, ConstantInt::get(IntType, IDA.getValueID(Arg2))};
  CallInst::Create(MainArgsAllocHook, Args, "", Main->begin()->getFirstNonPHI());
}

void MemoryInstrumenter::instrumentAlloca(AllocaInst *AI) {
  // Calculate where to insert.
  BasicBlock::iterator Loc = AI; ++Loc;

  // start = alloca type
  // =>
  // start = alloca type
  // HookMemAlloc(ValueID, AI, undef)
  // The allocation size is undef temporarily, because Preparer will later change it, and decide the actual allocation size.
  instrumentMemoryAllocation(AI, UndefValue::get(LongType), nullptr, Loc);
}

void MemoryInstrumenter::instrumentMemoryAllocation(Value *Start, Value *Size, Value *Success, Instruction *Loc)
{
  IDAssigner &IDA = getAnalysis<IDAssigner>();

  assert(Start->getType()->isPointerTy());
  assert(Size);
  // The size argument to HookMemAlloc must be long.
  assert(Size->getType() == LongType);
  assert(Success == nullptr || Success->getType()->isIntegerTy(1));
  assert(Loc);

  std::vector<Value *> Args;
  // Arg 1: value ID
  Args.push_back(ConstantInt::get(IntType, IDA.getValueID(Start)));
  // Arg 2: starting address
  if (Start->getType() != CharStarType) {
    Start = new BitCastInst(Start, CharStarType, "", Loc);
  }
  Args.push_back(Start);
  // Arg 3: bound
  Args.push_back(Size);

  // If the allocation is always successful, such as new, we create the memory allocation hook directly; otherwise, we need to check the condition and add the memory allocation hook.
  if (Success == nullptr) {
    CallInst::Create(MemAllocHook, Args, "", Loc);
  } else {
    BasicBlock *BB = Loc->getParent();
    BasicBlock *RestBB = BB->splitBasicBlock(Loc, "rest");
    BasicBlock *CallMallocHookBB = BasicBlock::Create(BB->getContext(), "call_malloc_hook", BB->getParent(), RestBB);
    BB->getTerminator()->eraseFromParent();
    BranchInst::Create(CallMallocHookBB, RestBB, Success, BB);
    CallInst::Create(MemAllocHook, Args, "", CallMallocHookBB);
    BranchInst::Create(RestBB, CallMallocHookBB);
  }
}

void MemoryInstrumenter::instrumentFork(const CallSite &CS) {
  Instruction *Ins = CS.getInstruction();
  assert(!Ins->isTerminator());
  assert(CS.getCalledFunction()->getName() == "fork" || CS.getCalledFunction()->getName() == "vfork");

  BasicBlock::iterator Loc = Ins;
  CallInst::Create(BeforeForkHook, "", Loc);
  ++Loc;
  CallInst::Create(AfterForkHook, Ins, "", Loc);
}

void MemoryInstrumenter::instrumentMalloc(const CallSite &CS)
{
  auto const& dataLayout = getAnalysis<DataLayoutPass>().getDataLayout();
  auto& TLI = getAnalysis<TargetLibraryInfo>();

  Function *Callee = CS.getCalledFunction();
  assert(DynAAUtils::IsMalloc(Callee));

  Instruction *Ins = CS.getInstruction();

  // Calculate where to insert.
  // <Loc> will be the next instruction executed.
  BasicBlock::iterator Loc;
  if (!Ins->isTerminator()) {
    Loc = Ins;
    ++Loc;
  } else {
    assert(isa<InvokeInst>(Ins));
    InvokeInst *II = cast<InvokeInst>(Ins);
    assert(II->getNormalDest()->getUniquePredecessor());
    Loc = II->getNormalDest()->getFirstInsertionPt();
  }

  IRBuilder<> Builder(Loc);
  Value *Start = nullptr;
  Value *Size = nullptr;
  Value *Success = nullptr; // Indicate whether the allocation succeeded.

  StringRef CalleeName = Callee->getName();
  if (CalleeName == "malloc" || CalleeName == "valloc")
  {
    Start = Ins;
    Size = UndefValue::get(LongType);
    Success = Builder.CreateICmpNE(Ins, ConstantPointerNull::get(CharStarType));
  } else if (CalleeName.startswith("_Zn")) {
    Start = Ins;
    Size = CS.getArgument(0);
  } else if (CalleeName == "calloc") {
    // calloc() takes two size_t, i.e. i64.
    // Therefore, no need to worry Mul will have two operands with different types. Also, Size will always be of type i64.
    Start = Ins;
    assert(CS.getArgument(0)->getType() == LongType);
    assert(CS.getArgument(1)->getType() == LongType);
    Size = BinaryOperator::Create(Instruction::Mul, CS.getArgument(0), CS.getArgument(1), "", Loc);
    Success = Builder.CreateICmpNE(Ins, ConstantPointerNull::get(CharStarType));
  } else if (CalleeName == "memalign" || CalleeName == "realloc") {
    Start = Ins;
    Size = CS.getArgument(1);
    Success = Builder.CreateICmpNE(Ins, ConstantPointerNull::get(CharStarType));
  } else if (CalleeName == "strdup" || CalleeName == "__strdup") {
    Start = Ins;
    // Use strlen to compute the length of the allocated memory.
    Value *StrLen = EmitStrLen(Ins, Builder, &dataLayout, &TLI);
    // size = strlen(result) + 1
    Size = Builder.CreateAdd(StrLen, ConstantInt::get(LongType, 1));
    Success = Builder.CreateICmpNE(Ins, ConstantPointerNull::get(CharStarType));
  } else if (CalleeName == "getline") {
    // getline(char **lineptr, size_t *n, FILE *stream)
    // start = *lineptr
    // size = *n
    // succ = (<rv> != -1)
    Start = Builder.CreateLoad(CS.getArgument(0));
    Size = Builder.CreateLoad(CS.getArgument(1));
    Success = Builder.CreateICmpNE(Ins, ConstantInt::get(Ins->getType(), -1));
  } else {
    llvm_unreachable("Unhandled malloc function call");
  }

  //      start = malloc(size)
  //      if (success)
  //        HookMemAlloc
  // Loc:
  instrumentMemoryAllocation(Start, Size, Success, Loc);
}

void MemoryInstrumenter::checkFeatures(const Module &M)
{
  // Check whether any memory allocation function can
  // potentially be pointed by function pointers.
  // Also, all intrinsic functions will be called directly,
  // i.e. not via function pointers.
  for (auto const& f: M)
  {
    if (DynAAUtils::IsMalloc(&f) || f.isIntrinsic())
    {
      for (auto user: f.users())
      {
        ImmutableCallSite cs(user);
        assert(cs);
        for (unsigned i = 0; i < cs.arg_size(); ++i)
          if (cs.getArgument(i) == &f)
            llvm_unreachable("intrinsic or malloc called indirectly!");
      }
    }
  }

  // Check whether memory allocation functions are captured.
  for (auto const& f: M)
  {
    // 0 is the return, 1 is the first parameter.
    if (f.isDeclaration() && f.doesNotAlias(0) && !DynAAUtils::IsMalloc(&f))
    {
      errs().changeColor(raw_ostream::RED);
      errs() << f.getName() << "'s return value is marked noalias, ";
      errs() << "but the function is not treated as malloc.\n";
      errs().resetColor();
    }
  }

  // Global variables shouldn't be of the array type.
  for (auto const& g: M.globals())
  {
    if (g.getType()->isArrayTy())
    {
      errs().changeColor(raw_ostream::RED);
      errs() << "Global var " << g.getName() << " is an array\n";
      errs().resetColor();
    }
  }

  // A function parameter or an instruction can be an array, but we don't instrument such constructs for now. Issue a warning on such cases.
  for (auto const& f: M)
  {
    for (auto const& arg: f.args())
    {
      if (arg.getType()->isArrayTy())
      {
        errs().changeColor(raw_ostream::RED);
        errs() << f.getName() << ": param " << arg << " is an array\n";
        errs().resetColor();
      }
    }
  }

  for (auto const& f: M)
  {
    for (auto const& bb: f)
    {
      for (auto const& inst: bb)
      {
        if (inst.getType()->isArrayTy())
        {
          errs().changeColor(raw_ostream::RED);
          errs() << f.getName() << ": instruction " << inst << " is an array\n";
          errs().resetColor();
        }
      }
    }
  }
}

void MemoryInstrumenter::setupHooks(Module &M) {
  // No existing functions have the same name.
  assert(M.getFunction(DynAAUtils::MemAllocHookName) == nullptr);
  assert(M.getFunction(DynAAUtils::MainArgsAllocHookName) == nullptr);
  assert(M.getFunction(DynAAUtils::TopLevelHookName) == nullptr);
  assert(M.getFunction(DynAAUtils::EnterHookName) == nullptr);
  assert(M.getFunction(DynAAUtils::StoreHookName) == nullptr);
  assert(M.getFunction(DynAAUtils::CallHookName) == nullptr);
  assert(M.getFunction(DynAAUtils::ReturnHookName) == nullptr);
  assert(M.getFunction(DynAAUtils::GlobalsAllocHookName) == nullptr);
  assert(M.getFunction(DynAAUtils::BasicBlockHookName) == nullptr);
  assert(M.getFunction(DynAAUtils::MemHooksIniterName) == nullptr);
  assert(M.getFunction(DynAAUtils::AfterForkHookName) == nullptr);
  assert(M.getFunction(DynAAUtils::BeforeForkHookName) == nullptr);

  // Setup MemAllocHook.
  std::vector<Type *> ArgTypes;
  ArgTypes.push_back(IntType);
  ArgTypes.push_back(CharStarType);
  ArgTypes.push_back(LongType);
  FunctionType *MemAllocHookType = FunctionType::get(VoidType, ArgTypes, false);
  MemAllocHook = Function::Create(MemAllocHookType, GlobalValue::ExternalLinkage, DynAAUtils::MemAllocHookName, &M);

  // Setup MainArgsAllocHook.
  ArgTypes.clear();
  ArgTypes.push_back(IntType);
  ArgTypes.push_back(PointerType::getUnqual(CharStarType));
  ArgTypes.push_back(IntType);
  FunctionType *MainArgsAllocHookType = FunctionType::get(VoidType, ArgTypes, false);
  MainArgsAllocHook = Function::Create(MainArgsAllocHookType, GlobalValue::ExternalLinkage, DynAAUtils::MainArgsAllocHookName, &M);

  // Setup MemHooksIniter.
  FunctionType *MemHooksIniterType = FunctionType::get(VoidType, false);
  MemHooksIniter = Function::Create(MemHooksIniterType, GlobalValue::ExternalLinkage, DynAAUtils::MemHooksIniterName, &M);

  // Setup TopLevelHook.
  ArgTypes.clear();
  ArgTypes.push_back(CharStarType);
  ArgTypes.push_back(CharStarType);
  ArgTypes.push_back(IntType);
  FunctionType *TopLevelHookType = FunctionType::get(VoidType, ArgTypes, false);
  TopLevelHook = Function::Create(TopLevelHookType, GlobalValue::ExternalLinkage, DynAAUtils::TopLevelHookName, &M);

  // Setup EnterHook.
  FunctionType *EnterHookType = FunctionType::get(VoidType, IntType, false);
  EnterHook = Function::Create(EnterHookType, GlobalValue::ExternalLinkage, DynAAUtils::EnterHookName, &M);

  // Setup StoreHook.
  ArgTypes.clear();
  ArgTypes.push_back(CharStarType);
  ArgTypes.push_back(CharStarType);
  ArgTypes.push_back(IntType);
  FunctionType *StoreHookType = FunctionType::get(VoidType, ArgTypes, false);
  StoreHook = Function::Create(StoreHookType, GlobalValue::ExternalLinkage, DynAAUtils::StoreHookName, &M);

  // Setup CallHook.
  ArgTypes.clear();
  ArgTypes.push_back(IntType);
  ArgTypes.push_back(IntType);
  FunctionType *CallHookType = FunctionType::get(VoidType, ArgTypes, false);
  CallHook = Function::Create(CallHookType, GlobalValue::ExternalLinkage, DynAAUtils::CallHookName, &M);

  // Setup ReturnHook.
  ArgTypes.clear();
  ArgTypes.push_back(IntType);
  ArgTypes.push_back(IntType);
  FunctionType *ReturnHookType = FunctionType::get(VoidType, ArgTypes, false);
  ReturnHook = Function::Create(ReturnHookType, GlobalValue::ExternalLinkage, DynAAUtils::ReturnHookName, &M);

  // Setup GlobalsAccessHook.
  FunctionType *GlobalsAllocHookType = FunctionType::get(VoidType, false);
  GlobalsAllocHook = Function::Create(GlobalsAllocHookType, GlobalValue::ExternalLinkage, DynAAUtils::GlobalsAllocHookName, &M);

  // Setup BasicBlockHook.
  ArgTypes.clear();
  ArgTypes.push_back(IntType);
  FunctionType *BasicBlockHookType = FunctionType::get(VoidType, ArgTypes, false);
  BasicBlockHook = Function::Create(BasicBlockHookType, GlobalValue::ExternalLinkage, DynAAUtils::BasicBlockHookName, &M);

  // Setup AfterForkHook
  ArgTypes.clear();
  ArgTypes.push_back(IntType);
  FunctionType *AfterForkHookType = FunctionType::get(VoidType, ArgTypes, false);
  AfterForkHook = Function::Create(AfterForkHookType, GlobalValue::ExternalLinkage, DynAAUtils::AfterForkHookName, &M);

  // Setup BeforeForkHook
  ArgTypes.clear();
  FunctionType *BeforeForkHookType = FunctionType::get(VoidType, false);
  BeforeForkHook = Function::Create(BeforeForkHookType, GlobalValue::ExternalLinkage, DynAAUtils::BeforeForkHookName, &M);

  // Setup VAStartHook
  FunctionType *VAStartHookType = FunctionType::get(VoidType, CharStarType, false);
  VAStartHook = Function::Create(VAStartHookType, GlobalValue::ExternalLinkage, DynAAUtils::VAStartHookName, &M);
}

void MemoryInstrumenter::setupScalarTypes(const Module &M)
{
  VoidType = Type::getVoidTy(M.getContext());
  CharType = Type::getInt8Ty(M.getContext());
  CharStarType = PointerType::getUnqual(CharType);
  LongType = Type::getIntNTy(M.getContext(), sizeof(long) * 8);
  IntType = Type::getIntNTy(M.getContext(), sizeof(int) * 8);
}

void MemoryInstrumenter::instrumentGlobals(Module &M) {
  auto const& dataLayout = getAnalysis<DataLayoutPass>().getDataLayout();
  IDAssigner &IDA = getAnalysis<IDAssigner>();

  // Function HookGlobalsAlloc contains only one basic block.
  // The BB iterates through all global variables, and calls HookMemAlloc for each of them.
  BasicBlock *BB = BasicBlock::Create(M.getContext(), "entry", GlobalsAllocHook);
  Instruction *Ret = ReturnInst::Create(M.getContext(), BB);

  for (auto& g: M)
  {
    // We are going to delete llvm.global_ctors.
    // Therefore, don't instrument it.
    if (g.getName() == "llvm.global_ctors")
      continue;
    // Prevent global variables from sharing the same address, because it breaks the assumption that global variables do not alias.
    // The same goes to functions.
    if (g.hasUnnamedAddr())
      g.setUnnamedAddr(false);
    uint64_t TypeSize = dataLayout.getTypeStoreSize(g.getType()->getElementType());
    instrumentMemoryAllocation(&g, ConstantInt::get(LongType, TypeSize), nullptr, Ret);
    instrumentPointer(&g, nullptr, Ret);
  }

  for (auto& f: M)
  {
    // InvalidID: maybe this is inserted by alias checker in hybrid mode.
    if (IDA.getValueID(&f) == IDAssigner::InvalidID)
      continue;
    // Ignore intrinsic functions because we cannot take the address of an intrinsic. Also, no function pointers will point to instrinsic functions.
    if (f.isIntrinsic())
      continue;
    // Prevent functions from sharing the same address.
    if (f.hasUnnamedAddr())
      f.setUnnamedAddr(false);
    uint64_t TypeSize = dataLayout.getTypeStoreSize(f.getType());
    assert(TypeSize == dataLayout.getPointerSize());
    instrumentMemoryAllocation(&f, ConstantInt::get(LongType, TypeSize), nullptr, Ret);
    instrumentPointer(&f, nullptr, Ret);
  }
}

bool MemoryInstrumenter::IsWhiteListed(const Function &F) {
  // TODO: now the whitelist is short. if it is long, we should use a hash set
  if (OfflineWhiteList.size() != 0) {
    for (unsigned i = 0; i < OfflineWhiteList.size(); i++) {
      if (OfflineWhiteList[i] == F.getName()) {
        return true;
      }
    }
    return false;
  }
  return true;
}

bool MemoryInstrumenter::runOnModule(Module &M) {
  // Check whether there are unsupported language features.
  checkFeatures(M);

  // Setup scalar types.
  setupScalarTypes(M);

  // Find the main function.
  Main = M.getFunction("main");
  assert(Main && !Main->isDeclaration() && !Main->hasLocalLinkage());

  // Setup hook function declarations.
  setupHooks(M);

  // Hook global variable allocations.
  instrumentGlobals(M);

  // Hook memory allocations and memory accesses.
  for (auto& f: M)
  {
    if (f.isDeclaration())
      continue;
    if (!IsWhiteListed(f))
      continue;
    // The second argument of main(int argc, char *argv[]) needs special handling, which is done in instrumentMainArgs.
    // We should treat argv as a memory allocation instead of a regular pointer.
    if (Main != &f)
      instrumentPointerParameters(f);
    if (f.isVarArg())
      instrumentVarArgFunction(f);
    for (auto& bb: f)
    {
      if (Diagnose)
        instrumentBasicBlock(&bb);
      for (auto& inst: bb)
        instrumentInstructionIfNecessary(&inst);
    }
    instrumentEntry(f);
  }

  // main(argc, argv)
  // argv is allocated by outside.
  instrumentMainArgs(M);

  // Lower global constructors.
  lowerGlobalCtors(M);

  // Call the memory hook initializer and the global variable allocation hook at the very beginning.
  Instruction *OldEntry = Main->begin()->getFirstNonPHI();
  CallInst::Create(MemHooksIniter, "", OldEntry);
  CallInst::Create(GlobalsAllocHook, "", OldEntry);

  return true;
}

void MemoryInstrumenter::lowerGlobalCtors(Module &M) {
  // Find llvm.global_ctors.
  GlobalVariable *GV = M.getNamedGlobal("llvm.global_ctors");
  if (!GV)
    return;
  assert(!GV->isDeclaration() && !GV->hasLocalLinkage());

  // Should be an array of '{ int, void ()* }' structs.  The first value is the init priority, which must be 65535 if the bitcode is generated using clang.
  if (ConstantArray *InitList = dyn_cast<ConstantArray>(GV->getInitializer())) {
    for (unsigned i = 0, e = InitList->getNumOperands(); i != e; ++i) {
      ConstantStruct *CS =
        dyn_cast<ConstantStruct>(InitList->getOperand(i));
      assert(CS);
      assert(CS->getNumOperands() == 2);

      // Get the priority.
      ConstantInt *Priority = dyn_cast<ConstantInt>(CS->getOperand(0));
      assert(Priority);
      // TODO: For now, we assume all priorities must be 65535.
      assert(Priority->equalsInt(65535));

      // Get the constructor function.
      Constant *FP = CS->getOperand(1);
      if (FP->isNullValue())
        break;  // Found a null terminator, exit.

      // Explicitly call the constructor at the main entry.
      CallInst::Create(FP, "", Main->begin()->getFirstNonPHI());
    }
  }

  // Clear the global_ctors array.
  // Use eraseFromParent() instead of removeFromParent().
  GV->eraseFromParent();
}

void MemoryInstrumenter::instrumentStoreInst(StoreInst *SI) {
  IDAssigner &IDA = getAnalysis<IDAssigner>();

  Value *ValueStored = SI->getValueOperand();
  const Type *ValueType = ValueStored->getType();
  if (ValueType == LongType || ValueType->isPointerTy()) {
    std::vector<Value *> Args;

    if (ValueType == LongType) {
      Args.push_back(new IntToPtrInst(ValueStored, CharStarType, "", SI));
    } else {
      Args.push_back(new BitCastInst(ValueStored, CharStarType, "", SI));
    }

    Args.push_back(new BitCastInst(SI->getPointerOperand(), CharStarType, "", SI));

    unsigned InsID = IDA.getInstructionID(SI);
    assert(InsID != IDAssigner::InvalidID);
    Args.push_back(ConstantInt::get(IntType, InsID));

    CallInst::Create(StoreHook, Args, "", SI);
  }
}

void MemoryInstrumenter::instrumentReturnInst(Instruction *I)
{
  IDAssigner &IDA = getAnalysis<IDAssigner>();
  unsigned InsID = IDA.getInstructionID(I);
  assert(InsID != IDAssigner::InvalidID);
  unsigned FuncID = IDA.getFunctionID(I->getParent()->getParent());
  assert(FuncID != IDAssigner::InvalidID);

  std::vector<Value *> Args;
  Args.push_back(ConstantInt::get(IntType, FuncID));
  Args.push_back(ConstantInt::get(IntType, InsID));
  CallInst::Create(ReturnHook, Args, "", I);
}

void MemoryInstrumenter::instrumentCallSite(CallSite CS)
{
  IDAssigner &IDA = getAnalysis<IDAssigner>();
  unsigned InsID = IDA.getInstructionID(CS.getInstruction());
  assert(InsID != IDAssigner::InvalidID);

  int NumCallingArgs = CS.arg_size();
  std::vector<Value *> Args;
  Args.push_back(ConstantInt::get(IntType, InsID));
  Args.push_back(ConstantInt::get(IntType, NumCallingArgs));
  CallInst::Create(CallHook, Args, "", CS.getInstruction());
}

IntrinsicInst *MemoryInstrumenter::findAnyVAStart(Function& F)
{
  for (auto& bb: F)
  {
    for (auto& inst: bb)
    {
      if (auto II = dyn_cast<IntrinsicInst>(&inst))
      {
        if (II->getIntrinsicID() == Intrinsic::vastart)
          return II;
      }
    }
  }
  return nullptr;
}

void MemoryInstrumenter::instrumentVarArgFunction(Function& F)
{
  IntrinsicInst *VAStart = findAnyVAStart(F);
  assert(VAStart && "cannot find any llvm.va_start");
  BitCastInst *ArrayDecay = cast<BitCastInst>(VAStart->getOperand(0));
  assert(ArrayDecay->getType() == CharStarType);

  // The source of the bitcast does not have to be an alloca. In unoptimized bitcode, it's likely a GEP. In that case, we need track further.
  Instruction *Alloca = ArrayDecay;
  while (!isa<AllocaInst>(Alloca)) {
    Alloca = cast<Instruction>(Alloca->getOperand(0));
  }

  // Clone Alloca, ArrayDecay, and VAStart, and replace their operands.
  Instruction *ClonedAlloca = Alloca->clone();
  Instruction *ClonedArrayDecay = ArrayDecay->clone();
  Instruction *ClonedVAStart = VAStart->clone();
  ClonedArrayDecay->setOperand(0, ClonedAlloca);
  ClonedVAStart->setOperand(0, ClonedArrayDecay);

  // Insert the cloned instructions to the entry block.
  BasicBlock::iterator InsertPos = F.begin()->begin();
  BasicBlock::InstListType &InstList = F.begin()->getInstList();
  InstList.insert(InsertPos, ClonedAlloca);
  InstList.insert(InsertPos, ClonedArrayDecay);
  InstList.insert(InsertPos, ClonedVAStart);

  // Hook the llvm.va_start.
  CallInst::Create(VAStartHook, ClonedArrayDecay, "", InsertPos);
}

void MemoryInstrumenter::instrumentEntry(Function &F) {
  IDAssigner &IDA = getAnalysis<IDAssigner>();
  unsigned FuncID = IDA.getFunctionID(&F);
  // Skip the functions added by us.
  if (FuncID != IDAssigner::InvalidID) {
    CallInst::Create(EnterHook, ConstantInt::get(IntType, FuncID), "", F.begin()->getFirstInsertionPt());
  }
}

void MemoryInstrumenter::instrumentInstructionIfNecessary(Instruction *I) {
  // Skip those instructions added by us.
  IDAssigner &IDA = getAnalysis<IDAssigner>();
  if (IDA.getValueID(I) == IDAssigner::InvalidID)
    return;

  // Instrument pointer stores, i.e. store X *, X **.
  // store long, long * is considered as a pointer store as well.
  if (StoreInst *SI = dyn_cast<StoreInst>(I)) {
    instrumentStoreInst(SI);
    return;
  }

  // Instrument returns and resume.
  if (isa<ReturnInst>(I) || isa<ResumeInst>(I)) {
    instrumentReturnInst(I);
    return;
  }

  // Any instructions of a pointer type, including mallocs and AllocaInsts.
  // Call instrumentPointerInstruction before instrumentMalloc so that HookMemAlloc will be added before HookTopLevel which prevents us from using an outdated version number.
  if (I->getType()->isPointerTy())
    instrumentPointerInstruction(I);

  CallSite CS(I);
  if (CS) {
    // Instrument memory allocation function calls.
    // TODO: A function pointer can possibly point to memory allocation or memory free functions. We don't handle this case for now.
    // We added a feature check. The pass will assertion fail upon such cases.
    Function *Callee = CS.getCalledFunction();
    if (Callee && DynAAUtils::IsMalloc(Callee))
      instrumentMalloc(CS);
    if (Diagnose || Callee == nullptr || Callee->isVarArg()) {
      // Instrument a callsite if we are in the diagnosis mode (for TraceSlicer and Reducer), or it has variable length arguments.
      instrumentCallSite(CS);
    }
    // Instrument fork() to support multiprocess programs.
    // Instrument fork() at last, because it flushes the logs.
    if (Callee &&
        (Callee->getName() == "fork" || Callee->getName() == "vfork")) {
      instrumentFork(CS);
    }
  }

  // Instrument AllocaInsts.
  if (AllocaInst *AI = dyn_cast<AllocaInst>(I))
    instrumentAlloca(AI);
}

void MemoryInstrumenter::instrumentBasicBlock(BasicBlock *BB) {
  IDAssigner &IDA = getAnalysis<IDAssigner>();
  unsigned ValueID = IDA.getValueID(BB);
  if (ValueID != IDAssigner::InvalidID) {
    CallInst::Create(BasicBlockHook,
                     ConstantInt::get(IntType,  ValueID),
                     "",
                     BB->getFirstNonPHI());
  }
}

void MemoryInstrumenter::instrumentPointerInstruction(Instruction *I)
{
  BasicBlock::iterator Loc;
  if (isa<PHINode>(I)) {
    // Cannot insert hooks right after a PHI, because PHINodes have to be grouped together.
    Loc = I->getParent()->getFirstNonPHI();
  } else if (!I->isTerminator())
  {
    Loc = I;
    ++Loc;
  } else {
    assert(isa<InvokeInst>(I));
    InvokeInst *II = cast<InvokeInst>(I);
    BasicBlock *NormalDest = II->getNormalDest();
    // It's not always OK to insert HookTopLevel simply at the beginning of the normal destination, because the normal destionation may be shared by multiple InvokeInsts. In that case, we will create a critical edge block, and add the HookTopLevel over there.
    if (NormalDest->getUniquePredecessor())
    {
      Loc = NormalDest->getFirstNonPHI();
    } else {
      BasicBlock *CritEdge = BasicBlock::Create(I->getContext(), "crit_edge", I->getParent()->getParent());
      Loc = BranchInst::Create(NormalDest, CritEdge);
      // Now that CritEdge becomes the new predecessor of NormalDest, replace all phi uses of I->getParent() with CritEdge.
      for (auto J = NormalDest->begin(); NormalDest->getFirstNonPHI() != J; ++J)
      {
        PHINode *Phi = cast<PHINode>(J);
        int i;
        while ((i = Phi->getBasicBlockIndex(I->getParent())) >= 0)
          Phi->setIncomingBlock(i, CritEdge);
      }
      II->setNormalDest(CritEdge);
    }
  }
  if (LoadInst *LI = dyn_cast<LoadInst>(I))
    instrumentPointer(I, LI->getPointerOperand(), Loc);
  else
    instrumentPointer(I, nullptr, Loc);
}

// If ValueOperand is a LoadInst, PointerOperand is the pointer operand; otherwise, it is nullptr.
void MemoryInstrumenter::instrumentPointer(Value *ValueOperand, Value *PointerOperand, Instruction *DefLoc)
{
  IDAssigner &IDA = getAnalysis<IDAssigner>();

  assert(ValueOperand->getType()->isPointerTy());

  unsigned ValueID = IDA.getValueID(ValueOperand);
  // Skip the values that don't exist in the original program.
  if (ValueID == IDAssigner::InvalidID) {
    return;
  }

  // Add a hook to define this pointer.
  std::vector<Value *> Args;
  Args.push_back(new BitCastInst(ValueOperand, CharStarType, "", DefLoc));
  if (PointerOperand != nullptr)
    Args.push_back(new BitCastInst(PointerOperand, CharStarType, "", DefLoc));
  else
    Args.push_back(ConstantPointerNull::get(CharStarType));
  Args.push_back(ConstantInt::get(IntType, ValueID));
  CallInst::Create(TopLevelHook, Args, "", DefLoc);
}

void MemoryInstrumenter::instrumentPointerParameters(Function& F)
{
  assert(!F.isDeclaration());

  auto const& dataLayout = getAnalysis<DataLayoutPass>().getDataLayout();

  Instruction *Entry = F.begin()->getFirstInsertionPt();
  for (auto& arg: F.args())
  {
    if (PointerType *ArgType = dyn_cast<PointerType>(arg.getType()))
    {
      // If an argument is marked as byval, add an implicit allocation.
      // FIXME: still broken. We need allocate one extra byte for it. We'd better do it in the backend.
      if (arg.hasByValAttr())
      {
        uint64_t TypeSize = dataLayout.getTypeStoreSize(ArgType->getElementType());
        instrumentMemoryAllocation(&arg, ConstantInt::get(LongType, TypeSize), nullptr, Entry);
      }
      instrumentPointer(&arg, nullptr, Entry);
    }
  }
}

char MemoryInstrumenter::ID = 0;
static RegisterPass<MemoryInstrumenter> X("instrument-memory", "Instrument memory operations", false, false);

}