# REQUIRES: aarch64, x86
# RUN: llvm-mc -filetype=obj -triple=arm64-apple-ios13.0 %s -o %t.arm64.o
# RUN: llvm-mc -filetype=obj -triple=x86_64-apple-ios13.0-simulator %s -o %t.x86.o
# RUN: %no-arg-lld -dylib -arch arm64 -platform_version ios 14.0 14.0 -no_fixup_chains %t.arm64.o -o %t.arm64
# RUN: llvm-objdump --macho --objc-meta-data %t.arm64 | FileCheck %s --check-prefix=REL
# RUN: llvm-readobj --sections %t.arm64 | FileCheck %s --check-prefix=SECTION
# RUN: %no-arg-lld -dylib -arch x86_64 -platform_version ios-simulator 14.0 14.0 -no_fixup_chains %t.x86.o -o %t.x86
# RUN: llvm-objdump --macho --objc-meta-data %t.x86 | FileCheck %s --check-prefix=REL
# RUN: %no-arg-lld -dylib -arch arm64 -platform_version ios 14.0 14.0 -no_fixup_chains -dead_strip %t.arm64.o -o %t.dead
# RUN: llvm-objdump --macho --objc-meta-data %t.dead | FileCheck %s --check-prefix=REL
# RUN: %no-arg-lld -dylib -arch arm64 -platform_version ios 14.0 14.0 -no_fixup_chains -no_objc_relative_method_lists %t.arm64.o -o %t.disabled
# RUN: llvm-objdump --macho --objc-meta-data %t.disabled | FileCheck %s --check-prefix=ABS
# RUN: %no-arg-lld -dylib -arch arm64 -platform_version ios 13.0 14.0 -no_fixup_chains %t.arm64.o -o %t.old
# RUN: llvm-objdump --macho --objc-meta-data %t.old | FileCheck %s --check-prefix=ABS
# RUN: %no-arg-lld -dylib -arch arm64 -platform_version ios 13.0 14.0 -no_fixup_chains -objc_relative_method_lists %t.arm64.o -o %t.forced
# RUN: llvm-objdump --macho --objc-meta-data %t.forced | FileCheck %s --check-prefix=REL

## Clang and Swift protocol lists have three-pointer entries, with null IMPs
## that have no relocations. Optional lists share the required-list prefix.
## Use two entries per list to verify that writing a null IMP advances both
## input and output offsets. A nonnull IMP exercises the ordinary relocation
## path, and an unrecognized list verifies mixed relative/absolute output.

## Eight two-entry lists: 8 * (8 + 2 * 12) bytes.
# SECTION: Name: __objc_methlist
# SECTION: Size: 0x100

# REL: name {{.*}} ClangProtocol
# REL: instanceMethods {{.*}} (struct method_list_t *)
# REL-NEXT: entsize 12 (relative)
# REL-NEXT: count 2
# REL-NEXT: name {{.*}} required_instance_1
# REL-NEXT: types {{.*}} v16@0:8
# REL-NEXT: imp 0x0 (0x{{[0-9a-f]+}})
# REL-NEXT: name {{.*}} required_instance_2
# REL-NEXT: types {{.*}} v16@0:8
# REL-NEXT: imp {{.*}} _imp
# REL: classMethods {{.*}} (struct method_list_t *)
# REL-NEXT: entsize 12 (relative)
# REL-NEXT: count 2
# REL-NEXT: name {{.*}} required_class_1
# REL-NEXT: types {{.*}} v16@0:8
# REL-NEXT: imp 0x0 (0x{{[0-9a-f]+}})
# REL-NEXT: name {{.*}} required_class_2
# REL-NEXT: types {{.*}} v16@0:8
# REL-NEXT: imp 0x0 (0x{{[0-9a-f]+}})
# REL: optionalInstanceMethods 0x[[CLANG_OPT_INSTANCE:[0-9a-f]+]]
# REL: optionalClassMethods 0x[[CLANG_OPT_CLASS:[0-9a-f]+]]

