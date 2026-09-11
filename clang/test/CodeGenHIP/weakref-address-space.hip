// RUN: %clang_cc1 -triple amdgcn-amd-amdhsa -fcuda-is-device \
// RUN:   -emit-llvm -o - %s | FileCheck %s

#define __device__ __attribute__((device))

__device__ static int ref __attribute__((weakref("missing")));
__device__ int use(void) { return ref; }

// CHECK: @missing = extern_weak addrspace(1) global i32
// CHECK-LABEL: define{{.*}} i32 @_Z3usev()
// CHECK: load i32, ptr addrspace(1) @missing
