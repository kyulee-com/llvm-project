# REQUIRES: aarch64
## On arm64e, calls to an interposable definition go through __auth_stubs, whose
## __auth_got slots are authenticated binds. Like ld64, the binds look the
## symbol up in this image under the default two-level namespace, and in the
## flat namespace with -flat_namespace.

# RUN: llvm-mc -filetype=obj -triple=arm64e-apple-macos %s -o %t.o
# RUN: %lld -arch arm64e -platform_version macos 13.0 13.0 -dylib \
# RUN:   -interposable %t.o -o %t.dylib
# RUN: llvm-objdump --macho --dyld-info --chained-fixups %t.dylib | \
# RUN:   FileCheck %s -DSCOPE=this-image -DORDINAL=0
# RUN: %lld -arch arm64e -platform_version macos 13.0 13.0 -dylib \
# RUN:   -interposable -flat_namespace %t.o -o %t-flat.dylib
# RUN: llvm-objdump --macho --dyld-info --chained-fixups %t-flat.dylib | \
# RUN:   FileCheck %s -DSCOPE=flat-namespace -DORDINAL=-2

# CHECK-LABEL: dyld information:
# CHECK:       __DATA_CONST __auth_got {{.*}} bind 0x0000 true IA 0x0 [[SCOPE]] _foo
# CHECK:       dyld chained import[0]
# CHECK-NEXT:    lib_ordinal = [[ORDINAL]] ([[SCOPE]])
# CHECK-NEXT:    weak_import = 0
# CHECK-NEXT:    name_offset = 0 (_foo)

.globl _foo
.p2align 2
_foo:
  ret

_caller:
  bl _foo
  ret

.subsections_via_symbols
