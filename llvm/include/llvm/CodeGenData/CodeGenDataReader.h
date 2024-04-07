//===- CodeGenDataReader.h --------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
/// \file
///
/// Defines CodeGen Data Reader
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_CODEGENDATA_CODEGENDATAREADER_H
#define LLVM_CODEGENDATA_CODEGENDATAREADER_H

#include "llvm/CodeGenData/CodeGenData.h"
#include "llvm/CodeGenData/OutlinedHashTree.h"

namespace llvm {

class CodeGenDataReader {
  cgdata_error LastError = cgdata_error::success;
  std::string LastErrorMsg;

public:
  CodeGenDataReader() = default;
  virtual ~CodeGenDataReader() = default;

  /// Read the header.  Required before reading first record.
  virtual Error read() = 0;
  /// Return the codegen data version.
  virtual uint32_t getVersion() const = 0;
  /// Return the codegen data kind.
  virtual CGDataKind getDataKind() const = 0;
  /// Return true if the data has an outlined hash tree.
  virtual bool hasOutlinedHashTree() const = 0;

  static Expected<std::unique_ptr<CodeGenDataReader>> create();

protected:
  /// Set the current error and return same.
  Error error(cgdata_error Err, const std::string &ErrMsg = "") {
    LastError = Err;
    LastErrorMsg = ErrMsg;
    if (Err == cgdata_error::success)
      return Error::success();
    return make_error<CGDataError>(Err, ErrMsg);
  }

  Error error(Error &&E) {
    handleAllErrors(std::move(E), [&](const CGDataError &IPE) {
      LastError = IPE.get();
      LastErrorMsg = IPE.getMessage();
    });
    return make_error<CGDataError>(LastError, LastErrorMsg);
  }

  /// Clear the current error and return a successful one.
  Error success() { return error(cgdata_error::success); }
};

class IndexedCodeGenDataReader : public CodeGenDataReader {
  /// The profile data file contents.
  std::unique_ptr<MemoryBuffer> DataBuffer;
  /// The header
  IndexedCGData::Header Header;
  /// The outlined hash tree
  std::unique_ptr<OutlinedHashTree> HashTree;

public:
  IndexedCodeGenDataReader(std::unique_ptr<MemoryBuffer> DataBuffer)
      : DataBuffer(std::move(DataBuffer)) {}
  IndexedCodeGenDataReader(const IndexedCodeGenDataReader &) = delete;
  IndexedCodeGenDataReader &
  operator=(const IndexedCodeGenDataReader &) = delete;

  static Expected<std::unique_ptr<IndexedCodeGenDataReader>>
  create(const Twine &Path);
  static Expected<std::unique_ptr<IndexedCodeGenDataReader>>
  create(std::unique_ptr<MemoryBuffer> Buffer);
  static bool hasFormat(const MemoryBuffer &Buffer);

  /// Read the contents including the header.
  Error read() override;
  /// Return the codegen data version.
  uint32_t getVersion() const override { return Header.Version; }
  /// Return the codegen data kind.
  CGDataKind getDataKind() const override {
    return static_cast<CGDataKind>(Header.DataKind);
  }
  /// Return true if the header indicates the data has an outlined hash tree.
  /// This does not mean that the data is still available.
  bool hasOutlinedHashTree() const override {
    return Header.DataKind &
           static_cast<uint32_t>(CGDataKind::FunctionOutlinedHashTree);
  }
  /// Return the outlined hash tree that is released from the reader.
  std::unique_ptr<OutlinedHashTree> releaseOutlinedHashTree() {
    return std::move(HashTree);
  }
  /// Return the outlined hash tree as read-only data.
  const OutlinedHashTree *getOutlinedHashTree() { return HashTree.get(); }
};

} // end namespace llvm

#endif // LLVM_CODEGENDATA_CODEGENDATAREADER_H
