# REQUIRES: aarch64
## An ObjC class stub loads an interposable class defined in this image from a
## bound __got slot. Like other interposable binds, the slot looks the class up
## in this image under the default two-level namespace, and in the flat
## namespace with -flat_namespace. _objc_msgSend keeps its dylib lookup.

# RUN: rm -rf %t; split-file %s %t
# RUN: llvm-mc -filetype=obj -triple=arm64-apple-macos %t/objc.s -o %t/objc-arm64.o
# RUN: llvm-mc -filetype=obj -triple=arm64e-apple-macos %t/objc.s -o %t/objc-arm64e.o
# RUN: llvm-mc -filetype=obj -triple=arm64-apple-macos %t/test.s -o %t/arm64.o
# RUN: llvm-mc -filetype=obj -triple=arm64e-apple-macos %t/test.s -o %t/arm64e.o
# RUN: %lld -arch arm64 -dylib -install_name /usr/lib/libobjc.A.dylib \
# RUN:   %t/objc-arm64.o -o %t/libobjc-arm64.dylib
# RUN: %lld -arch arm64e -platform_version macos 13.0 13.0 -dylib \
# RUN:   -install_name /usr/lib/libobjc.A.dylib %t/objc-arm64e.o \
# RUN:   -o %t/libobjc-arm64e.dylib

# RUN: %lld -arch arm64 -platform_version macos 13.0 13.0 -dylib \
# RUN:   -interposable -objc_stubs_fast \
# RUN:   %t/arm64.o %t/libobjc-arm64.dylib -o %t/arm64-fast.dylib
# RUN: llvm-objdump --macho --dyld-info %t/arm64-fast.dylib | \
# RUN:   FileCheck %s -DSCOPE=this-image -DOBJC=libobjc
# RUN: %lld -arch arm64 -platform_version macos 13.0 13.0 -dylib \
# RUN:   -interposable -objc_stubs_small \
# RUN:   %t/arm64.o %t/libobjc-arm64.dylib -o %t/arm64-small.dylib
# RUN: llvm-objdump --macho --dyld-info %t/arm64-small.dylib | \
# RUN:   FileCheck %s -DSCOPE=this-image -DOBJC=libobjc
# RUN: %lld -arch arm64 -platform_version macos 13.0 13.0 -dylib \
# RUN:   -interposable -flat_namespace -objc_stubs_fast \
# RUN:   %t/arm64.o %t/libobjc-arm64.dylib -o %t/arm64-flat.dylib
# RUN: llvm-objdump --macho --dyld-info %t/arm64-flat.dylib | \
# RUN:   FileCheck %s -DSCOPE=flat-namespace -DOBJC=flat-namespace

## Legacy bind opcodes carry the same lookup.
# RUN: %lld -arch arm64 -platform_version macos 11.0 11.0 -dylib \
# RUN:   -interposable -objc_stubs_fast %t/arm64.o %t/libobjc-arm64.dylib \
# RUN:   -o %t/arm64-legacy.dylib
# RUN: llvm-objdump --macho --bind %t/arm64-legacy.dylib | \
# RUN:   FileCheck %s --check-prefix=LEGACY

# RUN: %lld -arch arm64e -platform_version macos 13.0 13.0 -dylib \
# RUN:   -interposable -objc_stubs_fast %t/arm64e.o %t/libobjc-arm64e.dylib \
# RUN:   -o %t/arm64e-fast.dylib
# RUN: llvm-objdump --macho --dyld-info %t/arm64e-fast.dylib | \
# RUN:   FileCheck %s -DSCOPE=this-image -DOBJC=libobjc
# RUN: %lld -arch arm64e -platform_version macos 13.0 13.0 -dylib \
# RUN:   -interposable -objc_stubs_small %t/arm64e.o %t/libobjc-arm64e.dylib \
# RUN:   -o %t/arm64e-small.dylib
# RUN: llvm-objdump --macho --dyld-info %t/arm64e-small.dylib | \
# RUN:   FileCheck %s -DSCOPE=this-image -DOBJC=libobjc
# RUN: %lld -arch arm64e -platform_version macos 13.0 13.0 -dylib \
# RUN:   -interposable -flat_namespace -objc_stubs_fast %t/arm64e.o \
# RUN:   %t/libobjc-arm64e.dylib -o %t/arm64e-flat.dylib
# RUN: llvm-objdump --macho --dyld-info %t/arm64e-flat.dylib | \
# RUN:   FileCheck %s -DSCOPE=flat-namespace -DOBJC=flat-namespace

# CHECK-LABEL: dyld information:
# CHECK-DAG:   __DATA_CONST __got {{.*}} bind {{.*}} [[SCOPE]] _OBJC_CLASS_$_Foo
# CHECK-DAG:   bind {{.*}} [[OBJC]] _objc_msgSend

# LEGACY-LABEL: Bind table:
# LEGACY-DAG:   __DATA_CONST __got {{.*}} pointer 0 this-image _OBJC_CLASS_$_Foo
# LEGACY-DAG:   __DATA_CONST __got {{.*}} pointer 0 libobjc    _objc_msgSend

#--- objc.s
.globl _objc_msgSend
.p2align 2
_objc_msgSend:
  ret

#--- test.s
.section __DATA,__objc_data
.globl _OBJC_CLASS_$_Foo
.p2align 3
_OBJC_CLASS_$_Foo:
  .quad 0

.text
.globl _main
.p2align 2
_main:
  bl "_objc_msgSendClass$alloc$_OBJC_CLASS_$_Foo"
  ret

.subsections_via_symbols
