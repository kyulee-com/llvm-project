//===---- GlobalMergeFunctions.cpp - Global merge functions -------*- C++ -===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This implements a function merge using function hash. Like the pika merge
// functions, this can merge functions that differ by Constant operands thru
// parameterizing them. However, instead of directly comparing IR functions,
// this uses stable function hash to find potential merge candidates.
// This provides a flexible framework to implement a global function merge with
// ThinLTO two-codegen rounds. The first codegen round collects stable function
// hashes, and determines the merge candidates that match the stable function
// hashes. The set of parameters pointing to different Constants are also
// computed during the stable function merge. The second codegen round uses this
// global function info to optimistically create a merged function in each
// module context to guarantee correct transformation. Similar to the global
// outliner, the linker's deduplication (ICF) folds the identical merged
// functions to save the final binary size.
//
//===----------------------------------------------------------------------===//

#include "llvm/Transforms/IPO/GlobalMergeFunctions.h"
#include "llvm/Transforms/IPO/GlobalMergeFunctionsIO.h"
#include "llvm/Transforms/IPO/GlobalMergeFunctionsYAML.h"

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallSet.h"
#include "llvm/ADT/StableHashing.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/ADT/Twine.h"
#include "llvm/CodeGen/MachineStableHash.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/IR/DiagnosticInfo.h"
#include "llvm/IR/DiagnosticPrinter.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/StructuralHash.h"
#include "llvm/InitializePasses.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/raw_ostream.h"
#include <functional>
#include <tuple>
#include <vector>

#define DEBUG_TYPE "global-merge-func"

using namespace llvm;
using namespace llvm::support;

cl::opt<bool> EnableGlobalMergeFunc(
    "enable-global-merge-func", cl::init(false), cl::Hidden,
    cl::desc("enable global merge functions (default = off)"));

cl::opt<unsigned> GlobalMergeExtraThreshold(
    "globalmergefunc-extra-threshold",
    cl::desc("An extra cost threshold for merging. '0' disables the extra cost "
             "and benefit analysis."),
    cl::init(0), cl::Hidden);

cl::opt<bool> DisableCrossModuleGlobalMergeFunc(
    "disable-cross-module-global-merge-func", cl::init(false), cl::Hidden,
    cl::desc("disable cross-module global merge functions. When this flag is "
             "true, only local functions are merged by global merge func."));

cl::opt<std::string> ReadHashFunctionFilename(
    "read-hash-function-filename", cl::init(""), cl::value_desc("filename"),
    cl::desc("Read the published stable hash function from this file."));

cl::opt<std::string> WriteHashFunctionFilename(
    "write-hash-function-filename", cl::init(""), cl::value_desc("filename"),
    cl::desc("Write the published stable hash function to this file."));

cl::opt<bool> EnableWriteHashFunction(
    "enable-write-hash-function", cl::init(false), cl::Hidden,
    cl::desc(
        "[Pika] Enable to write the published hash function. In our app "
        "build, we do not set `write-hash-function-filename`. Instead we use "
        "this boolean flag so that the filename is automatically set "
        "using 'getPikaPath()' (default = off)."));

cl::opt<bool> UseYamlHashFunction(
    "use-yaml-hash-function", cl::init(false), cl::Hidden,
    cl::desc(
        "Use yaml files to read or write stable functions. If this is "
        "disabled, we use the custom binary file which is more efficient."));

cl::opt<bool> SkipCompareIR(
    "global-merge-func-skip-compare-ir", cl::init(true), cl::Hidden,
    cl::desc(
        "This skips comparing IRs of local candidates while creating indiviual "
        "merged function per candidate. The actual merging will happen at link "
        "time, as with global candidates across modules (default = off)."));

cl::opt<bool>
    UseStructuralHash("use-structural-hash", cl::init(true), cl::Hidden,
                      cl::desc("Use structural hash to compute stable hash."));

STATISTIC(NumMismatchedFunctionHashGlobalMergeFunction,
          "Number of mismatched function hash for global merge function");
STATISTIC(NumMismatchedInstCountGlobalMergeFunction,
          "Number of mismatched instruction count for global merge function");
STATISTIC(NumMismatchedConstHashGlobalMergeFunction,
          "Number of mismatched const hash for global merge function");
STATISTIC(NumMismatchedIRGlobalMergeFunction,
          "Number of mismatched IR for global merge function");
STATISTIC(NumMismatchedModuleIdGlobalMergeFunction,
          "Number of mismatched Module Id for global merge function");
STATISTIC(
    NumMismatchedGlobalMergeFunctionCandidates,
    "Number of mismatched global merge function candidates that are skipped");
STATISTIC(NumGlobalMergeFunctionCandidates,
          "Number of global merge function candidates");
STATISTIC(NumCrossModuleGlobalMergeFunctionCandidates,
          "Number of cross-module global merge function candidates");
STATISTIC(NumIdenticalGlobalMergeFunctionCandidates,
          "Number of global merge function candidates that are identical (no "
          "parameter)");
STATISTIC(NumGlobalMergeFunctions,
          "Number of functions that are actually merged using function hash");
STATISTIC(
    NumCreatedMergedFunctions,
    "Number of functions that are additionally created using function hash");
STATISTIC(NumAnalyzedModues, "Number of modules that are analyzed");
STATISTIC(NumAnalyzedFunctions, "Number of functions that are analyzed");
STATISTIC(NumEligibleFunctions, "Number of functions that are eligible");

