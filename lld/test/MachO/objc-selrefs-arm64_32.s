# REQUIRES: aarch64
# RUN: llvm-mc -filetype=obj -triple=arm64_32-apple-watchos7.0 %s -o %t.o
# RUN: %no-arg-lld -dylib -arch arm64_32 -platform_version watchos 7.0 7.0 %t.o -o %t
# RUN: llvm-readobj --sections %t | FileCheck %s --check-prefix=SECTION
# RUN: llvm-objdump --macho --full-contents --section=__objc_methlist %t | FileCheck %s --check-prefix=DATA
# RUN: llvm-objdump --macho --rebase %t | FileCheck %s --check-prefix=REBASE

## The linker creates one four-byte selref shared by both methods. The input
## uses an existing Clang class-method-list prefix and nonnull implementations,
## so this test does not depend on Swift/protocol method-list support.
# SECTION: Name: __objc_methlist
# SECTION: Size: 0x20
# SECTION: Name: __objc_selrefs
# SECTION: Size: 0x4

# DATA: 8000000c 00000002
## Only the list reference and the synthesized selref need rebasing.
# REBASE-COUNT-2: pointer
# REBASE-NOT: pointer

.text
.globl _imp
_imp:
  ret
.section __TEXT,__objc_methname,cstring_literals
Lname:
  .asciz "method"
.section __TEXT,__objc_methtype,cstring_literals
Ltype:
  .asciz "v8@0:4"
.section __DATA,__objc_data
.p2align 2
.globl __OBJC_$_INSTANCE_METHODS_C
.private_extern __OBJC_$_INSTANCE_METHODS_C
__OBJC_$_INSTANCE_METHODS_C:
  .long 12
  .long 2
  .long Lname
  .long Ltype
  .long _imp
  .long Lname
  .long Ltype
  .long _imp
.section __DATA,__data
.p2align 2
.globl _lists
_lists:
  .long __OBJC_$_INSTANCE_METHODS_C
.subsections_via_symbols
