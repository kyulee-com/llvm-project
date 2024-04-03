//=-- CodeGenData.cpp - Codegen Data ---------------------------------------=//
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

#include "llvm/CodeGenData/CodeGenData.h"
#include "llvm/Support/CommandLine.h"

#define DEBUG_TYPE "cg-data"

using namespace llvm;
using namespace cgdata;

cl::opt<bool>
    EmitCodeGenData("emit-codegen-data", cl::init(false), cl::Hidden,
                    cl::desc("Emit CodeGen Data into custom sections"));
cl::opt<std::string>
    UseCodeGenDataPath("use-codegen-data-path", cl::init(""), cl::Hidden,
                       cl::desc("Path to where .cgdata file is read"));

namespace {

const char *CodeGenDataSectNameCommon[] = {
#define CG_DATA_SECT_ENTRY(Kind, SectNameCommon, SectNameCoff, Prefix)         \
  SectNameCommon,
#include "llvm/CodeGenData/CodeGenData.inc"
};

const char *CodeGenDataSectNameCoff[] = {
#define CG_DATA_SECT_ENTRY(Kind, SectNameCommon, SectNameCoff, Prefix)         \
  SectNameCoff,
#include "llvm/CodeGenData/CodeGenData.inc"
};

const char *CodeGenDataSectNamePrefix[] = {
#define CG_DATA_SECT_ENTRY(Kind, SectNameCommon, SectNameCoff, Prefix) Prefix,
#include "llvm/CodeGenData/CodeGenData.inc"
};

} // namespace

namespace llvm {

std::string getCodeGenDataSectionName(CGDataSectKind CGSK,
                                      Triple::ObjectFormatType OF,
                                      bool AddSegmentInfo) {
  std::string SectName;

  if (OF == Triple::MachO && AddSegmentInfo)
    SectName = CodeGenDataSectNamePrefix[CGSK];

  if (OF == Triple::COFF)
    SectName += CodeGenDataSectNameCoff[CGSK];
  else
    SectName += CodeGenDataSectNameCommon[CGSK];

  return SectName;
}

std::unique_ptr<CodeGenData> CodeGenData::instance = nullptr;
std::once_flag CodeGenData::onceFlag;

CodeGenData &CodeGenData::getInstance() {
  std::call_once(CodeGenData::onceFlag, []() {
    auto *CGD = new CodeGenData();
    // Initialize outlined hash tree if the input file name is given
    if (EmitCodeGenData)
      CGD->emitCGData = true;
    else if (!UseCodeGenDataPath.empty()) {
      // TODO.
      // auto *Reader = IndexedCodeGenDataReader::create(UseCodeGenDataPath);
      // Reader->readHeader();
      // Reader->read();
      // FakeOutFunctionInfo.reset(Reader->getOutlinedFunctionInfo());
    }
    instance.reset(CGD);
  });
  return *(instance.get());
}

bool CodeGenData::shouldWriteCGData() { return emitCGData; }

bool CodeGenData::shouldReadCGData() {
  return !emitCGData && hasGlobalOutlinedHashTree() &&
         !GlobalOutlinedHashTree.get();
}

} // end namespace llvm
