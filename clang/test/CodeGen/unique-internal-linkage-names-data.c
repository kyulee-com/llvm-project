// RUN: %clang_cc1 -triple x86_64-linux-gnu -emit-llvm \
// RUN:   -funique-internal-linkage-names -o - %s \
// RUN:   | FileCheck %s --check-prefix=FUNCTIONS
// RUN: %clang_cc1 -triple x86_64-linux-gnu -emit-llvm \
// RUN:   -funique-internal-linkage-names=functions -o - %s \
// RUN:   | FileCheck %s --check-prefix=FUNCTIONS
// RUN: %clang_cc1 -triple x86_64-linux-gnu -emit-llvm \
// RUN:   -funique-internal-linkage-names=all -o - %s \
// RUN:   | FileCheck %s --check-prefix=ALL
// RUN: %clang_cc1 -triple x86_64-pc-windows-msvc -emit-llvm \
// RUN:   -funique-internal-linkage-names=functions -o - %s \
// RUN:   | FileCheck %s --check-prefix=FUNCTIONS-MS
// RUN: %clang_cc1 -triple x86_64-pc-windows-msvc -emit-llvm \
// RUN:   -funique-internal-linkage-names=all -o - %s \
// RUN:   | FileCheck %s --check-prefix=ALL-MS

__attribute__((used)) static int known_data = 1;
extern int known_data_alias __attribute__((alias("known_data")));

extern int late_data_alias __attribute__((alias("late_data")));
static int late_data = 2;

__attribute__((used)) static int stable_data asm("stable_data") = 3;

__attribute__((used)) static int function_target(void) { return 7; }

int *address_of_local_data(void) {
  static int local_data;
  return &local_data;
}

__attribute__((used)) static int weak_target = 4;
static int weak_ref __attribute__((weakref("weak_target")));
int read_weak_ref(void) { return weak_ref; }

static int mixed_weak_ref __attribute__((weakref("mixed_target")));
int read_mixed_weak_ref(void) { return mixed_weak_ref; }
__attribute__((used)) static int mixed_target = 5;
extern int mixed_alias __attribute__((alias("mixed_target")));

// The bare flag and =functions preserve the existing function-only behavior.
// FUNCTIONS-DAG: @known_data = internal global i32 1
// FUNCTIONS-DAG: @known_data_alias = alias i32, ptr @known_data
// FUNCTIONS-DAG: @late_data = internal global i32 2
// FUNCTIONS-DAG: @late_data_alias = alias i32, ptr @late_data
// FUNCTIONS-DAG: @stable_data = internal global i32 3
// FUNCTIONS-DAG: @address_of_local_data.local_data = internal global i32 0
// FUNCTIONS-DAG: load i32, ptr @weak_target
// FUNCTIONS-DAG: define internal i32 @_ZL15function_targetv.[[FUNCTION_HASH:__uniq\.[0-9]+]]()

// A known target keeps its unique name, while an alias seen first preserves
// the later target's ordinary assembler name.
// ALL-DAG: @_ZL10known_data.[[HASH:__uniq\.[0-9]+]] = internal global i32 1
// ALL-DAG: @known_data_alias = alias i32, ptr @_ZL10known_data.[[HASH]]
// ALL-DAG: @late_data = internal global i32 2
// ALL-DAG: @late_data_alias = alias i32, ptr @late_data
// ALL-DAG: @stable_data = internal global i32 3
// ALL-DAG: @_ZZ21address_of_local_dataE10local_data.[[HASH]] = internal global i32 0
// ALL-DAG: define internal i32 @_ZL15function_targetv.[[HASH]]()

// Weak references remain literal because their target may be external or
// supplied by module assembly.
// ALL-DAG: @_ZL11weak_target.[[HASH]] = internal global i32 4
// ALL-DAG: @weak_target = extern_weak global i32
// ALL-DAG: load i32, ptr @weak_target
// ALL-DAG: @_ZL12mixed_target.[[HASH]] = internal global i32 5
// ALL-DAG: @mixed_target = extern_weak global i32
// ALL-DAG: @mixed_alias = alias i32, ptr @_ZL12mixed_target.[[HASH]]

// FUNCTIONS-MS-DAG: @known_data = internal global i32 1
// FUNCTIONS-MS-DAG: @late_data = internal global i32 2
// FUNCTIONS-MS-DAG: @address_of_local_data.local_data = internal global i32 0
// ALL-MS-DAG: @"?known_data@@3HA.[[MS_HASH:__uniq\.[0-9]+]]" = internal global i32 1
// ALL-MS-DAG: @known_data_alias = dso_local alias i32, ptr @"?known_data@@3HA.[[MS_HASH]]"
// ALL-MS-DAG: @late_data = internal global i32 2
// ALL-MS-DAG: @late_data_alias = dso_local alias i32, ptr @late_data
// ALL-MS-DAG: @"?local_data@?1??address_of_local_data@@9@4HA.[[MS_HASH]]" = internal global i32 0