// A singleton context for diagnostic output.
static LLVMContext Ctx;
class GlobalMergeFuncDiagnosticInfo : public DiagnosticInfo {
  const Twine &Msg;

public:
  GlobalMergeFuncDiagnosticInfo(const Twine &DiagMsg,
                                DiagnosticSeverity Severity = DS_Error)
      : DiagnosticInfo(DK_Linker, Severity), Msg(DiagMsg) {}
  void print(DiagnosticPrinter &DP) const override { DP << Msg; }
};

// A singleton (global) merge function info. This collects global merge
// opportuinity.
MergeFunctionInfo *GlobalMergeFunctionInfo = nullptr;
// A singleton (published) merge function info. This provides global merge
// opportuinity.
MergeFunctionInfo *PublishedMergeFunctionInfo = nullptr;

static HashFunctionMode FunctionMode = HashFunctionMode::None;

namespace llvm {
bool isBuildingOrUsingHashFunction() {
  return isBuildingHashFunction() || isUsingHashFunction();
}

bool isBuildingHashFunction() {
  return FunctionMode == HashFunctionMode::BuildingHashFuncion;
}

bool isUsingHashFunction() {
  return FunctionMode == HashFunctionMode::UsingHashFunction;
}

void beginBuildingHashFunction() {
  if (!DisableCrossModuleGlobalMergeFunc) {
    Ctx.diagnose(GlobalMergeFuncDiagnosticInfo(
        "[GlobalMergeFunc] begin building hash function", DS_Note));
    assert(!GlobalMergeFunctionInfo);
    GlobalMergeFunctionInfo = new MergeFunctionInfo();
    FunctionMode = HashFunctionMode::BuildingHashFuncion;
  }
}

void endBuildingHashFunction() {
  Ctx.diagnose(GlobalMergeFuncDiagnosticInfo(
      "[GlobalMergeFunc] end building hash function", DS_Note));
  if (GlobalMergeFunctionInfo) {
    delete GlobalMergeFunctionInfo;
    GlobalMergeFunctionInfo = nullptr;
  }
  FunctionMode = HashFunctionMode::None;
}

void beginUsingHashFunction() {
  if (!DisableCrossModuleGlobalMergeFunc) {
    Ctx.diagnose(GlobalMergeFuncDiagnosticInfo(
        "[GlobalMergeFunc] begin using hash function", DS_Note));
    assert(PublishedMergeFunctionInfo);
    FunctionMode = HashFunctionMode::UsingHashFunction;
  }
}

void publishHashFunction() {
  if (DisableCrossModuleGlobalMergeFunc)
    return;
  Ctx.diagnose(GlobalMergeFuncDiagnosticInfo(
      "[GlobalMergeFunc] Publish stable hash functions", DS_Note));
  assert(GlobalMergeFunctionInfo);
  GlobalMergeFunctionInfo->mergeStableFunctions();

  PublishedMergeFunctionInfo = new MergeFunctionInfo();
  auto &StableHashToStableFuncs =
      GlobalMergeFunctionInfo->StableHashToStableFuncs;
  auto &StableHashParams = GlobalMergeFunctionInfo->StableHashParams;
  assert(StableHashToStableFuncs.size() == StableHashParams.size());
  assert(GlobalMergeFunctionInfo->IsMerged);
  PublishedMergeFunctionInfo->StableHashToStableFuncs =
      std::move(StableHashToStableFuncs);
  PublishedMergeFunctionInfo->StableHashParams = std::move(StableHashParams);
  PublishedMergeFunctionInfo->IsMerged = true;
}

void resetHashFunction() {
  if (GlobalMergeFunctionInfo) {
    delete GlobalMergeFunctionInfo;
    GlobalMergeFunctionInfo = nullptr;
  }
  if (PublishedMergeFunctionInfo) {
    delete PublishedMergeFunctionInfo;
    PublishedMergeFunctionInfo = nullptr;
  }
}

void endUsingHashFunction() {
  Ctx.diagnose(GlobalMergeFuncDiagnosticInfo(
      "[GlobalMergeFunc] end using hash function", DS_Note));
  FunctionMode = HashFunctionMode::None;
}

bool readHashFunction() {
  assert(!PublishedMergeFunctionInfo);
  PublishedMergeFunctionInfo = new MergeFunctionInfo();
  if (auto EC = PublishedMergeFunctionInfo->read(ReadHashFunctionFilename)) {
    Ctx.diagnose(GlobalMergeFuncDiagnosticInfo(
        "[GlobalMergeFunc] Fail to Read " + ReadHashFunctionFilename,
        DS_Warning));
    return false;
  }

  Ctx.diagnose(GlobalMergeFuncDiagnosticInfo(
      "[GlobalMergeFunc] Read " + ReadHashFunctionFilename, DS_Note));
  return true;
}

bool writeHashFunction() {
  if (!PublishedMergeFunctionInfo)
    PublishedMergeFunctionInfo = new MergeFunctionInfo();

  if (PublishedMergeFunctionInfo->empty()) {
    Ctx.diagnose(GlobalMergeFuncDiagnosticInfo(
        "[GlobalMergeFunc] Writing empty published merge function info",
        DS_Warning));
  }

  // In our app build with buck, we use getPikaPath to set a path.
  const std::string OutputFilename = WriteHashFunctionFilename;
  // getPikaPath(WriteHashFunctionFilename, "-mergefunctioninfo");
  if (auto EC = PublishedMergeFunctionInfo->write(OutputFilename)) {
    Ctx.diagnose(GlobalMergeFuncDiagnosticInfo(
        "[GlobalMergeFunc] Fail to Write " + OutputFilename, DS_Warning));
    return false;
  }

  Ctx.diagnose(GlobalMergeFuncDiagnosticInfo(
      "[GlobalMergeFunc] Wrote " + OutputFilename, DS_Note));
  return true;
}

} // end namespace llvm

