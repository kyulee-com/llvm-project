// RUN: %clang_cc1 -triple x86_64-linux-gnu -O1 -emit-llvm \
// RUN:   -fsanitize=signed-integer-overflow -o - %s \
// RUN:   | FileCheck %s --check-prefix=SANITIZER --implicit-check-not=extern_weak
// RUN: %clang_cc1 -triple x86_64-apple-darwin -fblocks -DBLOCK_RUNTIME \
// RUN:   -emit-llvm -o - %s \
// RUN:   | FileCheck %s --check-prefix=BLOCK --implicit-check-not=extern_weak

#ifndef BLOCK_RUNTIME

// CodeGen also creates this sanitizer entry point without an associated
// FunctionDecl. That direct use must promote the declaration first created by
// the weakref.
static void runtime_ref(void)
    __attribute__((weakref("__ubsan_handle_add_overflow_abort")));
int add(int lhs, int rhs) {
  if (runtime_ref)
    runtime_ref();
  return lhs + rhs;
}

// SANITIZER-LABEL: define dso_local {{.*}}i32 @add(
// SANITIZER: call void @__ubsan_handle_add_overflow_abort()
// SANITIZER: call void @__ubsan_handle_add_overflow_abort(ptr
// SANITIZER: declare void @__ubsan_handle_add_overflow_abort()

#else

// A global block likewise creates this runtime declaration without a VarDecl.
// It must supersede the declaration first created by the weakref.
static void *block_runtime_ref
    __attribute__((weakref("_NSConcreteGlobalBlock")));
void *read_block_runtime(void) { return block_runtime_ref; }
void (^global_block)(void) = ^{};

// BLOCK: @_NSConcreteGlobalBlock = external global ptr
// BLOCK: load ptr, ptr @_NSConcreteGlobalBlock

#endif
