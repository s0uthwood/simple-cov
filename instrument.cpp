#include <random>
#include <algorithm>
#include <ctime>
#include <iostream>

#include "instrument.h"
#include "config.h"

using namespace llvm;

inline bool is_intrinsic_file_name(const StringRef& name) {
    return name.startswith("__asan_") || name.startswith("asan.") ||
           name.startswith("__sanitizer_") || name.startswith("__inst_") ||
           name.startswith("__cxx_") || name.startswith("__cxa_") ||
           name.startswith("llvm.");
}

inline bool is_intrinsics(const Function& function) {
    return function.isIntrinsic() || is_intrinsic_file_name(function.getName());
}

static FunctionCallee getOrInsertCovFunc(Module &M) {
    LLVMContext &context = M.getContext();
    Type *VoidTy = Type::getVoidTy(context);
    Type *Int32Ty = Type::getInt32Ty(context);
    return M.getOrInsertFunction(
        "__mycov_hit", // 函数名
        VoidTy,         // 返回类型
        Int32Ty
    );
}

void instrumentModule(Module &M) {
    auto covFunc = getOrInsertCovFunc(M);

    // Prepare a shuffled pool of unique branch IDs in [0, MAP_SIZE*8)
    size_t maxBranches = MAP_SIZE * 8;
    std::vector<uint32_t> idPool(maxBranches);
    std::iota(idPool.begin(), idPool.end(), 0);
    uint32_t seed = static_cast<uint32_t>(std::time(nullptr));
    std::mt19937 gen(seed);
    std::shuffle(idPool.begin(), idPool.end(), gen);
    size_t idIndex = 0;

    // uint32_t branchId = 0;

    for (Function &F : M) {
        if (is_intrinsics(F) || F.isDeclaration())
            continue;
        // for (BasicBlock &BB : F) {
        //     Instruction *firstInst = BB.getTerminator();
        //     if (auto *br = dyn_cast<BranchInst>(firstInst)) {
        //         if (br->isConditional()) {
        //             IRBuilder<> builder(br);
        //             Value *idVal = ConstantInt::get(Type::getInt32Ty(M.getContext()), branchId++);
        //             builder.CreateCall(covFunc, {idVal});
        //         }
        //     }
        // }
        for (BasicBlock &BB : F) {
            // 在基本块入口插入打点 call __mycov_hit(bb_id)
            if (idIndex >= idPool.size()) {
                std::cerr << "Error: Not enough unique branch IDs available.\n";
                return;
            }
            uint32_t branchId = idPool[idIndex++];
            IRBuilder<> builder(&*BB.getFirstInsertionPt());
            ConstantInt *id_val = ConstantInt::get(
                Type::getInt32Ty(M.getContext()),
                branchId);
            builder.CreateCall(covFunc, {id_val});
        }
    }
}

void instrumentFile(const std::string &filename) {
    LLVMContext context;
    SMDiagnostic err;
    std::unique_ptr<Module> M = parseIRFile(filename, err, context);
    if (!M) {
        errs() << "Error reading IR file: " << filename << "\n";
        err.print("instrumentFile", errs());
        return;
    }

    instrumentModule(*M);

    std::error_code ec;
    raw_fd_ostream out(filename, ec, sys::fs::OF_None);
    if (ec) {
        errs() << "Could not open file for writing: " << filename << "\n";
        return;
    }
    M->print(out, nullptr);
}