# REL: name {{.*}} SwiftProtocol
# REL: instanceMethods {{.*}} (struct method_list_t *)
# REL-NEXT: entsize 12 (relative)
# REL-NEXT: count 2
# REL-NEXT: name {{.*}} required_instance_1
# REL-NEXT: types {{.*}} v16@0:8
# REL-NEXT: imp 0x0 (0x{{[0-9a-f]+}})
# REL-NEXT: name {{.*}} required_instance_2
# REL-NEXT: types {{.*}} v16@0:8
# REL-NEXT: imp 0x0 (0x{{[0-9a-f]+}})
# REL: classMethods {{.*}} (struct method_list_t *)
# REL-NEXT: entsize 12 (relative)
# REL-NEXT: count 2
# REL-NEXT: name {{.*}} required_class_1
# REL-NEXT: types {{.*}} v16@0:8
# REL-NEXT: imp 0x0 (0x{{[0-9a-f]+}})
# REL-NEXT: name {{.*}} required_class_2
# REL-NEXT: types {{.*}} v16@0:8
# REL-NEXT: imp 0x0 (0x{{[0-9a-f]+}})
# REL: optionalInstanceMethods 0x[[SWIFT_OPT_INSTANCE:[0-9a-f]+]]
# REL: optionalClassMethods 0x[[SWIFT_OPT_CLASS:[0-9a-f]+]]

# REL: name {{.*}} OpaqueProtocol
# REL: instanceMethods {{.*}} (struct method_list_t *)
# REL-NEXT: entsize 24
# REL-NEXT: count 2
# REL-NEXT: name {{.*}} required_instance_1
# REL-NEXT: types {{.*}} v16@0:8
# REL-NEXT: imp 0x0

# REL: _OBJC_CLASS_$_ClangOptionalInstance
# REL: baseMethods 0x[[CLANG_OPT_INSTANCE]] (struct method_list_t *)
# REL-NEXT: entsize 12 (relative)
# REL-NEXT: count 2
# REL-NEXT: name {{.*}} optional_instance_1
# REL-NEXT: types {{.*}} v16@0:8
# REL-NEXT: imp 0x0 (0x{{[0-9a-f]+}})
# REL-NEXT: name {{.*}} optional_instance_2
# REL-NEXT: types {{.*}} v16@0:8
# REL-NEXT: imp 0x0 (0x{{[0-9a-f]+}})

# REL: _OBJC_CLASS_$_ClangOptionalClass
# REL: baseMethods 0x[[CLANG_OPT_CLASS]] (struct method_list_t *)
# REL-NEXT: entsize 12 (relative)
# REL-NEXT: count 2
# REL-NEXT: name {{.*}} optional_class_1
# REL-NEXT: types {{.*}} v16@0:8
# REL-NEXT: imp 0x0 (0x{{[0-9a-f]+}})
# REL-NEXT: name {{.*}} optional_class_2
# REL-NEXT: types {{.*}} v16@0:8
# REL-NEXT: imp 0x0 (0x{{[0-9a-f]+}})

# REL: _OBJC_CLASS_$_SwiftOptionalInstance
# REL: baseMethods 0x[[SWIFT_OPT_INSTANCE]] (struct method_list_t *)
# REL-NEXT: entsize 12 (relative)
# REL-NEXT: count 2
# REL-NEXT: name {{.*}} optional_instance_1
# REL-NEXT: types {{.*}} v16@0:8
# REL-NEXT: imp 0x0 (0x{{[0-9a-f]+}})
# REL-NEXT: name {{.*}} optional_instance_2
# REL-NEXT: types {{.*}} v16@0:8
# REL-NEXT: imp 0x0 (0x{{[0-9a-f]+}})

# REL: _OBJC_CLASS_$_SwiftOptionalClass
# REL: baseMethods 0x[[SWIFT_OPT_CLASS]] (struct method_list_t *)
# REL-NEXT: entsize 12 (relative)
# REL-NEXT: count 2
# REL-NEXT: name {{.*}} optional_class_1
# REL-NEXT: types {{.*}} v16@0:8
# REL-NEXT: imp 0x0 (0x{{[0-9a-f]+}})
# REL-NEXT: name {{.*}} optional_class_2
# REL-NEXT: types {{.*}} v16@0:8
# REL-NEXT: imp 0x0 (0x{{[0-9a-f]+}})