#ifndef NDEBUG
void StableFunction::dump() {
  dbgs() << "StableFunc: " << Name << "\n";
  dbgs() << "  StableHash: " << StableHash << "\n";
  dbgs() << "  CountInsts: " << CountInsts << "\n";
  dbgs() << "  IsMergeCandidate: " << IsMergeCandidate << "\n";
  dbgs() << "  IndexToConstHashMap: \n";
  for (auto &[Index, Hash] : InstOpndIndexToConstHash) {
    auto [InstIndex, OpndIndex] = Index;
    dbgs() << "    (" << InstIndex << "," << OpndIndex << ") = " << Hash
           << "\n";
    if (IdxToInst.count(InstIndex)) {
      IdxToInst[InstIndex]->dump();
      IdxToInst[InstIndex]->getOperand(OpndIndex)->dump();
    }
  }
}
#endif

/// Returns true if the \opIdx operand of \p CI is the callee operand.
static bool isCalleeOperand(const CallBase *CI, unsigned opIdx) {
  return &CI->getCalledOperandUse() == &CI->getOperandUse(opIdx);
}

static bool canParameterizeCallOperand(const CallBase *CI, unsigned opIdx) {
  if (CI->isInlineAsm())
    return false;
  Function *Callee = CI->getCalledOperand()
                         ? dyn_cast_or_null<Function>(
                               CI->getCalledOperand()->stripPointerCasts())
                         : nullptr;
  if (Callee) {
    if (Callee->isIntrinsic())
      return false;
    // objc_msgSend stubs must be called, and can't have their address taken.
    if (Callee->getName().starts_with("objc_msgSend$"))
      return false;
  }
  if (isCalleeOperand(CI, opIdx) &&
      CI->getOperandBundle(LLVMContext::OB_ptrauth).has_value()) {
    // The operand is the callee and it has already been signed. Ignore this
    // because we cannot add another ptrauth bundle to the call instruction.
    return false;
  }
  return true;
}

bool isEligibleInstrunctionForConstantSharing(const Instruction *I) {
  switch (I->getOpcode()) {
  case Instruction::Load:
  case Instruction::Store:
  case Instruction::Call:
  case Instruction::Invoke:
    return true;
  default:
    return false;
  }
}

bool isEligibleOperandForConstantSharing(const Instruction *I, unsigned OpIdx) {
  assert(OpIdx < I->getNumOperands() && "Invalid operand index");

  if (!isEligibleInstrunctionForConstantSharing(I))
    return false;

  auto Opnd = I->getOperand(OpIdx);
  if (!isa<Constant>(Opnd))
    return false;

  if (const auto *CI = dyn_cast<CallBase>(I))
    return canParameterizeCallOperand(CI, OpIdx);

  return true;
}

/// Returns true if function \p F is eligible for merging.
bool isEligibleFunction(Function *F) {
  if (F->isDeclaration())
    return false;

  if (F->hasFnAttribute(llvm::Attribute::NoMerge))
    return false;

  if (F->hasAvailableExternallyLinkage()) {
    return false;
  }

  if (F->getFunctionType()->isVarArg()) {
    return false;
  }
#if 0
  // Check against blocklist.
  if (!MergeBlockRegexFilters.empty()) {
    StringRef FuncName = F->getName();
    for (const auto &tRegex : MergeBlockRegexFilters)
      if (Regex(tRegex).match(FuncName)) {
        return false;
      }
  }
  // Check against allowlist
  if (!MergeAllowRegexFilters.empty()) {
    StringRef FuncName = F->getName();
    bool found = false;
    for (const auto &tRegex : MergeAllowRegexFilters)
      if (Regex(tRegex).match(FuncName)) {
        found = true;
        break;
      }
    if (!found)
      return false;
  }
#endif
  if (F->getCallingConv() == CallingConv::SwiftTail)
    return false;

  // if function contains callsites with musttail, if we merge
  // it, the merged function will have the musttail callsite, but
  // the number of parameters can change, thus the parameter count
  // of the callsite will mismatch with the function itself.
  // if (IgnoreMusttailFunction) {
  for (const BasicBlock &BB : *F) {
    for (const Instruction &I : BB) {
      const auto *CB = dyn_cast<CallBase>(&I);
      if (CB && CB->isMustTailCall())
        return false;
    }
  }

  return true;
}

void MergeFunctionInfo::registerStableFunction(StableFunction &SF) {
  assert(!IsMerged);
  std::lock_guard<std::mutex> LogLock(MergeMutex);
  StableHashToStableFuncs[SF.StableHash].push_back(SF);
}

static std::map<SmallVector<ConstHash>, ParamLocs>
computeHashSeqToLocs(SmallVector<StableFunction> &SFS,
                     std::set<std::pair<int, int>> *KeysToDelete = nullptr) {
  std::map<SmallVector<ConstHash>, ParamLocs> HashSeqToLocs;
  auto &RSF = SFS[0];
  unsigned StableFunctionCount = SFS.size();

  for (auto &[IndexPair, Hash] : RSF.InstOpndIndexToConstHash) {
    // Const hash sequence across stable functions.
    // We will allocate a parameter per unique hash squence.
    SmallVector<ConstHash> ConstHashSeq;
    ConstHashSeq.push_back(Hash);
    bool Identical = true;
    for (unsigned J = 1; J < StableFunctionCount; ++J) {
      auto &SF = SFS[J];
      assert(SF.InstOpndIndexToConstHash.count(IndexPair));
      auto SHash = SF.InstOpndIndexToConstHash[IndexPair];
      if (Hash != SHash)
        Identical = false;
      ConstHashSeq.push_back(SHash);
    }

    // No need to parameterize them if Consts are identical across stable
    // functions.
    if (Identical)
      continue;
    if (KeysToDelete)
      KeysToDelete->erase(IndexPair);

    // For each unique Const hash sequence (parameter), add the locations.
    HashSeqToLocs[ConstHashSeq].push_back(IndexPair);
  }
  return HashSeqToLocs;
}

