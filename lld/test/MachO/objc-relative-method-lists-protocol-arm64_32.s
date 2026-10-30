# REQUIRES: aarch64
# RUN: llvm-mc -filetype=obj -triple=arm64_32-apple-watchos7.0 %s -o %t.o
# RUN: %no-arg-lld -dylib -arch arm64_32 -platform_version watchos 7.0 7.0 %t.o -o %t
# RUN: llvm-readobj --sections %t | FileCheck %s --check-prefix=SECTION
# RUN: llvm-objdump --macho --full-contents --section=__objc_methlist %t | FileCheck %s --check-prefix=DATA

## A 32-bit list does not shrink, but still gets the relative-format flag.
## No selref is present in the input: the linker must create a four-byte pointer
## with a 32-bit relocation, and share it between both methods.
# SECTION: Name: __objc_methlist
# SECTION: Size: 0x20
# SECTION: Name: __objc_selrefs
# SECTION: Size: 0x4

## The two null IMP slots are at offsets 16 and 28 in this 32-byte list.
# DATA: 8000000c 00000002 {{([0-9a-f]{8})}} {{([0-9a-f]{8})}}
# DATA-NEXT: {{[0-9a-f]+}} 00000000 {{([0-9a-f]{8})}} {{([0-9a-f]{8})}} 00000000

.section __TEXT,__objc_methname,cstring_literals
Lname:
 .asciz "requirement"
.section __TEXT,__objc_methtype,cstring_literals
Ltype:
 .asciz "v8@0:4"
.section __DATA,__objc_data
.p2align 2
.globl __PROTOCOL_INSTANCE_METHODS_Test
.private_extern __PROTOCOL_INSTANCE_METHODS_Test
__PROTOCOL_INSTANCE_METHODS_Test:
 .long 12
 .long 2
 .long Lname
 .long Ltype
 .long 0
 .long Lname
 .long Ltype
 .long 0
.section __DATA,__data
.p2align 2
.globl _lists
_lists:
 .long __PROTOCOL_INSTANCE_METHODS_Test
.subsections_via_symbols
