//===------ GlobalMergeFunctions.h - Global merge functions -----*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// This file defines global merge functions pass and related data structure.
///
//===----------------------------------------------------------------------===//

#ifndef PIKA_TRANSFORMS_UTILS_GLOBALMERGEFUNCTIONS_H
#define PIKA_TRANSFORMS_UTILS_GLOBALMERGEFUNCTIONS_H

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/StableHashing.h"
#include "llvm/ADT/StringRef.h"

#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include <map>
#include <mutex>

enum class HashFunctionMode {
  None,
  BuildingHashFuncion,
  UsingHashFunction,
};

namespace llvm {

/// Return true when either using the published hash function, or building the
/// global hash function.
bool isBuildingOrUsingHashFunction();

/// Return true when building the global hash function.
bool isBuildingHashFunction();

/// Return true when using the published hash function.
bool isUsingHashFunction();

/// Set a mode when building the global hash function.
void beginBuildingHashFunction();

/// End the mode building the global hash function.
void endBuildingHashFunction();

/// Set a mode when using the published hash function.
void beginUsingHashFunction();

/// End the mode using the publihsed hash function.
void endUsingHashFunction();

/// Merge and publish the global hash function.
void publishHashFunction();

/// Clean up the global or published hash function;
void resetHashFunction();

/// Read the published hash function.
bool readHashFunction();

/// Write the published hash function.
bool writeHashFunction();

// (inst, opnd) indices
using LocPair = std::pair<unsigned, unsigned>;
// 64 bit constant hash
using ConstHash = uint64_t;
// A map of location pair to constant hash
using InstOpndIdConstHashMapTy = DenseMap<LocPair, ConstHash>;

struct StableFunction {
  StableFunction() {}
  StableFunction(StringRef FuncName) : Name(FuncName) {}
  /// Stable hash ignoring Const for eligible operations
  uint64_t StableHash = 0;

  /// Function name
  std::string Name;

  /// Module identifier
  std::string ModuleIdentifier;

  /// Count of original instructions
  int CountInsts = 0;

  /// Map of (inst, opnd) indices to the Const hash in the eligible
  /// operations (in order). The keys of the map should be matched with other
  /// stable functions that have the same stable hash.
  InstOpndIdConstHashMapTy InstOpndIndexToConstHash;

  /// This is true when InstOpndIndexToConstHash is finalized (minimized) by
  /// being compared with other stable functions whose stable hashes are
  /// matched.
  bool IsMergeCandidate = false;

#ifndef NDEBUG
  /// Map of index to instruction.
  // std::map<int, Instruction *> IdxToInst;
  MapVector<unsigned, Instruction *> IdxToInst;

  /// Dump stable function contents.
  void dump();
#endif
};

// A vector of locations (the pair of (instruction, operand) indices) reachable
// from a parameter.
using ParamLocs = SmallVector<LocPair, 4>;
// A vector of parameters
using ParamLocsVecTy = SmallVector<ParamLocs, 8>;
// A map of stable hash to a vector of stable functions
using StableHashToStableFuncsTy =
    DenseMap<stable_hash, SmallVector<StableFunction>>;
// A map of stable hash to a vector of parameters that point to the reachable
// locations.
using StableHashParamsTy = DenseMap<stable_hash, ParamLocsVecTy>;

struct MergeFunctionInfo {
  /// The suffix is used to create a root function name by appending it
  /// to the original function name.
  static constexpr const char *Suffix = ".Tgm";

  /// A map from stable function hash to stable functions.
  StableHashToStableFuncsTy StableHashToStableFuncs;

  /// A map from stable function hash to parameters pointing to the pair of
  /// (instruction, operand) indices.
  StableHashParamsTy StableHashParams;

  /// mutex when updating the global merge function info.
  std::mutex MergeMutex;

  /// This tells whether StableHashToStableFuncs and StableHashParams are
  /// finalized.
  bool IsMerged = false;

  /// New stable funciton is registered to the global merge function info.
  void registerStableFunction(StableFunction &SF);

  /// Merge stable functions and determine StableHashParams.
  /// This should run sequentially when all stable functions were already
  /// resgistered.
  void mergeStableFunctions();

  /// Deserializes StableHashToStableFuncs from a yaml file at \p Filename.
  /// Derive StableHashParams from StableHashToStableFuncs.
  llvm::Error readFromYAMLFile(StringRef Filename);

  /// Serializes StableHashToStableFuncs to a yaml file at \p Filename.
  /// We do not serialize StableHashParams which can be driven from
  /// StableHashToStableFuncs.
  llvm::Error writeToYAMLFile(StringRef Filename);

  /// Deserializes StableHashToStableFuncs from a binary file at \p Filename.
  /// Derive StableHashParams from StableHashToStableFuncs.
  llvm::Error readFromBinaryFile(StringRef Filename);

  /// Serializes StableHashToStableFuncs to a binary file at \p Filename.
  /// We do not serialize StableHashParams which can be driven from
  /// StableHashToStableFuncs.
  llvm::Error writeToBinaryFile(StringRef Filename);

  /// Serializes StableHashToStableFuncs at \p Filename.
  /// This is a wrapper to dispatch either readFromYAMLFile or
  /// readFromBinaryFile.
  llvm::Error read(StringRef Filename);

  /// Deserializes StableHashToStableFuncs at \p Filename.
  /// This is a wrapper to dispatch either writeToYAMLFile or writeToBinaryFile.
  llvm::Error write(StringRef Filename);

  /// Return true if no stable functions are registered yet.
  bool empty() { return StableHashToStableFuncs.empty(); }

  /// Populate StableHashParams entry for the given StableHash and
  /// HashSeqToLocs.
  void populateStableHashParams(
      stable_hash StableHash,
      const std::map<SmallVector<ConstHash>, ParamLocs> HashSeqToLocs);

  /// After stable function maps are finalized, we update the StableHashParams.
  void finalizeStableHashParams();
};

/// GlobalMergeFunc finds functions which only differ by constants in
/// certain instructions, e.g. resulting from specialized functions of layout
/// compatible types.
/// Unlike PikaMergeFunc that directly compares IRs, this uses stable function
/// hash to find the merge candidate. Similar to the global outliner, we can run
/// codegen twice to collect function merge candidate in the first round, and
/// merge functions globally in the second round.
class GlobalMergeFunc : public ModulePass {
public:
  static char ID;
  GlobalMergeFunc();

  bool runOnModule(Module &M) override;

  /// Analyze module to find merge function candidates into MFI.
  void analyze(MergeFunctionInfo &MFI, Module &M);

  /// Merge functions in the module using MFI.
  bool merge(MergeFunctionInfo &MFI, Module &M);
};

} // end namespace llvm
#endif // PIKA_TRANSFORMS_UTILS_GLOBALMERGEFUNCTIONS_H