void MergeFunctionInfo::populateStableHashParams(
    stable_hash StableHash,
    const std::map<SmallVector<ConstHash>, ParamLocs> HashSeqToLocs) {
  // Populate ParamVec to StableHashParams in the source order.
  assert(!StableHashParams.count(StableHash));
  ParamLocsVecTy ParamLocsVec;
  for (auto &[HashSeq, Locs] : HashSeqToLocs)
    ParamLocsVec.push_back(std::move(Locs));
  std::sort(
      ParamLocsVec.begin(), ParamLocsVec.end(),
      [&](const ParamLocs &L, const ParamLocs &R) { return L[0] < R[0]; });
  StableHashParams.insert({StableHash, std::move(ParamLocsVec)});
}

void MergeFunctionInfo::finalizeStableHashParams() {
  for (auto &[StableHash, SFS] : StableHashToStableFuncs) {
    auto HashSeqToLocs = computeHashSeqToLocs(SFS);
    populateStableHashParams(StableHash, HashSeqToLocs);
  }
  assert(IsMerged);
}

void MergeFunctionInfo::mergeStableFunctions() {
  assert(!IsMerged && "Stable functions are aready merged.");

  IsMerged = true;
  for (auto It = StableHashToStableFuncs.begin();
       It != StableHashToStableFuncs.end();) {
    auto &[StableHash, SFS] = *It;
    // No interest if there is no common stable function globally.
    if (SFS.size() < 2) {
      StableHashToStableFuncs.erase(It++);
      continue;
    }

    // Group stable functions by ModuleIdentifier.
    std::stable_sort(SFS.begin(), SFS.end(),
                     [](const StableFunction &L, const StableFunction &R) {
                       return L.ModuleIdentifier < R.ModuleIdentifier;
                     });

    // Consider the first function as the root function.
    auto &RSF = SFS[0];

    // Initialize all index keys are to be deleted.
    std::set<std::pair<int, int>> KeysToDelete;
    for (auto &PC : RSF.InstOpndIndexToConstHash)
      KeysToDelete.insert(PC.first);

    LLVM_DEBUG({
      dbgs() << "[MergeFunctionInfo] Root stable func (hash:" << StableHash
             << ") : " << RSF.Name << "\n";
    });
    bool IsValid = true;
    bool HasCrossModuleCandidate = false;
    unsigned StableFunctionCount = SFS.size();
    for (unsigned I = 1; I < StableFunctionCount; ++I) {
      auto &SF = SFS[I];
      LLVM_DEBUG({
        dbgs() << "[MergeFunctionInfo] Trying to merge stable Func: " << SF.Name
               << "\n";
      });
      if (RSF.ModuleIdentifier != SF.ModuleIdentifier)
        HasCrossModuleCandidate = true;

      if (RSF.CountInsts != SF.CountInsts) {
        IsValid = false;
        break;
      }
      if (RSF.StableHash != SF.StableHash) {
        IsValid = false;
        break;
      }
      if (RSF.InstOpndIndexToConstHash.size() !=
          SF.InstOpndIndexToConstHash.size()) {
        IsValid = false;
        break;
      }
      for (auto &P : RSF.InstOpndIndexToConstHash) {
        auto &InstOpndIndex = P.first;
        if (!SF.InstOpndIndexToConstHash.count(InstOpndIndex)) {
          IsValid = false;
          break;
        }
      }
    }
    if (!IsValid) {
      LLVM_DEBUG({
        dbgs() << "[MergeFunctionInfo] Ignore mismatched stable functions.\n";
      });
      NumMismatchedGlobalMergeFunctionCandidates += StableFunctionCount;
      StableHashToStableFuncs.erase(It++);
      continue;
    }

    auto HashSeqToLocs = computeHashSeqToLocs(SFS, &KeysToDelete);
    LLVM_DEBUG({
      dbgs() << "[MergeFunctionInfo] Unique hash sequences (Parameters): "
             << HashSeqToLocs.size() << "\n";
    });

    // Compute extra benefit/cost for global merge func.
    if (GlobalMergeExtraThreshold) {
      unsigned Benefit = RSF.CountInsts * (StableFunctionCount - 1);
      unsigned Cost =
          (2 * HashSeqToLocs.size() + /*call*/ 1) * StableFunctionCount +
          GlobalMergeExtraThreshold;
      if (Benefit <= Cost) {
        LLVM_DEBUG({
          dbgs() << "[MergeFunctionInfo] (FuncSize, ParamSize, FuncCount) = ("
                 << RSF.CountInsts << ", " << HashSeqToLocs.size() << ", "
                 << StableFunctionCount << ")\n";
          dbgs() << "[MergeFunctionInfo] Skip since Benefit " << Benefit
                 << " <= " << Cost << "\n";
        });
        StableHashToStableFuncs.erase(It++);
        continue;
      }
    }

    // Now we have merging candidates that can save the size.
    ++It;
    if (HasCrossModuleCandidate)
      NumCrossModuleGlobalMergeFunctionCandidates += StableFunctionCount;
    NumGlobalMergeFunctionCandidates += StableFunctionCount;
    if (HashSeqToLocs.empty())
      NumIdenticalGlobalMergeFunctionCandidates += StableFunctionCount;

    // Minimize InstOpndIndexToConstHash by removing locations pointing to the
    // same Const.
    for (auto &SF : SFS) {
      for (auto &Key : KeysToDelete)
        SF.InstOpndIndexToConstHash.erase(Key);
      SF.IsMergeCandidate = true;
    }

    // Populate ParamVec to StableHashParams in the source order.
    assert(!StableHashParams.count(StableHash));
    populateStableHashParams(StableHash, HashSeqToLocs);
  }
}

