//=-- CodeGenDataWriter.cpp - Codegen Data ---------------------------------=//
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

#include "llvm/CodeGenData/CodeGenDataWriter.h"
#include "llvm/Support/Endian.h"
#include "llvm/Support/EndianStream.h"

#define DEBUG_TYPE "cg-data-writer"

using namespace llvm;

namespace llvm {

// TODO: These are same as PatchItem and ProfOStream, which we should refactor
// them into a common place.
// A struct to define how the data stream should be patched. For Indexed
// profiling, only uint64_t data type is needed.
struct CGDataPatchItem {
  uint64_t Pos; // Where to patch.
  uint64_t *D;  // Pointer to an array of source data.
  int N;        // Number of elements in \c D array.
};

// A wrapper class to abstract writer stream with support of bytes
// back patching.
class CGDataOStream {
public:
  CGDataOStream(raw_fd_ostream &FD)
      : IsFDOStream(true), OS(FD), LE(FD, llvm::endianness::little) {}
  CGDataOStream(raw_string_ostream &STR)
      : IsFDOStream(false), OS(STR), LE(STR, llvm::endianness::little) {}

  uint64_t tell() { return OS.tell(); }
  void write(uint64_t V) { LE.write<uint64_t>(V); }
  void write32(uint32_t V) { LE.write<uint32_t>(V); }
  void write8(uint8_t V) { LE.write<uint8_t>(V); }

  // \c patch can only be called when all data is written and flushed.
  // For raw_string_ostream, the patch is done on the target string
  // directly and it won't be reflected in the stream's internal buffer.
  void patch(ArrayRef<CGDataPatchItem> P) {
    using namespace support;

    if (IsFDOStream) {
      raw_fd_ostream &FDOStream = static_cast<raw_fd_ostream &>(OS);
      const uint64_t LastPos = FDOStream.tell();
      for (const auto &K : P) {
        FDOStream.seek(K.Pos);
        for (int I = 0; I < K.N; I++)
          write(K.D[I]);
      }
      // Reset the stream to the last position after patching so that users
      // don't accidentally overwrite data. This makes it consistent with
      // the string stream below which replaces the data directly.
      FDOStream.seek(LastPos);
    } else {
      raw_string_ostream &SOStream = static_cast<raw_string_ostream &>(OS);
      std::string &Data = SOStream.str(); // with flush
      for (const auto &K : P) {
        for (int I = 0; I < K.N; I++) {
          uint64_t Bytes =
              endian::byte_swap<uint64_t, llvm::endianness::little>(K.D[I]);
          Data.replace(K.Pos + I * sizeof(uint64_t), sizeof(uint64_t),
                       (const char *)&Bytes, sizeof(uint64_t));
        }
      }
    }
  }

  // If \c OS is an instance of \c raw_fd_ostream, this field will be
  // true. Otherwise, \c OS will be an raw_string_ostream.
  bool IsFDOStream;
  raw_ostream &OS;
  support::endian::Writer LE;
};

} // end namespace llvm

void CodeGenDataWriter::addRecord(const OutlinedHashTreeRecord Record) {
  assert(Record.HashTree && "empty hash tree in the record");
  HashTreeRecord = Record;
}

Error CodeGenDataWriter::write(raw_fd_ostream &OS) {
  CGDataOStream COS(OS);
  return writeImpl(COS);
}

Error CodeGenDataWriter::write(raw_string_ostream &OS) {
  CGDataOStream COS(OS);
  return writeImpl(COS);
}

Error CodeGenDataWriter::writeHeader(CGDataOStream &COS) {
  using namespace support;
  IndexedCGData::Header Header;
  Header.Magic = IndexedCGData::Magic;
  Header.Version = IndexedCGData::Version;

  // Set the CGDataType depending on the kind.
  if (static_cast<bool>(Kind & CGDataKind::FunctionOutlinedHashTree))
    Header.CGDataType |= static_cast<uint32_t>(CGDataKind::FunctionOutlinedHashTree);

  Header.OutlinedHashTreeOffset = 0;

  // Only write out up to the CGDataType. We need to remember the offest of the
  // remaing fields to allow back patching later.
  COS.write(Header.Magic);
  COS.write32(Header.Version);
  COS.write32(Header.CGDataType);

  // Save the location of Header.OutlinedHashTreeOffset field in \c COS.
  OutlinedHashTreeOffset = COS.tell();

  // Reserve the space for OutlinedHashTreeOffset field.
  COS.write(0);

  return Error::success();
}

Error CodeGenDataWriter::writeImpl(CGDataOStream &COS) {
  if (Error E = writeHeader(COS))
    return E;

  uint64_t OutlinedHashTreeFieldStart = COS.tell();
  HashTreeRecord.serialize(COS.OS);

  // Back patch the offsets.
  CGDataPatchItem PatchItems[] = {
        {OutlinedHashTreeFieldStart, &OutlinedHashTreeOffset, 1}};
  COS.patch(PatchItems);

  return Error::success();
}
