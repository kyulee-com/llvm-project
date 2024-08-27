//===-- StructuralHash.cpp - IR Hashing -------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/IR/StructuralHash.h"
#include "llvm/ADT/StableHashing.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Module.h"

using namespace llvm;

namespace {

// Basic hashing mechanism to detect structural change to the IR, used to verify
// pass return status consistency with actual change. In addition to being used
// by the MergeFunctions pass.

class StructuralHashImpl {
  stable_hash Hash = 4;

  bool DetailedHash;

  IgnoreOperandFunc IgnoreOp;

  std::unique_ptr<IndexInstructionMapType> IndexInstructionMap = nullptr;
  std::unique_ptr<IndexPairOperandHashMapType> IndexPairOperandHashMap =
      nullptr;

  DenseMap<const Value *, int> ValueToId;

  // This will produce different values on 32-bit and 64-bit systens as
  // hash_combine returns a size_t. However, this is only used for
  // detailed hashing which, in-tree, only needs to distinguish between
  // differences in functions.
  // TODO: deprecate. This is not stable as it depends on hash_combine().
  template <typename T> stable_hash hashArbitaryType(const T &V) {
    return hash_combine(V);
  }

  stable_hash hashType(Type *Ty) {
    SmallVector<stable_hash> Hashes;
#if 0 
    if (Ty->isIntegerTy())
      Hashes.emplace_back(Ty->getIntegerBitWidth());
#endif
    Hashes.emplace_back(Ty->getTypeID());

    switch (Ty->getTypeID()) {
    default:
      break;
    case Type::IntegerTyID:
      Hashes.emplace_back(Ty->getIntegerBitWidth());
      break;
    case Type::PointerTyID:
      Hashes.emplace_back(dyn_cast<PointerType>(Ty)->getAddressSpace());
      break;
    case Type::StructTyID: {
      StructType *ST = cast<StructType>(Ty);
      Hashes.emplace_back(ST->getNumElements());
      Hashes.emplace_back(ST->isPacked());
      for (unsigned I = 0; I < ST->getNumElements(); ++I)
        Hashes.emplace_back(hashType(ST->getElementType(I)));
      break;
    }
    case Type::FunctionTyID: {
      FunctionType *FT = cast<FunctionType>(FT);
      Hashes.emplace_back(FT->getNumParams());
      Hashes.emplace_back(FT->isVarArg());
      Hashes.emplace_back(hashType(FT->getReturnType()));
      for (unsigned I = 0; I < FT->getNumParams(); ++I)
        Hashes.emplace_back(hashType(FT->getParamType(I)));
      break;
    }
    case Type::ArrayTyID: {
      auto *AT = cast<ArrayType>(Ty);
      Hashes.emplace_back(AT->getNumElements());
      Hashes.emplace_back(hashType(AT->getElementType()));
      break;
    }
    case Type::FixedVectorTyID:
    case Type::ScalableVectorTyID: {
      auto *VT = cast<VectorType>(Ty);
      Hashes.emplace_back(VT->getElementCount().isScalable());
      Hashes.emplace_back(VT->getElementCount().getKnownMinValue());
      Hashes.emplace_back(hashType(VT->getElementType()));
      break;
    }
    }

    return stable_hash_combine(Hashes);
  }

public:
  StructuralHashImpl() = delete;
  explicit StructuralHashImpl(bool DetailedHash,
                              IgnoreOperandFunc IgnoreOp = nullptr)
      : DetailedHash(DetailedHash), IgnoreOp(IgnoreOp) {
    if (IgnoreOp) {
      IndexInstructionMap = std::make_unique<IndexInstructionMapType>();
      IndexPairOperandHashMap = std::make_unique<IndexPairOperandHashMapType>();
    }
  }

  // hash_value for APInt should be stable
  stable_hash hashAPInt(const APInt &I) { return hash_value(I); }

  stable_hash hashAPFloat(const APFloat &F) {
    SmallVector<stable_hash> Hashes;
    const fltSemantics &S = F.getSemantics();
    Hashes.emplace_back(APFloat::semanticsPrecision(S));
    Hashes.emplace_back(APFloat::semanticsMaxExponent(S));
    Hashes.emplace_back(APFloat::semanticsMinExponent(S));
    Hashes.emplace_back(APFloat::semanticsSizeInBits(S));
    Hashes.emplace_back(hashAPInt(F.bitcastToAPInt()));
    return stable_hash_combine(Hashes);
  }