llvm::Error MergeFunctionInfo::readFromYAMLFile(StringRef Filename) {
  assert(!IsMerged &&
         "don't read merge function info that has been already merged!");
  auto BufferOrErr = MemoryBuffer::getFile(Filename);
  if (auto EC = BufferOrErr.getError())
    return llvm::createStringError(EC, "Unable to read %s",
                                   Filename.str().c_str());
  auto &Buffer = BufferOrErr.get();
  yaml::Input yamlOS(Buffer->getMemBufferRef());
  yamlOS >> *this;

  // Now populate parameters from StableHashToStableFuncs.
  finalizeStableHashParams();

  return llvm::Error::success();
}

llvm::Error MergeFunctionInfo::writeToYAMLFile(StringRef Filename) {
  std::error_code EC;
  raw_fd_ostream OS(Filename, EC, sys::fs::OpenFlags::OF_Text);
  if (EC)
    return llvm::createStringError(EC, "Unable to write %s",
                                   Filename.str().c_str());
  yaml::Output yamlOS(OS);
  yamlOS << *this;
  return llvm::Error::success();
}

llvm::Error MergeFunctionInfo::readFromBinaryFile(StringRef Filename) {
  assert(!IsMerged &&
         "don't read merge function info that has been already merged!");
  auto BufferOrErr = MemoryBuffer::getFile(Filename);
  if (auto EC = BufferOrErr.getError())
    return llvm::createStringError(EC, "Unable to read %s",
                                   Filename.str().c_str());
  auto &Buffer = BufferOrErr.get();

  // Check the Magic first for early exit when invalid.
  const char *Data = Buffer->getBufferStart();
  if (Buffer->getBufferSize() <= 4 ||
      endian::readNext<uint32_t, endianness::little, unaligned>(Data) !=
          GlobalMergeFunctionsIO::GMF_MAGIC) {
    return llvm::createStringError(std::errc::invalid_argument,
                                   "Invalid the header for %s",
                                   Filename.str().c_str());
  }
  GlobalMergeFunctionsIO::read(*Buffer, *this);

  // Now populate parameters from StableHashToStableFuncs.
  finalizeStableHashParams();

  return llvm::Error::success();
}

llvm::Error MergeFunctionInfo::writeToBinaryFile(StringRef Filename) {
  std::error_code EC;
  raw_fd_ostream OS(Filename, EC, sys::fs::OpenFlags::OF_None);
  if (EC)
    return llvm::createStringError(EC, "Unable to write %s",
                                   Filename.str().c_str());
  GlobalMergeFunctionsIO::write(OS, *this);
  return llvm::Error::success();
}

llvm::Error MergeFunctionInfo::read(StringRef Filename) {
  return UseYamlHashFunction ? readFromYAMLFile(Filename)
                             : readFromBinaryFile(Filename);
}

llvm::Error MergeFunctionInfo::write(StringRef Filename) {
  return UseYamlHashFunction ? writeToYAMLFile(Filename)
                             : writeToBinaryFile(Filename);
}

static bool
isEligibleInstrunctionForConstantSharingLocal(const Instruction *I) {
  switch (I->getOpcode()) {
  case Instruction::Load:
  case Instruction::Store:
  case Instruction::Call:
  case Instruction::Invoke:
    return true;
  default:
    return false;
  }
}

static bool ignoreOp(const Instruction *I, unsigned OpIdx) {
  assert(OpIdx < I->getNumOperands() && "Invalid operand index");

  if (!isEligibleInstrunctionForConstantSharingLocal(I))
    return false;

  if (!isa<Constant>(I->getOperand(OpIdx)))
    return false;

  if (const auto *CI = dyn_cast<CallBase>(I))
    return canParameterizeCallOperand(CI, OpIdx);

  return true;
}

// copy from merge functions.cpp
static Value *createCast(IRBuilder<> &Builder, Value *V, Type *DestTy) {
  Type *SrcTy = V->getType();
  if (SrcTy->isStructTy()) {
    assert(DestTy->isStructTy());
    assert(SrcTy->getStructNumElements() == DestTy->getStructNumElements());
    Value *Result = PoisonValue::get(DestTy);
    for (unsigned int I = 0, E = SrcTy->getStructNumElements(); I < E; ++I) {
      Value *Element =
          createCast(Builder, Builder.CreateExtractValue(V, ArrayRef(I)),
                     DestTy->getStructElementType(I));

      Result = Builder.CreateInsertValue(Result, Element, ArrayRef(I));
    }
    return Result;
  }
  assert(!DestTy->isStructTy());
  if (auto *SrcAT = dyn_cast<ArrayType>(SrcTy)) {
    auto *DestAT = dyn_cast<ArrayType>(DestTy);
    assert(DestAT);
    assert(SrcAT->getNumElements() == DestAT->getNumElements());
    Value *Result = UndefValue::get(DestTy);
    for (unsigned int I = 0, E = SrcAT->getNumElements(); I < E; ++I) {
      Value *Element =
          createCast(Builder, Builder.CreateExtractValue(V, ArrayRef(I)),
                     DestAT->getElementType());

      Result = Builder.CreateInsertValue(Result, Element, ArrayRef(I));
    }
    return Result;
  }
  assert(!DestTy->isArrayTy());
  if (SrcTy->isIntegerTy() && DestTy->isPointerTy())
    return Builder.CreateIntToPtr(V, DestTy);
  else if (SrcTy->isPointerTy() && DestTy->isIntegerTy())
    return Builder.CreatePtrToInt(V, DestTy);
  else
    return Builder.CreateBitCast(V, DestTy);
}

