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
#include "llvm/IR/PassManager.h"
#include <mutex>

namespace llvm {
namespace cgdata {

class CodeGenData {
  // outlined function info
  std::unique_ptr<OutlinedHashTree> GlobalOutlinedHashTree;

  // This flag is initialized when -emit-codegen-data is set.
  // Or, with -ftwo-codegen-rounds,
  bool emitCGData;

  CodeGenData() = default;
  static std::unique_ptr<CodeGenData> instance;
  static std::once_flag onceFlag;

public:
  virtual ~CodeGenData() = default;
  static CodeGenData &getInstance();

  bool isWriteCGData();
  bool isReadCGData();
  void publishOutlinedFunctionInfo();
};

// struct CodeGenData {
bool isWriteCGData();

//};
// TODO

} // end namespace cgdata
} // end namespace llvm

#endif // LLVM_CODEGEN_PREPARE_H
