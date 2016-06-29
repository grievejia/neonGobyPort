#include "Dynamic/Instrument/MemoryInstrument.h"
#include "Dynamic/Instrument/AllocType.h"
#include "Dynamic/Instrument/DynamicHooks.h"
#include "Dynamic/Instrument/FeatureCheck.h"
#include "Dynamic/Instrument/IDAssigner.h"

#include <llvm/IR/CallSite.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/raw_ostream.h>

using namespace llvm;

namespace dynamic {

namespace {

BasicBlock::iterator nextInsertionPos(Instruction& inst) {
    BasicBlock::iterator loc(&inst);
    ++loc;
    return loc;
}

bool isMalloc(const Function* f) {
    auto fName = f->getName();
    return fName == "malloc" || fName == "calloc" || fName == "valloc" ||
           fName == "strdup" || fName == "_Znwj" || fName == "_Znwm" ||
           fName == "_Znaj" || fName == "_Znam" || fName == "getline";
}

class Instrumenter
{
private:
    DynamicHooks& hooks;
    const IDAssigner& idMap;

    LLVMContext& context;

    size_t getID(const Value& v) const {
        auto id = idMap.getID(v);
        assert(id != nullptr && "ID not found");
        return *id;
    }
    Type* getIntType() { return Type::getIntNTy(context, sizeof(int) * 8); }
    Type* getLongType() { return Type::getIntNTy(context, sizeof(size_t) * 8); }
    Type* getCharType() { return Type::getInt8Ty(context); }
    Type* getCharPtrType() { return PointerType::getUnqual(getCharType()); }

    void instrumentPointer(Value*, Instruction*);
    void instrumentAllocation(AllocType, Value*, Instruction*);
    void instrumentGlobals(Module&);
    void instrumentFunction(Function&);
    void instrumentFunctionParams(Function&);
    void instrumentMain(Function&);
    void instrumentEntry(Function&);

    void instrumentInst(Instruction&);
    void instrumentExit(Instruction&);
    void instrumentAlloca(AllocaInst&);
    void instrumentCall(CallSite cs);
    void instrumentPointerInst(Instruction&);
    void instrumentMalloc(CallSite cs);

public:
    Instrumenter(DynamicHooks& d, const IDAssigner& i, LLVMContext& c)
        : hooks(d), idMap(i), context(c) {}