void GlobalMergeFunc::analyze(MergeFunctionInfo &MFI, Module &M) {
  ++NumAnalyzedModues;
  for (Function &Func : M) {
    ++NumAnalyzedFunctions;
    if (isEligibleFunction(&Func)) {
      ++NumEligibleFunctions;
      StableFunction SF(get_stable_name(Func.getName()));
      MapVector<unsigned, Instruction *> IdxToInst;
      if (UseStructuralHash) {
        auto FI = llvm::StructuralHashWithDifferences(Func, ignoreOp);

        SF.StableHash = FI.FunctionHash;
        IdxToInst = std::move(*FI.IndexInstruction);
        SF.InstOpndIndexToConstHash = std::move(*FI.IndexPairOpndHash);
      } else {
        errs() << "Not supported pikafunctino hash \n";
      }
      SF.ModuleIdentifier = M.getModuleIdentifier();
      SF.CountInsts = IdxToInst.size();
#ifndef NDEBUG
      SF.IdxToInst = std::move(IdxToInst);
#endif
      MFI.registerStableFunction(SF);
    }
  }
}

/// Tuple to hold function info to process merging.
struct FuncInfo {
  StableFunction SF;
  Function *F;
  MapVector<unsigned, Instruction *> IdxToInst;
};

// Given the root func info, and the parameterized locations, create and return
// a new merged function.
static Function *createMergedFunction(FuncInfo &RootFI,
                                      ParamLocsVecTy &ParamLocsVec) {
  // Synthesize a new merged function name by appending ".Tgm" to the root
  // function's name.
  auto *RootFunc = RootFI.F;
  auto NewFunctionName = RootFunc->getName().str() + MergeFunctionInfo::Suffix;
  auto *M = RootFunc->getParent();
  assert(!M->getFunction(NewFunctionName));

  FunctionType *OrigTy = RootFunc->getFunctionType();
  // Get the original params' types.
  SmallVector<Type *> ParamTypes(OrigTy->param_begin(), OrigTy->param_end());
  // Append extra params' types derived from the first (any) Constant.
  for (auto &ParamLocs : ParamLocsVec) {
    assert(!ParamLocs.empty());
    auto [InstIndex, OpndIndex] = ParamLocs[0];
    auto *Inst = RootFI.IdxToInst[InstIndex];
    auto *Const = (Constant *)Inst->getOperand(OpndIndex);
    ParamTypes.push_back(Const->getType());
  }
  FunctionType *FuncType =
      FunctionType::get(OrigTy->getReturnType(), ParamTypes, false);

  // Declare a new function
  Function *NewFunction =
      Function::Create(FuncType, RootFunc->getLinkage(), NewFunctionName);
  if (auto *SP = RootFunc->getSubprogram())
    NewFunction->setSubprogram(SP);
  NewFunction->copyAttributesFrom(RootFunc);
  NewFunction->setDLLStorageClass(GlobalValue::DefaultStorageClass);
#if 0
  if (UseLinkOnceODRLinkageMerging)
    NewFunction->setLinkage(GlobalValue::LinkOnceODRLinkage);
  else
#endif
  NewFunction->setLinkage(GlobalValue::InternalLinkage);
  // if (NoInlineForMergedFunction)
  NewFunction->addFnAttr(Attribute::NoInline);
  // #endif

  // Add the new function before the root function.
  M->getFunctionList().insert(RootFunc->getIterator(), NewFunction);

  // Move the body of RootFunc into the NewFunction.
  NewFunction->splice(NewFunction->begin(), RootFunc);

  // Update the original args by the new args.
  auto NewArgIter = NewFunction->arg_begin();
  for (Argument &OrigArg : RootFunc->args()) {
    Argument &NewArg = *NewArgIter++;
    OrigArg.replaceAllUsesWith(&NewArg);
  }

  // Replace the original Constants by the new args.
  unsigned NumOrigArgs = RootFunc->arg_size();
  for (unsigned ParamIdx = 0; ParamIdx < ParamLocsVec.size(); ++ParamIdx) {
    Argument *NewArg = NewFunction->getArg(NumOrigArgs + ParamIdx);
    for (auto [InstIndex, OpndIndex] : ParamLocsVec[ParamIdx]) {
      auto *Inst = RootFI.IdxToInst[InstIndex];
      auto *OrigC = Inst->getOperand(OpndIndex);
      if (OrigC->getType() != NewArg->getType()) {
        IRBuilder<> Builder(Inst->getParent(), Inst->getIterator());
        Inst->setOperand(OpndIndex,
                         createCast(Builder, NewArg, OrigC->getType()));
      } else
        Inst->setOperand(OpndIndex, NewArg);
    }
  }

  return NewFunction;
}

