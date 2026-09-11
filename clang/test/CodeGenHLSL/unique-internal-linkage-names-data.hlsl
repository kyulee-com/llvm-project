// RUN: %clang_cc1 -triple spirv-pc-vulkan-compute -x hlsl -emit-llvm \
// RUN:   -finclude-default-header -disable-llvm-passes -o - %s \
// RUN:   | FileCheck %s --implicit-check-not=__uniq
// RUN: %clang_cc1 -triple spirv-pc-vulkan-compute -x hlsl -emit-llvm \
// RUN:   -finclude-default-header -disable-llvm-passes \
// RUN:   -funique-internal-linkage-names=all -o - %s \
// RUN:   | FileCheck %s --implicit-check-not=__uniq

struct S {
  uint value;
};

// Pipeline-initialized variables are externally visible even when declared
// static, so data uniquing must not change their interface name.
[[vk::push_constant]] static S data;

// CHECK: @_ZL4data = external hidden addrspace(13) externally_initialized global %struct.S

[numthreads(1, 1, 1)]
void main() {
  uint value = data.value;
}