    void instrument(Module&);
};

void Instrumenter::instrumentPointer(Value* val, Instruction* pos) {
    assert(val != nullptr && pos != nullptr && val->getType()->isPointerTy());
    auto id = getID(*val);

    auto idArg = ConstantInt::get(getIntType(), id);
    if (val->getType() != getCharPtrType())
        val = new BitCastInst(val, getCharPtrType(), "ptr", pos);
    CallInst::Create(hooks.getPointerHook(), {idArg, val}, "", pos);
}

void Instrumenter::instrumentAllocation(AllocType allocType, Value* ptr,
                                        Instruction* pos) {
    assert(ptr != nullptr && pos != nullptr && ptr->getType()->isPointerTy());

    auto allocTypeArg = ConstantInt::get(getCharType(), allocType);
    auto ptrId = getID(*ptr);
    if (ptr->getType() != getCharPtrType())
        ptr = new BitCastInst(ptr, getCharPtrType(), "alloc_ptr", pos);
    auto idArg = ConstantInt::get(getIntType(), ptrId);
    CallInst::Create(hooks.getAllocHook(), {allocTypeArg, idArg, ptr}, "", pos);
}

void Instrumenter::instrumentGlobals(Module& module) {
    auto bb = BasicBlock::Create(context, "entry", hooks.getGlobalHook());
    auto retInst = ReturnInst::Create(context, bb);

    // Global values
    for (auto& global : module.globals()) {
        // Prevent global variables from sharing the same address, because it
        // breaks the assumption that global variables do not alias.
        if (global.hasAtLeastLocalUnnamedAddr())
            global.setUnnamedAddr(GlobalValue::UnnamedAddr::None);

        instrumentAllocation(AllocType::Global, &global, retInst);
    }

    // Functions
    for (auto& f : module) {
        if (f.isIntrinsic())
            continue;
        if (hooks.isHook(f))
            continue;

        if (f.hasAtLeastLocalUnnamedAddr())
            f.setUnnamedAddr(GlobalValue::UnnamedAddr::None);

        instrumentAllocation(AllocType::Global, &f, retInst);
    }
}

void Instrumenter::instrumentFunctionParams(Function& f) {
    auto entry = f.begin()->getFirstInsertionPt();
    for (auto& arg : f.args()) {
        if (arg.getType()->isPointerTy()) {
            if (arg.hasByValAttr()) {
                instrumentAllocation(AllocType::Stack, &arg, &*entry);
            }

            instrumentPointer(&arg, &*entry);
        }
    }
}

void Instrumenter::instrumentEntry(Function& f) {
    auto entry = f.begin()->getFirstInsertionPt();
    auto id = getID(f);
    auto idArg = ConstantInt::get(getIntType(), id);
    CallInst::Create(hooks.getEnterHook(), {idArg}, "", &*entry);
}

void Instrumenter::instrumentExit(Instruction& inst) {
    auto func = inst.getParent()->getParent();
    auto funcID = getID(*func);
    auto idArg = ConstantInt::get(getIntType(), funcID);
    CallInst::Create(hooks.getExitHook(), {idArg}, "", &inst);
}

void Instrumenter::instrumentAlloca(AllocaInst& allocInst) {
    auto pos = nextInsertionPos(allocInst);
    instrumentAllocation(AllocType::Stack, &allocInst, &*pos);
}

void Instrumenter::instrumentMalloc(CallSite cs) {
    // We don't handle invoke for now
    assert(!cs.isInvoke() && "Not supported yet");

    auto pos = nextInsertionPos(*cs.getInstruction());
    instrumentAllocation(AllocType::Heap, cs.getInstruction(), &*pos);
}

void Instrumenter::instrumentCall(CallSite cs) {
    // Instrument memory allocation function calls.
    // TODO: A function pointer can possibly point to memory allocation or
    // memory free functions. We don't handle this case for now.
    auto callee = cs.getCalledFunction();
    auto inst = cs.getInstruction();

    if (callee && isMalloc(callee))
        instrumentMalloc(cs);
    else {
        // CallHook must be inserted before the call actually happens
        auto id = getID(*inst);
        auto idArg = ConstantInt::get(getIntType(), id);
        CallInst::Create(hooks.getCallHook(), {idArg}, "", inst);

        // If the call returns a pointer, record it
        if (inst->getType()->isPointerTy())
            instrumentPointer(inst, &*nextInsertionPos(*inst));
    }
}

void Instrumenter::instrumentPointerInst(Instruction& inst) {
    BasicBlock::iterator pos;
    if (isa<PHINode>(inst)) {
        // Cannot insert hooks right after a PHI, because PHINodes have to be
        // grouped together
        auto pos = inst.getParent()->getFirstNonPHI();
        instrumentPointer(&inst, &*pos);
    } else if (!inst.isTerminator()) {
        auto pos = nextInsertionPos(inst);
        instrumentPointer(&inst, &*pos);
    }
}

void Instrumenter::instrumentInst(Instruction& inst) {
    // Do not touch the instrumented codes
    if (idMap.getID(inst) == nullptr)
        return;

    if (isa<ReturnInst>(inst) || isa<ResumeInst>(inst))
        instrumentExit(inst);
    else if (auto allocInst = dyn_cast<AllocaInst>(&inst))
        instrumentAlloca(*allocInst);
    else if (isa<CallInst>(inst) || isa<InvokeInst>(inst))
        instrumentCall(CallSite(&inst));
    else if (isa<StoreInst>(inst))
        return;
    else if (inst.getType()->isPointerTy())
        instrumentPointerInst(inst);
}

void Instrumenter::instrumentMain(Function& mainFunc) {
    assert(mainFunc.getName() == "main");

    auto pos = mainFunc.begin()->getFirstInsertionPt();
    CallInst::Create(hooks.getInitHook(), "", &*pos);
    CallInst::Create(hooks.getGlobalHook(), "", &*pos);

    if (mainFunc.arg_size() > 0) {
        assert(mainFunc.arg_size() >= 2);
        auto argItr = mainFunc.arg_begin();
        ++argItr;
        Value* argv = &*argItr;
        auto argvId = getID(*argv);
        auto idArg = ConstantInt::get(getIntType(), argvId);

        Value* envp =
            ConstantPointerNull::get(cast<PointerType>(getCharPtrType()));
        Constant* envpArg = ConstantInt::get(getIntType(), 0);
        ++argItr;
        if (argItr != mainFunc.arg_end()) {
            envp = &*argItr;
            auto envpId = getID(*envp);
            envpArg = ConstantInt::get(getIntType(), envpId);
        }

        CallInst::Create(hooks.getMainHook(), {idArg, argv, envpArg, envp}, "",
                         &*pos);
    }

    auto id = getID(mainFunc);
    auto idArg = ConstantInt::get(getIntType(), id);
    CallInst::Create(hooks.getEnterHook(), {idArg}, "", &*pos);
}

void Instrumenter::instrumentFunction(Function& f) {
    if (f.isDeclaration())
        return;

    if (hooks.isHook(f))
        return;

    for (auto& bb : f)
        for (auto& inst : bb)
            instrumentInst(inst);

    if (f.getName() == "main")
        instrumentMain(f);
    else {
        instrumentFunctionParams(f);
        instrumentEntry(f);
    }
}

void Instrumenter::instrument(Module& module) {
    instrumentGlobals(module);

    for (auto& f : module)
        instrumentFunction(f);
}
}

void MemoryInstrument::runOnModule(Module& module) {
    // Check unsupported features in the input IR and issue warnings accordingly
    FeatureCheck().runOnModule(module);

    IDAssigner idMap(module);
    DynamicHooks hooks(module);

    Instrumenter(hooks, idMap, module.getContext()).instrument(module);
}
}
