// RUN: %clang_cc1 -triple x86_64-linux-gnu -std=c++17 -emit-llvm \
// RUN:   -funique-internal-linkage-names=functions -o - %s \
// RUN:   | FileCheck %s --check-prefix=FUNCTIONS-ITANIUM \
// RUN:       --implicit-check-not=__uniq
// RUN: %clang_cc1 -triple x86_64-linux-gnu -std=c++17 -emit-llvm \
// RUN:   -funique-internal-linkage-names=all -o - %s \
// RUN:   | FileCheck %s --check-prefix=ALL-ITANIUM
// RUN: %clang_cc1 -triple x86_64-pc-windows-msvc -std=c++17 -emit-llvm \
// RUN:   -funique-internal-linkage-names=functions -o - %s \
// RUN:   | FileCheck %s --check-prefix=FUNCTIONS-MS \
// RUN:       --implicit-check-not=__uniq
// RUN: %clang_cc1 -triple x86_64-pc-windows-msvc -std=c++17 -emit-llvm \
// RUN:   -funique-internal-linkage-names=all -o - %s \
// RUN:   | FileCheck %s --check-prefix=ALL-MS

static int file_data = 1;
int *address_of_file_data() { return &file_data; }

int read_local_data() {
  static int local_data = 2;
  return local_data;
}

extern int make_value();
int read_guarded() {
  static int guarded = make_value();
  return guarded;
}

static thread_local int tls_data = make_value();
int read_tls_data() { return tls_data; }

namespace {
int anonymous_data = 3;
}
int *address_of_anonymous_data() { return &anonymous_data; }

static int stable_data asm("stable_data") = 4;
int *address_of_stable_data() { return &stable_data; }

#ifndef _WIN32
int f() {
  static int x = 7;
  return x;
}
extern "C" int local_alias __attribute__((alias("_ZZ1fvE1x")));

namespace {
struct KnownMember {
  static int value;
};
int KnownMember::value = 5;
struct LateMember {
  static int value;
};
} // namespace
extern "C" int known_member_alias
    __attribute__((alias("_ZN12_GLOBAL__N_111KnownMember5valueE")));
extern "C" int late_member_alias
    __attribute__((alias("_ZN12_GLOBAL__N_110LateMember5valueE")));
int LateMember::value = 6;
#endif

#ifdef _WIN32
static int windows_alias_target = 5;
extern int windows_alias __attribute__((alias("windows_alias_target")));
int read_windows_alias() { return windows_alias; }
extern int late_windows_alias
    __attribute__((alias("late_windows_alias_target")));
static int late_windows_alias_target = 6;
#endif

// FUNCTIONS-ITANIUM-DAG: @_ZL9file_data = internal global i32 1
// FUNCTIONS-ITANIUM-DAG: @_ZZ15read_local_datavE10local_data = internal global i32 2
// FUNCTIONS-ITANIUM-DAG: @_ZZ12read_guardedvE7guarded = internal global i32 0
// FUNCTIONS-ITANIUM-DAG: @_ZGVZ12read_guardedvE7guarded = internal global i64 0
// FUNCTIONS-ITANIUM-DAG: @_ZL8tls_data = internal thread_local global i32 0
// FUNCTIONS-ITANIUM-DAG: @_ZN12_GLOBAL__N_114anonymous_dataE = internal global i32 3
// FUNCTIONS-ITANIUM-DAG: @stable_data = internal global i32 4
// FUNCTIONS-ITANIUM-DAG: @_ZZ1fvE1x = internal global i32 7
// FUNCTIONS-ITANIUM-DAG: @local_alias = alias i32, ptr @_ZZ1fvE1x

// ALL-ITANIUM-DAG: @_ZL9file_data.[[HASH:__uniq\.[0-9]+]] = internal global i32 1
// ALL-ITANIUM-DAG: @_ZZ15read_local_datavE10local_data.[[HASH]] = internal global i32 2
// ALL-ITANIUM-DAG: @_ZZ12read_guardedvE7guarded.[[HASH]] = internal global i32 0
// ALL-ITANIUM-DAG: @_ZGVZ12read_guardedvE7guarded.[[HASH]] = internal global i64 0
// ALL-ITANIUM-DAG: @_ZL8tls_data.[[HASH]] = internal thread_local global i32 0
// ALL-ITANIUM-DAG: @_ZTHL8tls_data = internal alias void (), ptr @__tls_init
// ALL-ITANIUM-DAG: define internal{{.*}} ptr @_ZTWL8tls_data()
// ALL-ITANIUM-DAG: @_ZN12_GLOBAL__N_114anonymous_dataE.[[HASH]] = internal global i32 3
// ALL-ITANIUM-DAG: @stable_data = internal global i32 4
// ALL-ITANIUM-DAG: @_ZZ1fvE1x.[[HASH]] = internal global i32 7
// ALL-ITANIUM-DAG: @local_alias = alias i32, ptr @_ZZ1fvE1x.[[HASH]]
// ALL-ITANIUM-DAG: @_ZN12_GLOBAL__N_111KnownMember5valueE.[[HASH]] = internal global i32 5
// ALL-ITANIUM-DAG: @known_member_alias = alias i32, ptr @_ZN12_GLOBAL__N_111KnownMember5valueE.[[HASH]]
// ALL-ITANIUM-DAG: @_ZN12_GLOBAL__N_110LateMember5valueE = internal global i32 6
// ALL-ITANIUM-DAG: @late_member_alias = alias i32, ptr @_ZN12_GLOBAL__N_110LateMember5valueE

// FUNCTIONS-MS-DAG: @file_data = internal global i32 1
// FUNCTIONS-MS-DAG: @"?local_data@?1??read_local_data@@YAHXZ@4HA" = internal global i32 2
// FUNCTIONS-MS-DAG: @"?guarded@?1??read_guarded@@YAHXZ@4HA" = internal global i32 0
// FUNCTIONS-MS-DAG: @"?$TSS0@?1??read_guarded@@YAHXZ@4HA" = internal global i32 0
// FUNCTIONS-MS-DAG: @"?anonymous_data@?A0x{{[0-9A-F]+}}@@3HA" = internal global i32 3
// FUNCTIONS-MS-DAG: @stable_data = internal global i32 4

// ALL-MS-DAG: @"?file_data@@3HA.[[MS_HASH:__uniq\.[0-9]+]]" = internal global i32 1
// ALL-MS-DAG: @"?local_data@?1??read_local_data@@YAHXZ@4HA.[[MS_HASH]]" = internal global i32 2
// ALL-MS-DAG: @"?guarded@?1??read_guarded@@YAHXZ@4HA.[[MS_HASH]]" = internal global i32 0
// ALL-MS-DAG: @"?$TSS0@?1??read_guarded@@YAHXZ@4HA.[[MS_HASH]]" = internal global i32 0
// ALL-MS-DAG: @"?anonymous_data@?A0x{{[0-9A-F]+}}@@3HA.[[MS_HASH]]" = internal global i32 3
// ALL-MS-DAG: @stable_data = internal global i32 4
// ALL-MS-DAG: @"?windows_alias_target@@3HA.[[MS_HASH]]" = internal global i32 5
// ALL-MS-DAG: @"?windows_alias@@3HA" = dso_local alias i32, ptr @"?windows_alias_target@@3HA.[[MS_HASH]]"
// ALL-MS-DAG: @late_windows_alias_target = internal global i32 6
// ALL-MS-DAG: @"?late_windows_alias@@3HA" = dso_local alias i32, ptr @late_windows_alias_target
