// RUN: %clang_cc1 -triple x86_64-unknown-linux -emit-llvm \
// RUN:   -debug-info-kind=limited -dwarf-version=5 \
// RUN:   -funique-internal-linkage-names=functions -o - %s \
// RUN:   | FileCheck %s --check-prefix=FUNCTIONS --implicit-check-not=__uniq
// RUN: %clang_cc1 -triple x86_64-unknown-linux -emit-llvm \
// RUN:   -debug-info-kind=limited -dwarf-version=5 \
// RUN:   -funique-internal-linkage-names=all -o - %s \
// RUN:   | FileCheck %s --check-prefix=ALL

static int data;
int *address_of_data(void) { return &data; }

// FUNCTIONS: @data = internal global i32 0
// FUNCTIONS: distinct !DIGlobalVariable(name: "data"{{.*}})

// ALL: @_ZL4data.[[HASH:__uniq\.[0-9]+]] = internal global i32 0
// ALL: distinct !DIGlobalVariable(name: "data", linkageName: "_ZL4data.[[HASH]]"{{.*}})
