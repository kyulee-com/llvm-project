# REQUIRES: aarch64
# RUN: llvm-mc -filetype=obj -triple=arm64-apple-ios13.0 %s -o %t.o
# RUN: %no-arg-lld -dylib -arch arm64 -platform_version ios 14.0 14.0 -no_fixup_chains %t.o -o %t.small
# RUN: llvm-readobj --sections %t.small | FileCheck %s --check-prefix=SECTION
# RUN: llvm-objdump --macho --syms %t.small | FileCheck %s --check-prefix=SMALL
# RUN: llvm-objdump --macho --rebase %t.small | FileCheck %s --check-prefix=SMALL-REBASE
# RUN: %no-arg-lld -dylib -arch arm64 -platform_version ios 14.0 14.0 -no_fixup_chains -dead_strip %t.o -o %t.dead
# RUN: llvm-objdump --macho --syms %t.dead | FileCheck %s --check-prefix=SMALL
# RUN: %no-arg-lld -dylib -arch arm64 -platform_version ios 14.0 14.0 -no_fixup_chains -no_objc_relative_method_lists %t.o -o %t.disabled
# RUN: llvm-objdump --macho --syms %t.disabled | FileCheck %s --check-prefix=LARGE
# RUN: llvm-objdump --macho --rebase %t.disabled | FileCheck %s --check-prefix=LARGE-REBASE
# RUN: %no-arg-lld -dylib -arch arm64 -platform_version ios 13.0 14.0 -no_fixup_chains %t.o -o %t.old
# RUN: llvm-objdump --macho --syms %t.old | FileCheck %s --check-prefix=LARGE
# RUN: %no-arg-lld -dylib -arch arm64 -platform_version ios 13.0 14.0 -no_fixup_chains -objc_relative_method_lists %t.o -o %t.forced
# RUN: llvm-objdump --macho --syms %t.forced | FileCheck %s --check-prefix=SMALL

## Swift class/category lists use the same three-pointer layout as Clang.
## The last list has a similar but unrecognized name and remains absolute.
## Four one-entry lists occupy 4 * (8 + 3 * 4) bytes after conversion.
# SECTION: Name: __objc_methlist
# SECTION: Size: 0x50

# SMALL-DAG: __TEXT,__objc_methlist .hidden __INSTANCE_METHODS_C
# SMALL-DAG: __TEXT,__objc_methlist .hidden __CLASS_METHODS_C
# SMALL-DAG: __TEXT,__objc_methlist .hidden __CATEGORY_INSTANCE_METHODS_C
# SMALL-DAG: __TEXT,__objc_methlist .hidden __CATEGORY_CLASS_METHODS_C
# SMALL-DAG: __DATA,__objc_data .hidden __INSTANCE_METHOD_C

# LARGE-DAG: __DATA,__objc_data .hidden __INSTANCE_METHODS_C
# LARGE-DAG: __DATA,__objc_data .hidden __CLASS_METHODS_C
# LARGE-DAG: __DATA,__objc_data .hidden __CATEGORY_INSTANCE_METHODS_C
# LARGE-DAG: __DATA,__objc_data .hidden __CATEGORY_CLASS_METHODS_C
# LARGE-DAG: __DATA,__objc_data .hidden __INSTANCE_METHOD_C

## Five list references, three pointers in the unrecognized list, and a selref.
# SMALL-REBASE-COUNT-9: pointer
# SMALL-REBASE-NOT: pointer
## Five list references and three pointers in each of the five absolute lists.
# LARGE-REBASE-COUNT-20: pointer
# LARGE-REBASE-NOT: pointer

.text
.globl _imp
_imp:
  ret

.section __TEXT,__objc_methname,cstring_literals
Lselector:
  .asciz "method"
.section __TEXT,__objc_methtype,cstring_literals
Ltypes:
  .asciz "v16@0:8"

.macro method_list name
.section __DATA,__objc_data
.p2align 3
.globl \name
.private_extern \name
\name:
  .long 24
  .long 1
  .quad Lselector
  .quad Ltypes
  .quad _imp
.endm

method_list __INSTANCE_METHODS_C
method_list __CLASS_METHODS_C
method_list __CATEGORY_INSTANCE_METHODS_C
method_list __CATEGORY_CLASS_METHODS_C
method_list __INSTANCE_METHOD_C

.section __DATA,__data
.globl _lists
.p2align 3
_lists:
  .quad __INSTANCE_METHODS_C
  .quad __CLASS_METHODS_C
  .quad __CATEGORY_INSTANCE_METHODS_C
  .quad __CATEGORY_CLASS_METHODS_C
  .quad __INSTANCE_METHOD_C

.subsections_via_symbols
