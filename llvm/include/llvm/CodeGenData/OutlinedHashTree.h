//===-- OutlinedHashTree.h --------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Contains a stable hash tree implementation based on llvm::stable_hash.
/// An OutlinedHashTree is a Trie that contains sequences of hash values of
/// instructions that have been outlined in a module. The OutlinedHashTree can
/// be used to understand the outlined instruction sequences collected across
/// modules. It can be also serialized for use in a future build.
///
/// TODO: Update the following conent
/// To use a StableHashTree you must already have a way to take some sequence of
/// data and use llvm::stable_hash to turn that sequence into a
/// std::vector<llvm::stable_hash> (ie StableHashSequence). Each of these hash
/// sequences can be inserted into a StableHashTree where the beginning of a
/// unique sequence starts from the root of the tree and ends at a Terminal
/// (IsTerminal) node.
///
/// This StableHashTree was originally implemented as part of the EuroLLVM 2020
/// talk "Global Machine Outliner for ThinLTO":
///
///   https://llvm.org/devmtg/2020-04/talks.html#TechTalk_58
///
/// This talk covers how a global stable hash tree is used to collect
/// information about valid MachineOutliner Candidates across modules, and used
/// to inform modules where matching candidates are encountered but occur in
/// less frequency and as a result are ignored by the MachineOutliner had there
/// not been a global stable hash tree in use (assuming FullLTO is disabled).
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_CODEGENDATA_OUTLINEDHASHTREE_H
#define LLVM_CODEGENDATA_OUTLINEDHASHTREE_H

#include "llvm/ADT/StableHashing.h"
#include "llvm/ObjectYAML/YAML.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/raw_ostream.h"

#include <unordered_map>
#include <vector>

namespace llvm {

/// \brief A HashNode is an entry in an OutlinedHashTree, holding a Hash value
/// and a collection of Successors (other HashNodes). If a HashNode has
/// Terminals (>0), it signifies the end of a llvm::stable_hash sequence with
// that occurrence count.
struct HashNode {
  stable_hash Hash;
  unsigned Terminals;
  std::unordered_map<stable_hash, std::unique_ptr<HashNode>> Successors;
};

/// \brief HashNodeStable is the serialized, stable, and compact representation
/// of a HashNode.
struct HashNodeStable {
  llvm::yaml::Hex64 Hash;
  unsigned Terminals;
  std::vector<unsigned> SuccessorIds;
};

class OutlinedHashTree {

  /// Graph traversal callback types.
  ///{
  using EdgeCallbackFn =
      std::function<void(const HashNode *, const HashNode *)>;
  using NodeCallbackFn = std::function<void(const HashNode *)>;
  ///}

  using HashSequence = std::vector<stable_hash>;
  using HashSequencePair = std::pair<std::vector<stable_hash>, unsigned>;

  /// Walks every edge and node in the OutlinedHashTree and calls CallbackEdge
  /// for the edges and CallbackNode for the nodes with the stable_hash for
  /// the source and the stable_hash of the sink for an edge. These generic
  /// callbacks can be used to traverse a OutlinedHashTree for the purpose of
  /// print debugging or serializing it.
  void walkGraph(EdgeCallbackFn CallbackEdge,
                 NodeCallbackFn CallbackNode) const;

public:
  /// Walks the nodes of a OutlinedHashTree using walkGraph.
  void walkVertices(NodeCallbackFn Callback) const {
    walkGraph([](const HashNode *A, const HashNode *B) {}, Callback);
  }

  /// Release all hash nodes except the root hash node.
  void clear() {
    assert(getRoot()->Hash == 0 && getRoot()->Terminals == 0);
    getRoot()->Successors.clear();
  }

  /// \returns true if the hash tree has only the root hash node.
  bool empty() { return size() == 1; }

  /// Uses walkVertices to print a OutlinedHashTree.
  /// If a \p DebugMap is provided, then it will be used to provide richer
  /// output.
  void print(raw_ostream &OS = llvm::errs(),
             std::unordered_map<stable_hash, std::string> DebugMap = {}) const;

  void dump() const { print(llvm::errs()); }

  /// \returns the size of a OutlinedHashTree by traversing it. If
  /// \p GetTerminalCountOnly is true, it only counts the terminal nodes
  /// (meaning it returns the size of the number of hash sequences in a
  /// OutlinedHashTree).
  size_t size(bool GetTerminalCountOnly = false) const {
    size_t Size = 0;
    walkVertices([&Size, GetTerminalCountOnly](const HashNode *N) {
      Size += (N && (!GetTerminalCountOnly || N->Terminals));
    });
    return Size;
  }

  size_t depth() const {
    size_t Size = 0;

    std::unordered_map<const HashNode *, size_t> DepthMap;

    walkGraph(
        [&DepthMap](const HashNode *Src, const HashNode *Dst) {
          size_t Depth = DepthMap[Src];
          DepthMap[Dst] = Depth + 1;
        },
        [&Size, &DepthMap](const HashNode *N) {
          Size = std::max(Size, DepthMap[N]);
        });

    return Size;
  }

  const HashNode *getRoot() const { return &Root; }
  HashNode *getRoot() { return &Root; }

  /// Inserts a \p Sequence into a OutlinedHashTree. The last node in the
  /// sequence will set IsTerminal to true in OutlinedHashTree.
  void insert(const HashSequencePair &SequencePair);

  /// Merge the given Tree into this OutlinedHashTree.
  void merge(const OutlinedHashTree *Tree);

  /// \returns the matching count if \p Sequence exists in a OutlinedHashTree.
  unsigned find(const HashSequence &Sequence) const;

private:
  /// OutlinedHashTree is a compact representation of a set of stable_hash
  /// sequences. It allows for for efficient walking of these sequences for
  /// matching purposes. HashTreeImpl is the root node of this tree. Its Hash
  /// value is 0, and its Successors are the beginning of StableHashSequences
  /// inserted into the OutlinedHashTree.
  HashNode Root;
};

} // namespace llvm

#endif
