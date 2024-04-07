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

#ifndef LLVM_CODEGENDATA_CODEGENDATAWRITER_H
#define LLVM_CODEGENDATA_CODEGENDATAWRITER_H

#include "llvm/CodeGenData/CodeGenData.h"
#include "llvm/CodeGenData/OutlinedHashTreeRecord.h"
#include "llvm/Support/Error.h"

namespace llvm {

class CGDataOStream;

class CodeGenDataWriter {
  /// The outlined hash tree
  OutlinedHashTreeRecord HashTreeRecord;

  // An enum describing the attributes of the cg data.
  CGDataKind DataKind = CGDataKind::Unknown;

public:
  CodeGenDataWriter() = default;
  ~CodeGenDataWriter() = default;

  /// Add the outlined hash tree record.
  void addRecord(const OutlinedHashTreeRecord Record);

  /// Write the profile to \c OS
  Error write(raw_fd_ostream &OS);

  /// Write the profile to a string output stream \c OS
  Error write(raw_string_ostream &OS);

  /// Write the profile in text format to \c OS
  Error writeText(raw_fd_ostream &OS);

  /// Update the attributes of the current CGData from the attributes
  /// specified. For now, each CGDataKind is assumed to be orthogonal.
  Error mergeCGDataKind(const CGDataKind Other) {
    // If the kind is unset, this is the first CGData we are merging so just
    // set it to the given type.
    if (DataKind == CGDataKind::Unknown) {
      DataKind = Other;
      return Error::success();
    }

    // Now we update the CGData type with the bits that are set.
    DataKind |= Other;
    return Error::success();
  }

  /// Return the attributes of the current CGData.
  CGDataKind getCGDataKind() const { return DataKind; }

  /// Return true if the header indicates the data has an outlined hash tree.
  bool hasOutlinedHashTree() const {
    return static_cast<uint32_t>(DataKind) &
           static_cast<uint32_t>(CGDataKind::FunctionOutlinedHashTree);
  }

private:
  uint64_t OutlinedHashTreeOffset;

  Error writeHeader(CGDataOStream &COS);

  Error writeImpl(CGDataOStream &COS);
};

} // end namespace llvm

#endif // LLVM_CODEGENDATA_CODEGENDATAWRITER_H
