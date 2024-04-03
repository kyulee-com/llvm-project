//===- CodeGenData.h --------------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
/// \file
///
/// Defines an IR pass for CodeGen Prepare.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_CODEGENDATA_CODEGENDATA_H
#define LLVM_CODEGENDATA_CODEGENDATA_H

#include "llvm/CodeGenData/OutlinedHashTree.h"
#include "llvm/TargetParser/Triple.h"

#include <mutex>

namespace llvm {

enum CGDataSectKind {
#define CG_DATA_SECT_ENTRY(Kind, SectNameCommon, SectNameCoff, Prefix) Kind,
#include "llvm/CodeGenData/CodeGenData.inc"
};

std::string getCodeGenDataSectionName(CGDataSectKind CGSK,
                                      Triple::ObjectFormatType OF,
                                      bool AddSegmentInfo = true);

enum CGDataMode {
  None,
  Read,
  Write,
};

class CodeGenData {
  // outlined function info
  std::unique_ptr<OutlinedHashTree> GlobalOutlinedHashTree;

  // This flag is initialized when -emit-codegen-data is set.
  // Or, with -ftwo-codegen-rounds,
  bool emitCGData;

  static std::unique_ptr<CodeGenData> instance;
  static std::once_flag onceFlag;

  CodeGenData() = default;

public:
  ~CodeGenData() = default;

  static CodeGenData &getInstance();

  bool hasGlobalOutlinedHashTree() { return !GlobalOutlinedHashTree; }

  OutlinedHashTree *getGlobalOutlinedHashTree() {
    return GlobalOutlinedHashTree.get();
  }

  bool shouldWriteCGData();

  bool shouldReadCGData();

  void publishOutlinedHashTree();
};

namespace cgdata {

inline bool hasGlobalOutlinedHashTree() {
  return CodeGenData::getInstance().hasGlobalOutlinedHashTree();
}

inline bool shouldWriteCGData() {
  return CodeGenData::getInstance().shouldWriteCGData();
}

inline bool shouldReadCGData() {
  return CodeGenData::getInstance().shouldReadCGData();
}

inline OutlinedHashTree *getGlobalOutlinedHashTree() {
  return CodeGenData::getInstance().getGlobalOutlinedHashTree();
}

} // end namespace cgdata

namespace IndexedCGData {

const uint64_t Magic = 0x81636764617461ff; // "\xffcgdata\x81"

enum CGDataVersion {
  // Version 1 is the first version. This version support the outlined
  // hash tree.
  Version1 = 1,
  CurrentVersion = CG_DATA_INDEX_VERSION
};

struct Header {
  uint64_t Magic;
  uint32_t Version;
  uint32_t CGDataType;
  uint64_t OutlinedHashTreeOffset;

  // New fields should only be added at the end to ensure that the size
  // computation is correct. The methods below need to be updated to ensure that
  // the new field is read correctly.

  // Reads a header struct from the buffer.
  static Expected<Header> readFromBuffer(const unsigned char *Buffer) {} // TODO

  // Returns the size of the header in bytes for all valid fields based on the
  // version. I.e a older version header will return a smaller size.
  size_t size() const {} // TODO

  // Returns the format version in little endian. The header retains the version
  // in native endian of the compiler runtime.
  uint64_t formatVersion() const {} // TODO
};

} // end namespace IndexedCGData

} // end namespace llvm

#endif // LLVM_CODEGEN_PREPARE_H
