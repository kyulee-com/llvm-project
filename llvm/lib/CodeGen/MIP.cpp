//===- MIP.cpp - Machine IR Profile ---------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/MIP/MIP.h"
#include "llvm/ADT/Twine.h"

namespace llvm {
namespace MachineProfile {

std::string fileTypeToString(const MIPFileType &FileType) {
  switch (FileType) {
  case MIP_FILE_TYPE_RAW:
    return "Raw";
  case MIP_FILE_TYPE_MAP:
    return "Map";
  case MIP_FILE_TYPE_PROFILE:
    return "Profile";
  case MIP_FILE_TYPE_CALL_EDGE_SAMPLES:
    return "Call Edge Samples";
  default:
    return "Unknown File Type";
  }
}

void MIRProfile::getOrderedProfiles(
    std::vector<MFProfile> &OrderedProfiles) const {
  for (auto &Profile : Profiles) {
    if (Profile.RawProfileCount > 0) {
      // Only consider functions that have been profiled.
      OrderedProfiles.push_back(Profile);
    }
  }
  std::stable_sort(OrderedProfiles.begin(), OrderedProfiles.end(),
                   [](auto &A, auto &B) {
                     // NOTE: The equation was modified to avoid division.
                     // A.OrderSum / A.ProfileCount < B.OrderSum /
                     // B.ProfileCount
                     return A.FunctionOrderSum * B.RawProfileCount <
                            B.FunctionOrderSum * A.RawProfileCount;
                   });
}

} // namespace MachineProfile
} // namespace llvm