# ABS-NOT: (relative)
# ABS: name {{.*}} ClangProtocol
# ABS: instanceMethods {{.*}} (struct method_list_t *)
# ABS-NEXT: entsize 24
# ABS-NEXT: count 2
# ABS: classMethods {{.*}} (struct method_list_t *)
# ABS-NEXT: entsize 24
# ABS-NEXT: count 2
# ABS: optionalInstanceMethods
# ABS: optionalClassMethods
# ABS-NOT: (relative)
# ABS: name {{.*}} SwiftProtocol
# ABS: instanceMethods {{.*}} (struct method_list_t *)
# ABS-NEXT: entsize 24
# ABS-NEXT: count 2
# ABS: classMethods {{.*}} (struct method_list_t *)
# ABS-NEXT: entsize 24
# ABS-NEXT: count 2
# ABS: optionalInstanceMethods
# ABS: optionalClassMethods
# ABS-NOT: (relative)
# ABS: name {{.*}} OpaqueProtocol
# ABS: instanceMethods {{.*}} (struct method_list_t *)
# ABS-NEXT: entsize 24
# ABS-NEXT: count 2
# ABS-NOT: (relative)
# ABS: _OBJC_CLASS_$_ClangOptionalInstance
# ABS: baseMethods {{.*}} (struct method_list_t *)
# ABS-NEXT: entsize 24
# ABS-NEXT: count 2
# ABS-NOT: (relative)
# ABS: _OBJC_CLASS_$_ClangOptionalClass
# ABS: baseMethods {{.*}} (struct method_list_t *)
# ABS-NEXT: entsize 24
# ABS-NEXT: count 2
# ABS-NOT: (relative)
# ABS: _OBJC_CLASS_$_SwiftOptionalInstance
# ABS: baseMethods {{.*}} (struct method_list_t *)
# ABS-NEXT: entsize 24
# ABS-NEXT: count 2
# ABS-NOT: (relative)
# ABS: _OBJC_CLASS_$_SwiftOptionalClass
# ABS: baseMethods {{.*}} (struct method_list_t *)
# ABS-NEXT: entsize 24
# ABS-NEXT: count 2
# ABS-NOT: (relative)

.text
.globl _imp
_imp:
  .long 0

.section __TEXT,__objc_methname,cstring_literals
Lrequired_instance_1:
  .asciz "required_instance_1"
Lrequired_instance_2:
  .asciz "required_instance_2"
Lrequired_class_1:
  .asciz "required_class_1"
Lrequired_class_2:
  .asciz "required_class_2"
Loptional_instance_1:
  .asciz "optional_instance_1"
Loptional_instance_2:
  .asciz "optional_instance_2"
Loptional_class_1:
  .asciz "optional_class_1"
Loptional_class_2:
  .asciz "optional_class_2"

.section __TEXT,__objc_methtype,cstring_literals
Ltypes:
  .asciz "v16@0:8"
.section __TEXT,__objc_classname,cstring_literals
LClangName:
  .asciz "ClangProtocol"
LSwiftName:
  .asciz "SwiftProtocol"
LOpaqueName:
  .asciz "OpaqueProtocol"

.macro method_list name, selector, implementation=0
.section __DATA,__objc_data
.p2align 3
.globl \name
.private_extern \name
\name:
  .long 24
  .long 2
  .quad L\selector\()_1
  .quad Ltypes
  .quad 0
  .quad L\selector\()_2
  .quad Ltypes
  .quad \implementation
.endm

method_list __OBJC_$_PROTOCOL_INSTANCE_METHODS_Clang, required_instance, _imp
method_list __OBJC_$_PROTOCOL_CLASS_METHODS_Clang, required_class
method_list __OBJC_$_PROTOCOL_INSTANCE_METHODS_OPT_Clang, optional_instance
method_list __OBJC_$_PROTOCOL_CLASS_METHODS_OPT_Clang, optional_class
method_list __PROTOCOL_INSTANCE_METHODS_Swift, required_instance
method_list __PROTOCOL_CLASS_METHODS_Swift, required_class
method_list __PROTOCOL_INSTANCE_METHODS_OPT_Swift, optional_instance
method_list __PROTOCOL_CLASS_METHODS_OPT_Swift, optional_class
method_list _opaque_methods, required_instance

