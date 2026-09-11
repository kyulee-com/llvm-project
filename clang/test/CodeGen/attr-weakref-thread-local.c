// RUN: %clang_cc1 -triple x86_64-linux-gnu -emit-llvm -o - %s \
// RUN:   | FileCheck %s

static __thread int tls_ref __attribute__((weakref("tls_target")));
int use_tls_ref(void) { return tls_ref; }

// A direct declaration after the weakref must see the target as thread-local.
extern __thread int direct_tls_target asm("tls_target");
int use_direct_tls_target(void) { return direct_tls_target; }

static __thread int missing_tls_ref
    __attribute__((weakref("missing_tls")));
int use_missing_tls_ref(void) { return missing_tls_ref; }

// CHECK-DAG: @tls_target = external thread_local global i32
// CHECK-DAG: @missing_tls = extern_weak thread_local global i32
// CHECK-LABEL: define dso_local i32 @use_tls_ref()
// CHECK: call ptr @llvm.threadlocal.address.p0(ptr @tls_target)
// CHECK-LABEL: define dso_local i32 @use_direct_tls_target()
// CHECK: call ptr @llvm.threadlocal.address.p0(ptr @tls_target)
// CHECK-LABEL: define dso_local i32 @use_missing_tls_ref()
// CHECK: call ptr @llvm.threadlocal.address.p0(ptr @missing_tls)
