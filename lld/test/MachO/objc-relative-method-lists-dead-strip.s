# REQUIRES: aarch64
# RUN: split-file %s %t
# RUN: llvm-mc -filetype=obj -triple=arm64-apple-macos11 %t/mixed.s -o %t/mixed.o
# RUN: %no-arg-lld -dylib -arch arm64 -platform_version macos 11.0 11.0 -dead_strip %t/mixed.o -o %t/stripped
# RUN: llvm-readobj --sections %t/stripped | FileCheck %s --check-prefix=LIVE
# RUN: %no-arg-lld -dylib -arch arm64 -platform_version macos 11.0 11.0 %t/mixed.o -o %t/all
# RUN: llvm-readobj --sections %t/all | FileCheck %s --check-prefix=ALL
# RUN: %no-arg-lld -dylib -arch arm64 -platform_version macos 11.0 11.0 -no_objc_relative_method_lists -dead_strip %t/mixed.o -o %t/absolute
# RUN: llvm-readobj --sections %t/absolute | FileCheck %s --check-prefix=ABS
# RUN: llvm-mc -filetype=obj -triple=arm64-apple-macos11 %t/dead-only.s -o %t/dead-only.o
# RUN: %no-arg-lld -dylib -arch arm64 -platform_version macos 11.0 11.0 -dead_strip %t/dead-only.o -o %t/empty
# RUN: llvm-readobj --sections %t/empty | FileCheck %s --check-prefix=EMPTY

## Lists are collected before dead stripping. Only the live protocol list
## should reach the encoder or cause the creation of a selector reference.
## The unused Clang class list exercises the pre-existing list families too.
# LIVE: Name: __objc_methlist
# LIVE: Size: 0x14
# LIVE: Name: __objc_selrefs
# LIVE: Size: 0x8

## Without dead stripping, all three lists still get encoded.
# ALL: Name: __objc_methlist
# ALL: Size: 0x3C
# ALL: Name: __objc_selrefs
# ALL: Size: 0x18

## Disabling relative conversion preserves the live absolute list.
# ABS-NOT: Name: __objc_methlist
# ABS: Name: __objc_const
# ABS: Size: 0x20
# ABS-NOT: Name: __objc_selrefs
# ABS-NOT: Name: __objc_methlist

# EMPTY-NOT: Name: __objc_methlist
# EMPTY-NOT: Name: __objc_selrefs

#--- mixed.s
.text
.globl _body
_body:
  ret

.section __TEXT,__objc_methname,cstring_literals
LliveName:
  .asciz "liveRequirement"
LdeadProtocolName:
  .asciz "deadRequirement"
LdeadClassName:
  .asciz "deadMethod"
.section __TEXT,__objc_methtype,cstring_literals
Ltype:
  .asciz "v16@0:8"
.section __DATA,__objc_const
.p2align 3
.globl __PROTOCOL_INSTANCE_METHODS_Live
.private_extern __PROTOCOL_INSTANCE_METHODS_Live
__PROTOCOL_INSTANCE_METHODS_Live:
  .long 24
  .long 1
  .quad LliveName
  .quad Ltype
  .quad 0
.globl __OBJC_$_PROTOCOL_CLASS_METHODS_OPT_Dead
.private_extern __OBJC_$_PROTOCOL_CLASS_METHODS_OPT_Dead
__OBJC_$_PROTOCOL_CLASS_METHODS_OPT_Dead:
  .long 24
  .long 1
  .quad LdeadProtocolName
  .quad Ltype
  .quad 0
.globl __OBJC_$_INSTANCE_METHODS_Dead
.private_extern __OBJC_$_INSTANCE_METHODS_Dead
__OBJC_$_INSTANCE_METHODS_Dead:
  .long 24
  .long 1
  .quad LdeadClassName
  .quad Ltype
  .quad _body
.section __DATA,__data
.globl _anchor
_anchor:
  .quad __PROTOCOL_INSTANCE_METHODS_Live
.subsections_via_symbols

#--- dead-only.s
.text
.globl _body
_body:
  ret
.section __TEXT,__objc_methname,cstring_literals
Lname:
  .asciz "unusedRequirement"
.section __TEXT,__objc_methtype,cstring_literals
Ltype:
  .asciz "v16@0:8"
.section __DATA,__objc_const
.p2align 3
.globl __PROTOCOL_INSTANCE_METHODS_Dead
.private_extern __PROTOCOL_INSTANCE_METHODS_Dead
__PROTOCOL_INSTANCE_METHODS_Dead:
  .long 24
  .long 1
  .quad Lname
  .quad Ltype
  .quad 0
.subsections_via_symbols