// Given the original function (Thunk) and the merged function (ToFunc), create
// a thunk to the merged function.
static void createThunk(Function *Thunk, Function *ToFunc,
                        SmallVector<Constant *> &Params) {
// TODO
#if 0
  unsigned ArgSize = Thunk->arg_size();
  auto ParamSize = Params.size();
  assert(ArgSize + ParamSize == ToFunc->getFunctionType()->getNumParams());
#endif
  Thunk->dropAllReferences();

  BasicBlock *BB = BasicBlock::Create(Thunk->getContext(), "", Thunk);
  IRBuilder<> Builder(BB);

  SmallVector<Value *> Args;
  unsigned ParamIdx = 0;
  FunctionType *ToFuncTy = ToFunc->getFunctionType();

  // Add arguments which are passed through Thunk.
  for (Argument &AI : Thunk->args()) {
    Args.push_back(createCast(Builder, &AI, ToFuncTy->getParamType(ParamIdx)));
    ++ParamIdx;
  }

  // Add new arguments defined by Params.
  for (auto *Param : Params) {
    assert(ParamIdx < ToFuncTy->getNumParams());
    // FIXME: do not support signing
    Args.push_back(
        createCast(Builder, Param, ToFuncTy->getParamType(ParamIdx)));
    ++ParamIdx;
  }

  CallInst *CI = Builder.CreateCall(ToFunc, Args);
  bool isSwiftTailCall = ToFunc->getCallingConv() == CallingConv::SwiftTail &&
                         Thunk->getCallingConv() == CallingConv::SwiftTail;
  CI->setTailCallKind(isSwiftTailCall ? llvm::CallInst::TCK_MustTail
                                      : llvm::CallInst::TCK_Tail);
  CI->setCallingConv(ToFunc->getCallingConv());
  CI->setAttributes(ToFunc->getAttributes());
  if (Thunk->getReturnType()->isVoidTy()) {
    Builder.CreateRetVoid();
  } else {
    Builder.CreateRet(createCast(Builder, CI, Thunk->getReturnType()));
  }
}

// Check if the old merged/optimized InstOpndIndexToConstHash is compatible with
// the current InstOpndIndexToConstHash. ConstHash may not be stable across
// different builds due to varying modules combined. To address this, one
// solution could be to relax the hash computation for Const in
// PikaFunctionHash. However, instead of doing so, we relax the hash check
// condition by comparing Const hash patterns instead of absolute hash values.
// For example, let's assume we have three Consts located at idx1, idx3, and
// idx6, where their corresponding hashes are hash1, hash2, and hash1 in the old
// merged map below:
//   Old (Merged): [(idx1, hash1), (idx3, hash2), (idx6, hash1)]
//   Current: [(idx1, hash1'), (idx3, hash2'), (idx6, hash1')]
// If the current function also has three Consts in the same locations,
// with hash sequences hash1', hash2', and hash1' where the first and third
// are the same as the old hash sequences, we consider them matched.
static bool checkConstHashCompatible(
    const DenseMap<LocPair, ConstHash> &OldInstOpndIndexToConstHash,
    const DenseMap<LocPair, ConstHash> &CurrInstOpndIndexToConstHash) {

  DenseMap<ConstHash, ConstHash> OldHashToCurrHash;
  for (const auto &[Index, OldHash] : OldInstOpndIndexToConstHash) {
    auto It = CurrInstOpndIndexToConstHash.find(Index);
    if (It == CurrInstOpndIndexToConstHash.end())
      return false;

    auto CurrHash = It->second;
    auto J = OldHashToCurrHash.find(OldHash);
    if (J == OldHashToCurrHash.end())
      OldHashToCurrHash.insert({OldHash, CurrHash});
    else if (J->second != CurrHash)
      return false;
  }

  return true;
}