  stable_hash hashGlobalValue(const GlobalValue *GV) {
#if 0
    auto *GVar = dyn_cast<GlobalVariable>(GV);
    if (GVar) {
      stable_hash ObjCSpecHash = stableObjCSpecGV(GVar);
      if (ObjCSpecHash)
        return hash(ObjCSpecHash, Hash);
    }
#endif
    if (!GV->hasName())
      return 0;

    // TODO: use stable_hash_name(Name) once it's available.
    SmallVector<stable_hash> Hashes;
    auto Name = GV->getName();
    // Use the content hash of the outlined function.
    auto [P0, S0] = Name.rsplit(".content.");
    if (!S0.empty())
      Hashes.emplace_back(xxh3_64bits(P0));
    else {
      auto [P1, S1] = Name.rsplit(".llvm.");
      auto [P2, S2] = P1.rsplit(".__uniq.");
      Hashes.emplace_back(xxh3_64bits(P2));
    }
    return stable_hash_combine(Hashes);
  }

  stable_hash hashConstant(const Constant *C) {
    SmallVector<stable_hash> Hashes;

    Type *Ty = C->getType();
    Hashes.emplace_back(hashType(Ty));

    if (C->isNullValue()) {
      Hashes.emplace_back(static_cast<stable_hash>('N'));
      return stable_hash_combine(Hashes);
    }

    auto *G = dyn_cast<GlobalValue>(C);
    if (G) {
      Hashes.emplace_back(hashGlobalValue(G));
      return stable_hash_combine(Hashes);
    }

    if (const auto *Seq = dyn_cast<ConstantDataSequential>(C)) {
      Hashes.emplace_back(xxh3_64bits(Seq->getRawDataValues()));
      return stable_hash_combine(Hashes);
    }

    switch (C->getValueID()) {
    case Value::UndefValueVal:
    case Value::PoisonValueVal:
    case Value::ConstantTokenNoneVal: {
      return stable_hash_combine(Hashes);
    }
    case Value::ConstantIntVal: {
      const APInt &Int = cast<ConstantInt>(C)->getValue();
      Hashes.emplace_back(hashAPInt(Int));
      return stable_hash_combine(Hashes);
    }
    case Value::ConstantFPVal: {
      const APFloat &APF = cast<ConstantFP>(C)->getValueAPF();
      Hashes.emplace_back(hashAPFloat(APF));
      return stable_hash_combine(Hashes);
    }
    case Value::ConstantArrayVal: {
      const ConstantArray *A = cast<ConstantArray>(C);
      uint64_t NumElements = cast<ArrayType>(Ty)->getNumElements();
      Hashes.emplace_back(NumElements);
      for (uint64_t i = 0; i < NumElements; ++i) {
        auto H = hashConstant(cast<Constant>(A->getOperand(i)));
        Hashes.emplace_back(H);
      }
      return stable_hash_combine(Hashes);
    }
    case Value::ConstantStructVal: {
      const ConstantStruct *S = cast<ConstantStruct>(C);
      unsigned NumElements = cast<StructType>(Ty)->getNumElements();
      Hashes.emplace_back(NumElements);
      for (unsigned i = 0; i != NumElements; ++i) {
        auto H = hashConstant(cast<Constant>(S->getOperand(i)));
        Hashes.emplace_back(H);
      }
      return stable_hash_combine(Hashes);
    }
    case Value::ConstantVectorVal: {
      const ConstantVector *V = cast<ConstantVector>(C);
      unsigned NumElements = cast<FixedVectorType>(Ty)->getNumElements();
      Hashes.emplace_back(NumElements);
      for (unsigned i = 0; i != NumElements; ++i) {
        auto H = hashConstant(cast<Constant>(V->getOperand(i)));
        Hashes.emplace_back(H);
      }
      return stable_hash_combine(Hashes);
    }
    case Value::ConstantExprVal: {
      const ConstantExpr *E = cast<ConstantExpr>(C);
      unsigned NumOperands = E->getNumOperands();
      Hashes.emplace_back(NumOperands);
      for (unsigned i = 0; i < NumOperands; ++i) {
        auto H = hashConstant(cast<Constant>(E->getOperand(i)));
        Hashes.emplace_back(H);
      }
      // TODO: GEPOperator
      return stable_hash_combine(Hashes);
    }
    case Value::BlockAddressVal: {
      const BlockAddress *BA = cast<BlockAddress>(C);
      auto H = hashGlobalValue(BA->getFunction());
      Hashes.emplace_back(H);
      // TODO: handle BBs in the same function. can we reference a block
      // in another TU?
      return stable_hash_combine(Hashes);
    }
    case Value::DSOLocalEquivalentVal: {
      const auto *Equiv = cast<DSOLocalEquivalent>(C);
      auto H = hashGlobalValue(Equiv->getGlobalValue());
      Hashes.emplace_back(H);
      return stable_hash_combine(Hashes);
    }
    default: // Unknown constant, abort.
      llvm_unreachable("Constant ValueID not recognized.");
    }
    return Hash;
  }

