//===- OutlinedHashTreeRecord.h ---------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
/// \file
///
/// TODO
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_CODEGENDATA_OUTLINEDHASHTREERECORD_H
#define LLVM_CODEGENDATA_OUTLINEDHASHTREERECORD_H

#include "llvm/CodeGenData/OutlinedHashTree.h"

namespace llvm {

using IdHashNodeStableMapTy = std::map<unsigned, HashNodeStable>;
using IdHashNodeMapTy = std::map<unsigned, HashNode *>;
using HashNodeIdMapTy = std::unordered_map<const HashNode *, unsigned>;

struct OutlinedHashTreeRecord {
  /// The outlined hash tree being held for serialization and deserialization.
  std::unique_ptr<OutlinedHashTree> HashTree;

  OutlinedHashTreeRecord() { HashTree = std::make_unique<OutlinedHashTree>(); }
  OutlinedHashTreeRecord(std::unique_ptr<OutlinedHashTree> HashTree)
      : HashTree(std::move(HashTree)){};

  void serialize(raw_ostream &OS) const;
  void deserialize(const unsigned char *&Ptr);
  void serializeYAML(yaml::Output &YOS) const;
  void deserializeYAML(yaml::Input &YOS);

  /// Merge the other outlined hash tree into this one.
  void merge(const OutlinedHashTreeRecord &Other) {
    HashTree->merge(Other.HashTree.get());
  }

  bool empty() { return HashTree->empty(); }

private:
  /// Convert HashTree to stable data.
  void convertToStableData(IdHashNodeStableMapTy &IdNodeStableMap) const;

  /// Convert stable data to HashTree.
  void convertFromStableData(const IdHashNodeStableMapTy &IdNodeStableMap);
};

} // end namespace llvm

#endif // LLVM_CODEGENDATA_OUTLINEDHASHTREERECORD_H
