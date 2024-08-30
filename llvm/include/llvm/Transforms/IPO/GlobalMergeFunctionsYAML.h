//===------ GlobalMergeFunctions.h - Global merge functions -----*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines classes for handling the YAML representation of Global
// Merge Functions Info.
//
//===----------------------------------------------------------------------===//

#ifndef PIKA_TRANSFORMS_UTILS_GLOBALMERGEFUNCTIONS_YAML_H
#define PIKA_TRANSFORMS_UTILS_GLOBALMERGEFUNCTIONS_YAML_H

#include "llvm/Support/YAMLTraits.h"
#include "llvm/Transforms/IPO/GlobalMergeFunctions.h"

LLVM_YAML_IS_SEQUENCE_VECTOR(StableFunction)

namespace llvm {
namespace yaml {

template <> struct CustomMappingTraits<InstOpndIdConstHashMapTy> {
  static void inputOne(IO &io, StringRef Key, InstOpndIdConstHashMapTy &V) {
    uint64_t ConstHash;
    io.mapRequired(Key.str().c_str(), ConstHash);
    int InstIdx, OpndIdx;
    auto KeyParts = Key.split(',');
    if (KeyParts.first.getAsInteger(0, InstIdx)) {
      io.setError("InstIdx not an integer");
      return;
    }
    if (KeyParts.second.getAsInteger(0, OpndIdx)) {
      io.setError("OpndIdx not an integer");
      return;
    }
    V.insert({{InstIdx, OpndIdx}, ConstHash});
  }
  static void output(IO &io, InstOpndIdConstHashMapTy &V) {
    for (auto Iter = V.begin(); Iter != V.end(); ++Iter) {
      auto KeyPairs =
          utostr(Iter->first.first) + ',' + utostr(Iter->first.second);
      io.mapRequired(KeyPairs.c_str(), Iter->second);
    }
  }
};

template <> struct MappingTraits<StableFunction> {
  static void mapping(IO &io, StableFunction &res) {
    io.mapRequired("StableHash", res.StableHash);
    io.mapRequired("Name", res.Name);
    io.mapRequired("ModuleIdentifier", res.ModuleIdentifier);
    io.mapRequired("CountInsts", res.CountInsts);
    io.mapRequired("InstOpndIndexToConstHash", res.InstOpndIndexToConstHash);
    io.mapRequired("IsMergeCandidate", res.IsMergeCandidate);
  }
};

template <> struct CustomMappingTraits<StableHashToStableFuncsTy> {
  static void inputOne(IO &io, StringRef Key, StableHashToStableFuncsTy &V) {
    SmallVector<StableFunction> SFI;
    io.mapRequired(Key.str().c_str(), SFI);
    uint64_t StableHash;
    if (Key.getAsInteger(0, StableHash)) {
      io.setError("StableHash not an integer");
      return;
    }
    V.insert({StableHash, SFI});
  }
  static void output(IO &io, StableHashToStableFuncsTy &V) {
    for (auto Iter = V.begin(); Iter != V.end(); ++Iter) {
      io.mapRequired(utostr(Iter->first).c_str(), Iter->second);
    }
  }
};

template <> struct MappingTraits<MergeFunctionInfo> {
  static void mapping(IO &io, MergeFunctionInfo &res) {
    io.mapRequired("StableHashToStableFuncs", res.StableHashToStableFuncs);
    io.mapRequired("IsMerged", res.IsMerged);
  }
};

} // namespace yaml
} // namespace llvm
#endif // PIKA_TRANSFORMS_UTILS_GLOBALMERGEFUNCTIONS_YAML_H
