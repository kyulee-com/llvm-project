// RUN: %clang_cc1 -triple x86_64-linux-gnu -std=c++17 -emit-llvm \
// RUN:   -funique-internal-linkage-names -mconstructor-aliases -o - %s \
// RUN:   | FileCheck %s

static int known_target() { return 1; }
extern int known_alias() __attribute__((alias("_ZL12known_targetv")));

extern int late_alias() __attribute__((alias("_ZL11late_targetv")));
static int late_target() { return 2; }

extern "C" int late_c_alias()
    __attribute__((alias("_ZL13late_c_targetv")));
extern "C" {
static int late_c_target() { return 3; }
}

__attribute__((used)) static int collision_target() { return 4; }
extern "C" int exact_target() asm("_ZL16collision_targetv");
__attribute__((used)) static int use_exact_declaration() {
  return exact_target();
}
extern "C" int collision_alias()
    __attribute__((alias("_ZL16collision_targetv")));
extern "C" int exact_target() { return 5; }

namespace {
struct LateConstructor {
  LateConstructor();
};
} // namespace
extern "C" void late_constructor_alias()
    __attribute__((alias("_ZN12_GLOBAL__N_115LateConstructorC1Ev")));
LateConstructor::LateConstructor() {}
__attribute__((used)) static LateConstructor late_constructor;

// CHECK-DAG: @_Z11known_aliasv = alias i32 (), ptr @_ZL12known_targetv.[[HASH:__uniq\.[0-9]+]]
// CHECK-DAG: define internal{{.*}} i32 @_ZL12known_targetv.[[HASH]]()
// CHECK-DAG: @_Z10late_aliasv = alias i32 (), ptr @_ZL11late_targetv
// CHECK-DAG: define internal{{.*}} i32 @_ZL11late_targetv()
// CHECK-DAG: @late_c_alias = alias i32 (), ptr @_ZL13late_c_targetv
// CHECK-DAG: define internal{{.*}} i32 @_ZL13late_c_targetv()

// A compiler-visible symbol with the exact requested name wins, even if it is
// only a declaration when the alias is emitted.
// CHECK-DAG: define internal{{.*}} i32 @_ZL16collision_targetv.[[HASH]]()
// CHECK-DAG: define dso_local i32 @_ZL16collision_targetv()
// CHECK-DAG: @collision_alias = alias i32 (), ptr @_ZL16collision_targetv
// CHECK-DAG: call i32 @_ZL16collision_targetv()

// Existing constructor lowering may replace the referenced complete entry
// with the base entry. The replacement remains valid after the bailout.
// CHECK-DAG: @late_constructor_alias = alias void (), ptr @_ZN12_GLOBAL__N_115LateConstructorC2Ev.[[HASH]]
// CHECK-DAG: define internal void @_ZN12_GLOBAL__N_115LateConstructorC2Ev.[[HASH]](
