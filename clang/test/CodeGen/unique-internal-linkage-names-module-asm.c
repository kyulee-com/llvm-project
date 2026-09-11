// RUN: %clang_cc1 -triple x86_64-linux-gnu -emit-llvm \
// RUN:   -funique-internal-linkage-names=all -o - %s \
// RUN:   | FileCheck %s --implicit-check-not='@module_asm_target =' \
// RUN:       --implicit-check-not='@_ZL13stable_target.'

// Module assembly is opaque to Clang. Although it defines the ordinary name
// below, the compiler-visible source definition is the target of the alias.
asm(".globl module_asm_target\n"
    "module_asm_target:\n"
    ".long 99");
__attribute__((used)) static int module_asm_target = 1;
extern int source_alias __attribute__((alias("module_asm_target")));

// An explicit assembly label is the escape hatch when assembly and a source
// declaration need to share a stable spelling.
asm(".quad stable_target");
static int stable_target asm("stable_target") = 2;
extern int stable_alias __attribute__((alias("stable_target")));

// CHECK: ".globl module_asm_target"
// CHECK: ".quad stable_target"
// CHECK-DAG: @_ZL17module_asm_target.[[HASH:__uniq\.[0-9]+]] = internal global i32 1
// CHECK-DAG: @source_alias = alias i32, ptr @_ZL17module_asm_target.[[HASH]]
// CHECK-DAG: @stable_target = internal global i32 2
// CHECK-DAG: @stable_alias = alias i32, ptr @stable_target
// CHECK: !llvm.ident =
