//===- GlobalMergeFunctionsIO.h - Global merge functions for IO -*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// This file defines I/O helpers to serialize or deserialize global merge
/// functions' artifacts.
///
//===----------------------------------------------------------------------===//

#ifndef PIKA_TRANSFORMS_UTILS_GLOBALMERGEFUNCTIONS_IO_H
#define PIKA_TRANSFORMS_UTILS_GLOBALMERGEFUNCTIONS_IO_H

#include "llvm/ADT/StringRef.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/EndianStream.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/IPO/GlobalMergeFunctions.h"
#include <map>

namespace llvm {

struct GlobalMergeFunctionsIO {
  // GMF magic value in little endian format.
  // \251 G    M    F
  // 0xFB 0x47 0x4D 0x46
  static const int GMF_MAGIC = 0x464D47FB;
  static void write(raw_ostream &OutputStream, MergeFunctionInfo &MFI);
  static void read(MemoryBuffer &Buffer, MergeFunctionInfo &MFI);
};

} // end namespace llvm
#endif // PIKA_TRANSFORMS_UTILS_GLOBALMERGEFUNCTIONS_IO_H
