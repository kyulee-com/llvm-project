// RUN: %clang_cc1 -triple x86_64-linux-gnu -emit-llvm \
// RUN:   -funique-internal-linkage-names -o - %s | FileCheck %s
// RUN: %clang_cc1 -triple x86_64-linux-gnu -emit-llvm -o - %s \
// RUN:   | FileCheck %s --check-prefix=NO-UNIQUE --implicit-check-not=__uniq

static int known_target(void) { return 1; }
extern int known_alias(void) __attribute__((alias("known_target")));

extern int late_alias(void) __attribute__((alias("late_target")));
static int late_target(void) { return 2; }

static int forward_declared_target(void);
extern int forward_declared_alias(void)
    __attribute__((alias("forward_declared_target")));
static int forward_declared_target(void) { return 5; }

__attribute__((used)) static int weak_target(void) { return 3; }
static int weak_ref(void) __attribute__((weakref("weak_target")));
__attribute__((used)) static int use_known_weak_ref(void) {
  return weak_ref();
}

static int mixed_weak_ref(void) __attribute__((weakref("mixed_target")));
int call_mixed_weak_ref(void) { return mixed_weak_ref(); }
static int mixed_target(void) { return 5; }
extern int mixed_alias(void) __attribute__((alias("mixed_target")));

static void known_implementation(void) {}
static void (*known_resolver(void))(void) { return known_implementation; }
void known_dispatch(void) __attribute__((ifunc("known_resolver")));

void late_dispatch(void) __attribute__((ifunc("late_resolver")));
static void late_implementation(void) {}
static void (*late_resolver(void))(void) { return late_implementation; }

// A target known before the symbolic reference keeps its unique name.
// CHECK-DAG: @known_alias = alias i32 (), ptr @_ZL12known_targetv.[[HASH:__uniq\.[0-9]+]]
// CHECK-DAG: define internal i32 @_ZL12known_targetv.[[HASH]]()
// CHECK-DAG: @known_dispatch = ifunc void (), ptr @_ZL14known_resolverv.[[HASH]]
// CHECK-DAG: define internal ptr @_ZL14known_resolverv.[[HASH]]()

// An unresolved alias or ifunc conservatively opts its later target out.
// CHECK-DAG: @late_alias = alias i32 (), ptr @late_target
// CHECK-DAG: define internal i32 @late_target()
// CHECK-DAG: @forward_declared_alias = alias i32 (), ptr @forward_declared_target
// CHECK-DAG: define internal i32 @forward_declared_target()
// CHECK-DAG: @late_dispatch = ifunc void (), ptr @late_resolver
// CHECK-DAG: define internal ptr @late_resolver()

// A weakref may denote an external or module-assembly definition. It is not
// redirected based on an internal declaration with the same ordinary name.
// CHECK-DAG: call i32 @weak_target()
// CHECK-DAG: declare extern_weak i32 @weak_target()
// CHECK-DAG: define internal i32 @_ZL11weak_targetv.[[HASH]]()

// A weakref placeholder does not hide a known target from a regular alias.
// The weakref remains literal while the alias follows the unique definition.
// CHECK-DAG: call i32 @mixed_target()
// CHECK-DAG: declare extern_weak i32 @mixed_target()
// CHECK-DAG: @mixed_alias = alias i32 (), ptr @_ZL12mixed_targetv.[[HASH]]
// CHECK-DAG: define internal i32 @_ZL12mixed_targetv.[[HASH]]()

// NO-UNIQUE-DAG: @known_alias = alias i32 (), ptr @known_target
// NO-UNIQUE-DAG: @late_alias = alias i32 (), ptr @late_target
// NO-UNIQUE-DAG: @known_dispatch = ifunc void (), ptr @known_resolver
// NO-UNIQUE-DAG: @late_dispatch = ifunc void (), ptr @late_resolver
