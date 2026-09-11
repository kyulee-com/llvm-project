// RUN: echo -n "GPU binary" > %t
// RUN: %clang_cc1 -x cuda -triple x86_64-unknown-linux-gnu \
// RUN:   -target-sdk-version=12.0 -fcuda-include-gpubinary %t \
// RUN:   -funique-internal-linkage-names=all -emit-llvm -o - %s \
// RUN:   | FileCheck %s --check-prefix=HOST
// RUN: %clang_cc1 -x cuda -triple x86_64-unknown-linux-gnu \
// RUN:   -fcuda-is-device -funique-internal-linkage-names=all \
// RUN:   -emit-llvm -o - %s | FileCheck %s --check-prefix=DEVICE
// RUN: %clang_cc1 -x cuda -triple x86_64-unknown-linux-gnu \
// RUN:   -target-sdk-version=12.0 -fcuda-include-gpubinary %t \
// RUN:   -funique-internal-linkage-names=functions -emit-llvm -o - %s \
// RUN:   | FileCheck %s --check-prefix=FUNCTIONS-HOST
// RUN: %clang_cc1 -x cuda -triple x86_64-unknown-linux-gnu \
// RUN:   -fcuda-is-device -funique-internal-linkage-names=functions \
// RUN:   -emit-llvm -o - %s | FileCheck %s --check-prefix=FUNCTIONS-DEVICE
// RUN: %clang_cc1 -x hip -triple x86_64-unknown-linux-gnu -fgpu-rdc \
// RUN:   -cuid=abc -funique-internal-linkage-names=all -emit-llvm \
// RUN:   -o %t.hip-host %s
// RUN: %clang_cc1 -x hip -triple amdgcn-amd-amdhsa -fcuda-is-device \
// RUN:   -fgpu-rdc -cuid=abc -funique-internal-linkage-names=all \
// RUN:   -emit-llvm -o %t.hip-device %s
// RUN: cat %t.hip-host %t.hip-device \
// RUN:   | FileCheck %s --check-prefix=HIP-RDC

#include "Inputs/cuda.h"

static __device__ int internal_data __attribute__((used)) = 1;
static __device__ __attribute__((used)) int internal_function() { return 4; }
static __device__ int externalized_data = 2;
static __host__ __device__ int read_externalized_data() {
  return externalized_data;
}
int use_externalized_data() { return read_externalized_data(); }

template <typename T> __device__ int template_data;
template __device__ int template_data<int>;
static __global__ __attribute__((used)) void use_template_data() {
  template_data<int> = 3;
}

// The registration name and device definition must agree. A variable that
// CUDA externalizes retains its existing unsuffixed device-side name.
// HOST-DAG: c"_ZL13internal_data.[[HASH:__uniq\.[0-9]+]]\00"
// HOST-DAG: c"_ZL17externalized_data\00"
// HOST-DAG: c"_Z13template_dataIiE\00"
// DEVICE-DAG: @_ZL13internal_data.[[HASH:__uniq\.[0-9]+]] = internal global i32 1
// DEVICE-DAG: define internal noundef i32 @_ZL17internal_functionv.[[HASH]]()
// DEVICE-DAG: @_ZL17externalized_data = externally_initialized global i32 2
// DEVICE-DAG: @_Z13template_dataIiE = {{.*}}global i32 0
// FUNCTIONS-HOST-DAG: c"_ZL13internal_data\00"
// FUNCTIONS-HOST-DAG: c"_ZL17externalized_data\00"
// FUNCTIONS-DEVICE-DAG: @_ZL13internal_data = internal global i32 1
// FUNCTIONS-DEVICE-DAG: define internal noundef i32 @_ZL17internal_functionv.[[FUNCTION_HASH:__uniq\.[0-9]+]]()
// FUNCTIONS-DEVICE-DAG: @_ZL17externalized_data = externally_initialized global i32 2

// HIP-RDC: c"_ZL13internal_data.[[HIP_HASH:__uniq\.[0-9]+]]\00"
// HIP-RDC: c"_ZL17externalized_data.static.[[CUID:[0-9a-f]+]]\00"
// HIP-RDC: @_ZL13internal_data.[[HIP_HASH]] = internal addrspace(1) global i32 1
// HIP-RDC: @_ZL17externalized_data.static.[[CUID]] = addrspace(1) externally_initialized global i32 2