bool GlobalMergeFunc::merge(MergeFunctionInfo &MFI, Module &M) {
  assert(MFI.IsMerged && "Stable functions should be merged!");
  bool Changed = false;

  // Build a map from stable function name to function.
  StringMap<Function *> StableNameToFuncMap;
  for (auto &F : M)
    StableNameToFuncMap[get_stable_name(F.getName())] = &F;
  // Track merged functions
  DenseSet<Function *> MergedFunctions;

  auto ModId = M.getModuleIdentifier();
  for (auto &[Hash, SFS] : MFI.StableHashToStableFuncs) {
    assert(SFS.size() >= 2);
    assert(MFI.StableHashParams.count(Hash));
    auto &ParamLocsVec = MFI.StableHashParams[Hash];
    LLVM_DEBUG({
      dbgs() << "[GlobalMergeFunc] Merging hash: " << Hash << " with Params "
             << ParamLocsVec.size() << "\n";
    });

    // Validate if the function candidates are also mergeable by pika-merge-func
    // comparator. Collect necessary function infos in the context of the
    // current module, M.
    Function *RootFunc = nullptr;
    std::string RootModId;
    SmallVector<FuncInfo> FuncInfos;
    for (auto &SF : SFS) {
      auto FI = StableNameToFuncMap.find(SF.Name);
      if (FI == StableNameToFuncMap.end())
        continue;
      Function *F = FI->second;
      assert(F);
      if (MergedFunctions.count(F))
        continue;

      if (!isEligibleFunction(F))
        continue;
      MapVector<unsigned, Instruction *> IdxToInst;
      DenseMap<LocPair, ConstHash> InstOpndIndexToConstHash;
      if (UseStructuralHash) {
        auto FI = llvm::StructuralHashWithDifferences(*F, ignoreOp);
        uint64_t FuncHash = FI.FunctionHash;
        if (Hash != FuncHash) {
          ++NumMismatchedFunctionHashGlobalMergeFunction;
          continue;
        }
        IdxToInst = std::move(*FI.IndexInstruction);
        InstOpndIndexToConstHash = std::move(*FI.IndexPairOpndHash);
      } else {
        errs() << "Error: UseStructuralHash is not supported!\n";
      }

      if ((unsigned)SF.CountInsts != IdxToInst.size()) {
        ++NumMismatchedInstCountGlobalMergeFunction;
        continue;
      }

      bool HasValidSharedConst = true;
      for (auto &[Index, Hash] : SF.InstOpndIndexToConstHash) {
        auto [InstIndex, OpndIndex] = Index;
        assert((unsigned long)InstIndex < IdxToInst.size());
        auto *Inst = IdxToInst[InstIndex];
        if (!isEligibleOperandForConstantSharing(Inst, OpndIndex)) {
          HasValidSharedConst = false;
          break;
        }
      }
      if (!HasValidSharedConst) {
        ++NumMismatchedConstHashGlobalMergeFunction;
        continue;
      }

      if (!checkConstHashCompatible(SF.InstOpndIndexToConstHash,
                                    InstOpndIndexToConstHash)) {
        ++NumMismatchedConstHashGlobalMergeFunction;
        continue;
      }

      if (RootFunc) {
        // Check if the matched functions fall into the same (first) module.
        if (RootModId != SF.ModuleIdentifier) {
          ++NumMismatchedModuleIdGlobalMergeFunction;
          continue;
        }

        // We do not need the following IR check if we always create a merged
        // function per each instance assuming the actual merging logic solely
        // depends on the linker's ICF.
        // With this assumption, we can omit porting the following IR compartor
        // while ignoring constants, which might be fragile, at the cost of
        // merging efficiency.
        // This will also improve the debugging experience as all the merging
        // instance (w/ suffix .Tgm) will appear in the linker map.
        if (!SkipCompareIR) {
          // TODO: deprecate
        }
      } else {
        RootFunc = F;
        RootModId = SF.ModuleIdentifier;
      }

      FuncInfos.push_back({SF, F, std::move(IdxToInst)});
      MergedFunctions.insert(F);
    }
    unsigned FuncInfoSize = FuncInfos.size();
    if (FuncInfoSize == 0)
      continue;

    LLVM_DEBUG({
      dbgs() << "[GlobalMergeFunc] Merging function count " << FuncInfoSize
             << " in  " << ModId << "\n";
    });
    Function *MergedFunc = nullptr;
    for (auto &FI : FuncInfos) {
      Changed = true;

      // auto *F = FI.F;
      if (!SkipCompareIR) {
        // TODO: deprecate
      }

      // Validate the locations pointed by param has the same hash and Constant.
      // Derive parameters that hold the actual Constants.
      SmallVector<Constant *> Params;
      for (auto &ParamLocs : ParamLocsVec) {
        std::optional<ConstHash> OldHash;
        std::optional<Constant *> OldConst;
        for (auto &Loc : ParamLocs) {
          assert(FI.SF.InstOpndIndexToConstHash.count(Loc));
          auto CurrHash = FI.SF.InstOpndIndexToConstHash[Loc];
          auto [InstIndex, OpndIndex] = Loc;
          assert((unsigned)InstIndex < FI.IdxToInst.size());
          auto *Inst = FI.IdxToInst[InstIndex];
          auto *CurrConst = cast<Constant>(Inst->getOperand(OpndIndex));
          if (!OldHash) {
            OldHash = CurrHash;
            OldConst = CurrConst;
          }

          assert(CurrHash == *OldHash);
          // assert(!FCmp.compareConstants(CurrConst, *OldConst));
        }

        assert(*OldConst);
        Params.push_back(*OldConst);
      }

      // Create a merged function derived from the first function in the current
      // module context.
      if (SkipCompareIR || !MergedFunc) {
        MergedFunc = createMergedFunction(FI, ParamLocsVec);
        assert(MergedFunc);
        LLVM_DEBUG({
          dbgs() << "[GlobalMergeFunc] Merged function (hash:"
                 << FI.SF.StableHash << ") " << MergedFunc->getName()
                 << " generated from " << FI.F->getName() << ":\n";
          MergedFunc->dump();
        });
        ++NumCreatedMergedFunctions;
      }

      // Create a thunk to the merged function.
      createThunk(FI.F, MergedFunc, Params);
      LLVM_DEBUG({
        dbgs() << "[GlobalMergeFunc] Thunk generated: \n";
        FI.F->dump();
      });
      ++NumGlobalMergeFunctions;
    }
  }

  return Changed;
}

char GlobalMergeFunc::ID = 0;
INITIALIZE_PASS_BEGIN(GlobalMergeFunc, "global-merge-func",
                      "Global merge function pass", false, false)
INITIALIZE_PASS_END(GlobalMergeFunc, "global-merge-func",
                    "Global merge function pass", false, false)

GlobalMergeFunc::GlobalMergeFunc() : ModulePass(ID) {
  initializeGlobalMergeFuncPass(*llvm::PassRegistry::getPassRegistry());
}

namespace llvm {
Pass *createGlobalMergeFuncPass() { return new GlobalMergeFunc(); }
} // namespace llvm

bool GlobalMergeFunc::runOnModule(Module &M) {
  bool Changed = false;

  if (!isBuildingOrUsingHashFunction()) {
    // No two-codegen rounds case (no actual global merge)
    MergeFunctionInfo MFI;
    analyze(MFI, M);
    MFI.mergeStableFunctions();
    assert(MFI.IsMerged);
    Changed = merge(MFI, M);
  } else if (isBuildingHashFunction()) {
    analyze(*GlobalMergeFunctionInfo, M);
  } else if (isUsingHashFunction()) {
    Changed = merge(*PublishedMergeFunctionInfo, M);
  }

  return Changed;
}