## Keep extended type encodings in the same order as the protocol methods.
.section __DATA,__objc_const
.p2align 3
LextendedTypes:
.rept 8
  .quad Ltypes
.endr

.macro protocol name, string, instance, class, optional_instance, optional_class, extended=0
.section __DATA,__objc_const
.p2align 3
.globl \name
.private_extern \name
\name:
  .quad 0
  .quad \string
  .quad 0
  .quad \instance
  .quad \class
  .quad \optional_instance
  .quad \optional_class
  .quad 0
  .long 96
  .long 0
  .quad \extended
  .quad 0
  .quad 0
.endm

protocol _ClangProtocol, LClangName, __OBJC_$_PROTOCOL_INSTANCE_METHODS_Clang, __OBJC_$_PROTOCOL_CLASS_METHODS_Clang, __OBJC_$_PROTOCOL_INSTANCE_METHODS_OPT_Clang, __OBJC_$_PROTOCOL_CLASS_METHODS_OPT_Clang, LextendedTypes
protocol _SwiftProtocol, LSwiftName, __PROTOCOL_INSTANCE_METHODS_Swift, __PROTOCOL_CLASS_METHODS_Swift, __PROTOCOL_INSTANCE_METHODS_OPT_Swift, __PROTOCOL_CLASS_METHODS_OPT_Swift, LextendedTypes
protocol _OpaqueProtocol, LOpaqueName, _opaque_methods, 0, 0, 0

.section __DATA,__objc_protolist,regular,no_dead_strip
.p2align 3
  .quad _ClangProtocol
  .quad _SwiftProtocol
  .quad _OpaqueProtocol
.section __DATA,__objc_imageinfo,regular,no_dead_strip
  .long 0
  .long 0

## Attach the protocols to a class so llvm-objdump decodes their method lists.
.section __TEXT,__objc_classname,cstring_literals
LclassName:
  .asciz "ProtocolClass"
.section __DATA,__objc_const
.p2align 3
Lprotocols:
  .quad 3
  .quad _ClangProtocol
  .quad _SwiftProtocol
  .quad _OpaqueProtocol
LclassRO:
  .long 2
  .long 0
  .long 8
  .long 0
  .quad 0
  .quad LclassName
  .quad 0
  .quad Lprotocols
  .quad 0
  .quad 0
  .quad 0
.section __DATA,__objc_data
.p2align 3
.globl _OBJC_CLASS_$_ProtocolClass
_OBJC_CLASS_$_ProtocolClass:
  .quad 0
  .quad 0
  .quad 0
  .quad 0
  .quad LclassRO
.section __DATA,__objc_classlist,regular,no_dead_strip
.p2align 3
  .quad _OBJC_CLASS_$_ProtocolClass

## llvm-objdump only decodes required methods when printing a protocol. These
## class records point to the optional lists so the class reader can decode
## their entries, including the null IMPs. Check that their addresses match the
## optional-list pointers printed in the protocols above.
.macro decode_optional name, methods
.section __TEXT,__objc_classname,cstring_literals
LoptionalName\@:
  .asciz "\name"
.section __DATA,__objc_const
.p2align 3
LoptionalRO\@:
  .long 2
  .long 0
  .long 8
  .long 0
  .quad 0
  .quad LoptionalName\@
  .quad \methods
  .quad 0
  .quad 0
  .quad 0
  .quad 0
.section __DATA,__objc_data
.p2align 3
.globl _OBJC_CLASS_$_\name
_OBJC_CLASS_$_\name:
  .quad 0
  .quad 0
  .quad 0
  .quad 0
  .quad LoptionalRO\@
.section __DATA,__objc_classlist,regular,no_dead_strip
.p2align 3
  .quad _OBJC_CLASS_$_\name
.endm

decode_optional ClangOptionalInstance, __OBJC_$_PROTOCOL_INSTANCE_METHODS_OPT_Clang
decode_optional ClangOptionalClass, __OBJC_$_PROTOCOL_CLASS_METHODS_OPT_Clang
decode_optional SwiftOptionalInstance, __PROTOCOL_INSTANCE_METHODS_OPT_Swift
decode_optional SwiftOptionalClass, __PROTOCOL_CLASS_METHODS_OPT_Swift

.subsections_via_symbols
