//===- GlobalMergeFunctionsIO.cpp - Global merge functions for IO -*- C++ -===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains IO helpers to write and read global merge functions info.
//
//===----------------------------------------------------------------------===//

#include "llvm/Transforms/IPO/GlobalMergeFunctionsIO.h"
#include "llvm/Support/FileSystem.h"

#define DEBUG_TYPE "global-merge-func"

using namespace llvm;
using namespace llvm::support;

static void
writeConstLocationMap(support::endian::Writer &Writer,
                      InstOpndIdConstHashMapTy &InstOpndIndexToConstHash) {
  Writer.write<uint32_t>(InstOpndIndexToConstHash.size());

  for (const auto &[LocPair, ConstHash] : InstOpndIndexToConstHash) {
    Writer.write<uint32_t>(LocPair.first);
    Writer.write<uint32_t>(LocPair.second);
    Writer.write<uint64_t>(ConstHash);
  }
}

static void
readConstLocationMap(const char *&Data,
                     InstOpndIdConstHashMapTy &InstOpndIndexToConstHash) {
  auto NumLocationMap =
      endian::readNext<uint32_t, endianness::little, unaligned>(Data);

  for (unsigned I = 0; I < NumLocationMap; ++I) {
    auto InstIndex =
        endian::readNext<uint32_t, endianness::little, unaligned>(Data);
    auto OpndIndex =
        endian::readNext<uint32_t, endianness::little, unaligned>(Data);
    auto ConstHash =
        endian::readNext<uint64_t, endianness::little, unaligned>(Data);
    InstOpndIndexToConstHash[std::make_pair(InstIndex, OpndIndex)] = ConstHash;
  }
}

static void
writeStableFunctionMap(support::endian::Writer &Writer,
                       StringMap<int> NameToIdMap,
                       StableHashToStableFuncsTy &StableHashToStableFuncs) {
  Writer.write<uint32_t>(StableHashToStableFuncs.size());

  for (auto &[StableHash, StableFuncs] : StableHashToStableFuncs) {
    Writer.write<uint64_t>(StableHash);
    Writer.write<uint32_t>(StableFuncs.size());

    for (auto &SF : StableFuncs) {
      // Skip writing stable hash which has the same key.
      assert(StableHash == SF.StableHash);
      Writer.write<int32_t>(NameToIdMap[SF.Name]);
      Writer.write<int32_t>(NameToIdMap[SF.ModuleIdentifier]);
      Writer.write<int32_t>(SF.CountInsts);
      Writer.write<bool>(SF.IsMergeCandidate);
      writeConstLocationMap(Writer, SF.InstOpndIndexToConstHash);
    }
  }
}

static void
readStableFunctionMap(const char *&Data, std::vector<std::string> &Names,
                      StableHashToStableFuncsTy &StableHashToStableFuncs) {
  auto NumStableFunctionMap =
      endian::readNext<uint32_t, endianness::little, unaligned>(Data);

  for (unsigned I = 0; I < NumStableFunctionMap; ++I) {
    auto StableHashKey =
        endian::readNext<uint64_t, endianness::little, unaligned>(Data);
    auto NumStableFuncs =
        endian::readNext<uint32_t, endianness::little, unaligned>(Data);

    for (unsigned J = 0; J < NumStableFuncs; ++J) {
      StableFunction SF;
      // Restore stable hash from the same key.
      SF.StableHash = StableHashKey;
      SF.Name =
          Names[endian::readNext<int32_t, endianness::little, unaligned>(Data)];
      SF.ModuleIdentifier =
          Names[endian::readNext<int32_t, endianness::little, unaligned>(Data)];
      SF.CountInsts =
          endian::readNext<int32_t, endianness::little, unaligned>(Data);
      SF.IsMergeCandidate =
          endian::readNext<bool, endianness::little, unaligned>(Data);
      readConstLocationMap(Data, SF.InstOpndIndexToConstHash);

      StableHashToStableFuncs[StableHashKey].push_back(SF);
    }
  }
}

void GlobalMergeFunctionsIO::write(raw_ostream &OutputStream,
                                   MergeFunctionInfo &MFI) {
  support::endian::Writer Writer(OutputStream, endianness::little);
  // A very simple header. Write a Magic
  Writer.write<uint32_t>(GlobalMergeFunctionsIO::GMF_MAGIC);

  // Build a name to id map
  std::vector<std::string> Names;
  StringMap<int> NameToIdMap;
  auto insertToNameMap = [&](std::string &Name) {
    if (!NameToIdMap.count(Name)) {
      NameToIdMap.insert({Name, Names.size()});
      Names.push_back(Name);
    }
  };
  for (auto &[StableHash, StableFuncs] : MFI.StableHashToStableFuncs) {
    for (auto &SF : StableFuncs) {
      insertToNameMap(SF.Name);
      insertToNameMap(SF.ModuleIdentifier);
    }
  }

  // Write Names
  Writer.write<uint32_t>(Names.size());
  for (auto &Name : Names)
    Writer.OS << Name << '\0';

  // Write StableFunctionMap
  writeStableFunctionMap(Writer, NameToIdMap, MFI.StableHashToStableFuncs);

  // Write IsMerged
  Writer.write<bool>(MFI.IsMerged);
}

void GlobalMergeFunctionsIO::read(MemoryBuffer &Buffer,
                                  MergeFunctionInfo &MFI) {
  // Magic has been already read from the call-site.
  const char *Data = Buffer.getBufferStart() + sizeof(GMF_MAGIC);

  // Read Names
  std::vector<std::string> Names;
  auto NumNames =
      endian::readNext<uint32_t, endianness::little, unaligned>(Data);
  for (unsigned I = 0; I < NumNames; ++I) {
    std::string Name(Data);
    Data += Name.size() + 1;
    Names.push_back(Name);
  }

  // Read StableFunctionMap
  readStableFunctionMap(Data, Names, MFI.StableHashToStableFuncs);

  // Read IsMerged
  MFI.IsMerged = endian::readNext<bool, endianness::little, unaligned>(Data);
}