  /// Hash a value in order similar to FunctionCompartor::cmpValue().
  /// If this is the first time the value are seen, it's added to the mapping
  /// so that we can use its index for hash computation.
  stable_hash hashValue(Value *V) {
    SmallVector<stable_hash> Hashes;

    // check const
    const Constant *C = dyn_cast<Constant>(V);
    if (C) {
      Hashes.emplace_back(hashConstant(C));
      return stable_hash_combine(Hashes);
    }
    if (Argument *Arg = dyn_cast<Argument>(V))
      Hashes.emplace_back(Arg->getArgNo());

    // TODO: Inline asm

    // Map non-constant value to an index
    auto I = ValueToId.insert({V, ValueToId.size()});
    Hashes.emplace_back(I.first->second);
    return stable_hash_combine(Hashes);
  }

  stable_hash hashOperand(Value *Operand) {
    SmallVector<stable_hash> Hashes;
    Hashes.emplace_back(hashType(Operand->getType()));

    Hashes.emplace_back(hashValue(Operand));
    return stable_hash_combine(Hashes);
#if 0
    // The cases enumerated below are not exhaustive and are only aimed to
    // get decent coverage over the function.
    if (ConstantInt *ConstInt = dyn_cast<ConstantInt>(Operand)) {
      Hashes.emplace_back(hashArbitaryType(ConstInt->getValue()));
    } else if (ConstantFP *ConstFP = dyn_cast<ConstantFP>(Operand)) {
      Hashes.emplace_back(hashArbitaryType(ConstFP->getValue()));
    } else if (Argument *Arg = dyn_cast<Argument>(Operand)) {
      Hashes.emplace_back(Arg->getArgNo());
    } else if (Function *Func = dyn_cast<Function>(Operand)) {
      // Hashing the name will be deterministic as LLVM's hashing infrastructure
      // has explicit support for hashing strings and will not simply hash
      // the pointer.
      Hashes.emplace_back(hashArbitaryType(Func->getName()));
    }

    return stable_hash_combine(Hashes);
#endif
  }

  stable_hash hashInstruction(const Instruction &Inst) {
    SmallVector<stable_hash> Hashes;
    Hashes.emplace_back(Inst.getOpcode());

    if (!DetailedHash)
      return stable_hash_combine(Hashes);

    Hashes.emplace_back(hashType(Inst.getType()));

    // Handle additional properties of specific instructions that cause
    // semantic differences in the IR.
    // TODO: expand cmpOperations for different type of instructions
    if (const auto *ComparisonInstruction = dyn_cast<CmpInst>(&Inst))
      Hashes.emplace_back(ComparisonInstruction->getPredicate());

    unsigned InstIdx = 0;
    if (IndexInstructionMap) {
      InstIdx = IndexInstructionMap->size();
      IndexInstructionMap->insert({InstIdx, const_cast<Instruction *>(&Inst)});
    }

    for (unsigned OpndIdx = 0; OpndIdx < Inst.getNumOperands(); ++OpndIdx) {
      auto *Op = Inst.getOperand(OpndIdx);
      auto OpndHash = hashOperand(Op);
      if (IgnoreOp && IgnoreOp(&Inst, OpndIdx)) {
        assert(IndexPairOperandHashMap);
        IndexPairOperandHashMap->insert({{InstIdx, OpndIdx}, OpndHash});
      } else
        Hashes.emplace_back(OpndHash);
    }

    return stable_hash_combine(Hashes);
  }

