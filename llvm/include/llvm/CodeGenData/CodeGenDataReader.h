//===- CodeGenDataWriter.h --------------------------------------*- C++ -*-===//
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

#ifndef LLVM_CODEGENDATA_CODEGENDATAREADER_H
#define LLVM_CODEGENDATA_CODEGENDATAREADER_H

#include "llvm/IR/PassManager.h"

namespace llvm {
class CodeGenDataReader {};

class ObjectCodeGenDataReader : public CodeGenDataReader {};

class TextCodeGenDataReader : public CodeGenDataReader {};

class IndexedCodeGenDataReader : public CodeGenDataReader {};

} // end namespace llvm

#endif // LLVM_CODEGENDATA_CODEGENDATAREADER_H
