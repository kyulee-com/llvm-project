//===---- OutlinedHashTree.cpp ----------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
//===----------------------------------------------------------------------===//

#include "llvm/CodeGenData/OutlinedHashTree.h"

#include <stack>

#define DEBUG_TYPE "outlined-hash-tree"

using namespace llvm;

void OutlinedHashTree::walkGraph(EdgeCallbackFn CallbackEdge,
                                 NodeCallbackFn CallbackNode) const {
  std::stack<const HashNode *> Stack;
  Stack.push(getRoot());

  while (!Stack.empty()) {
    const auto *Current = Stack.top();
    Stack.pop();
    CallbackNode(Current);

    // Sorted walk for the stable output.
    std::map<stable_hash, const HashNode *> SortedSuccessors;
    for (const auto &P : Current->Successors)
      SortedSuccessors[P.first] = P.second.get();

    for (const auto &P : SortedSuccessors) {
      CallbackEdge(Current, P.second);
      Stack.push(P.second);
    }
  }
}

void OutlinedHashTree::print(
    llvm::raw_ostream &OS,
    std::unordered_map<stable_hash, std::string> DebugMap) const {

  std::unordered_map<const HashNode *, unsigned> NodeMap;

  walkVertices([&NodeMap](const HashNode *Current) {
    size_t Index = NodeMap.size();
    NodeMap[Current] = Index;
    assert(Index = NodeMap.size() + 1 &&
                   "Expected size of ModeMap to increment by 1");
  });

  bool IsFirstEntry = true;
  OS << "{";
  for (const auto &Entry : NodeMap) {
    if (!IsFirstEntry)
      OS << ",";
    OS << "\n";
    IsFirstEntry = false;
    OS << "  \"" << Entry.second << "\" : {\n";
    OS << "    \"hash\" : \"";
    OS.raw_ostream::write_hex(Entry.first->Hash);
    OS << "\",\n";

    OS << "    \"Terminals\" : "
       << "\"" << Entry.first->Terminals << "\",\n";

    // For debugging we want to provide a string representation of the hashing
    // source, such as a MachineInstr dump, etc. Not intended for production.
    auto MII = DebugMap.find(Entry.first->Hash);
    if (MII != DebugMap.end())
      OS << "    \"source\" : \"" << MII->second << "\",\n";

    OS << "    \"neighbors\" : [";

    bool IsFirst = true;
    for (const auto &Adj : Entry.first->Successors) {
      if (!IsFirst)
        OS << ",";
      IsFirst = false;
      OS << " \"";
      OS << NodeMap[Adj.second.get()];
      OS << "\" ";
    }

    OS << "]\n  }";
  }
  OS << "\n}\n";
  OS.flush();
}

void OutlinedHashTree::insert(const HashSequencePair &SequencePair) {
  const auto &Sequence = SequencePair.first;
  unsigned Count = SequencePair.second;

  HashNode *Current = getRoot();
  for (stable_hash StableHash : Sequence) {
    auto I = Current->Successors.find(StableHash);
    if (I == Current->Successors.end()) {
      std::unique_ptr<HashNode> Next = std::make_unique<HashNode>();
      HashNode *NextPtr = Next.get();
      NextPtr->Hash = StableHash;
      Current->Successors.emplace(StableHash, std::move(Next));
      Current = NextPtr;
      continue;
    }
    Current = I->second.get();
  }
  Current->Terminals += Count;
}

unsigned OutlinedHashTree::find(const HashSequence &Sequence) const {
  const HashNode *Current = getRoot();
  for (stable_hash StableHash : Sequence) {
    const auto I = Current->Successors.find(StableHash);
    if (I == Current->Successors.end())
      return 0;
    Current = I->second.get();
  }
  return Current->Terminals;
}
