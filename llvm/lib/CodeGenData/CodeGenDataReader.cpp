//===-- CodeGenDataReader.cpp ---------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// TODO
//
//===----------------------------------------------------------------------===//

#include "llvm/CodeGenData/CodeGenDataReader.h"
#include "llvm/CodeGenData/OutlinedHashTreeRecord.h"
#include "llvm/Support/MemoryBuffer.h"

#define DEBUG_TYPE "cg-data-reader"

using namespace llvm;

namespace llvm {

static Expected<std::unique_ptr<MemoryBuffer>>
setupMemoryBuffer(const Twine &Filename) {
  auto BufferOrErr = MemoryBuffer::getFileOrSTDIN(Filename);
  if (auto EC = BufferOrErr.getError())
    return errorCodeToError(EC);

  return std::move(BufferOrErr.get());
}

Error IndexedCodeGenDataReader::read() {
  using namespace support;

  // The smallest header with the version 1 is 24 bytes
  const unsigned MinHeaderSize = 24;
  if (DataBuffer->getBufferSize() < MinHeaderSize)
    return error(cgdata_error::bad_header);

  auto *Start =
      reinterpret_cast<const unsigned char *>(DataBuffer->getBufferStart());
  auto HeaderOr = IndexedCGData::Header::readFromBuffer(Start);
  if (!HeaderOr)
    return HeaderOr.takeError();

  Header = HeaderOr.get();
  if (hasOutlinedHashTree()) {
    const unsigned char *Ptr = Start + Header.OutlinedHashTreeOffset;
    auto Tree = std::make_unique<OutlinedHashTree>();
    OutlinedHashTreeRecord Record(Tree.get());
    Record.deserialize(Ptr);

    HashTree = std::move(Tree);
  }

  return success();
}

Expected<std::unique_ptr<IndexedCodeGenDataReader>>
IndexedCodeGenDataReader::create(const Twine &Path) {
  // Set up the buffer to read.
  auto BufferOrError = setupMemoryBuffer(Path);
  if (Error E = BufferOrError.takeError())
    return std::move(E);
  return IndexedCodeGenDataReader::create(std::move(BufferOrError.get()));
}

Expected<std::unique_ptr<IndexedCodeGenDataReader>>
IndexedCodeGenDataReader::create(std::unique_ptr<MemoryBuffer> Buffer) {
  // Create the reader.
  if (!IndexedCodeGenDataReader::hasFormat(*Buffer))
    return make_error<CGDataError>(cgdata_error::bad_magic);
  auto Reader = std::make_unique<IndexedCodeGenDataReader>(std::move(Buffer));

  // Initialize the reader and return the result.
  if (Error E = Reader->read())
    return std::move(E);

  return std::move(Reader);
}

bool IndexedCodeGenDataReader::hasFormat(const MemoryBuffer &DataBuffer) {
  using namespace support;

  if (DataBuffer.getBufferSize() < 8)
    return false;
  uint64_t Magic = endian::read<uint64_t, llvm::endianness::little, aligned>(
      DataBuffer.getBufferStart());
  // Verify that it's magical.
  return Magic == IndexedCGData::Magic;
}

} // end namespace llvm