  // A function hash is calculated by considering only the number of arguments
  // and whether a function is varargs, the order of basic blocks (given by the
  // successors of each basic block in depth first order), and the order of
  // opcodes of each instruction within each of these basic blocks. This mirrors
  // the strategy FunctionComparator::compare() uses to compare functions by
  // walking the BBs in depth first order and comparing each instruction in
  // sequence. Because this hash currently does not look at the operands, it is
  // insensitive to things such as the target of calls and the constants used in
  // the function, which makes it useful when possibly merging functions which
  // are the same modulo constants and call targets.
  //
  // Note that different users of StructuralHash will want different behavior
  // out of it (i.e., MergeFunctions will want something different from PM
  // expensive checks for pass modification status). When modifying this
  // function, most changes should be gated behind an option and enabled
  // selectively.
  void update(const Function &F) {
    // Declarations don't affect analyses.
    if (F.isDeclaration())
      return;

    SmallVector<stable_hash> Hashes;
    Hashes.emplace_back(Hash);
    Hashes.emplace_back(0x62642d6b6b2d6b72); // Function header

    Hashes.emplace_back(F.isVarArg());
    Hashes.emplace_back(F.arg_size());

    SmallVector<const BasicBlock *, 8> BBs;
    SmallPtrSet<const BasicBlock *, 16> VisitedBBs;

    // Walk the blocks in the same order as
    // FunctionComparator::cmpBasicBlocks(), accumulating the hash of the
    // function "structure." (BB and opcode sequence)
    BBs.push_back(&F.getEntryBlock());
    VisitedBBs.insert(BBs[0]);
    while (!BBs.empty()) {
      const BasicBlock *BB = BBs.pop_back_val();

      // This random value acts as a block header, as otherwise the partition of
      // opcodes into BBs wouldn't affect the hash, only the order of the
      // opcodes
      Hashes.emplace_back(45798);
      for (auto &Inst : *BB)
        Hashes.emplace_back(hashInstruction(Inst));

      for (const BasicBlock *Succ : successors(BB))
        if (VisitedBBs.insert(Succ).second)
          BBs.push_back(Succ);
    }

    // Update the combined hash in place.
    Hash = stable_hash_combine(Hashes);
  }

  void update(const GlobalVariable &GV) {
    // Declarations and used/compiler.used don't affect analyses.
    // Since there are several `llvm.*` metadata, like `llvm.embedded.object`,
    // we ignore anything with the `.llvm` prefix
    if (GV.isDeclaration() || GV.getName().starts_with("llvm."))
      return;
    SmallVector<stable_hash> Hashes;
    Hashes.emplace_back(Hash);
    Hashes.emplace_back(23456); // Global header
    Hashes.emplace_back(GV.getValueType()->getTypeID());

    // Update the combined hash in place.
    Hash = stable_hash_combine(Hashes);
  }

  void update(const Module &M) {
    for (const GlobalVariable &GV : M.globals())
      update(GV);
    for (const Function &F : M)
      update(F);
  }

  uint64_t getHash() const { return Hash; }
  std::unique_ptr<IndexInstructionMapType> getIndexInstructionMap() {
    return std::move(IndexInstructionMap);
  }
  std::unique_ptr<IndexPairOperandHashMapType> getIndexPairOperandHashMap() {
    return std::move(IndexPairOperandHashMap);
  }
};

} // namespace

IRHash llvm::StructuralHash(const Function &F, bool DetailedHash) {
  StructuralHashImpl H(DetailedHash);
  H.update(F);
  return H.getHash();
}

IRHash llvm::StructuralHash(const Module &M, bool DetailedHash) {
  StructuralHashImpl H(DetailedHash);
  H.update(M);
  return H.getHash();
}

FunctionHashInfo
llvm::StructuralHashWithDifferences(const Function &F,
                                    IgnoreOperandFunc IgnoreOp) {
  StructuralHashImpl H(/*DetailedHash=*/true, IgnoreOp);
  H.update(F);
  return FunctionHashInfo(H.getHash(), H.getIndexInstructionMap(),
                          H.getIndexPairOperandHashMap());
}
