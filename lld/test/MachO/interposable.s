# REQUIRES: x86

# RUN: rm -rf %t; split-file %s %t
# RUN: llvm-mc -filetype=obj -triple=x86_64-apple-darwin %t/2.s -o %t/2.o
# RUN: llvm-mc -filetype=obj -triple=x86_64-apple-darwin %t/3.s -o %t/3.o
# RUN: llvm-mc -filetype=obj -triple=x86_64-apple-darwin %t/main.s -o %t/main.o

# RUN: %lld -arch x86_64 -interposable -lSystem -o %t/main %t/main.o %t/2.o %t/3.o
# RUN: llvm-objdump --macho -d %t/main | FileCheck %s --check-prefix BUNDLE-OBJ
BUNDLE-OBJ-LABEL: _my_user:
BUNDLE-OBJ-NEXT:          callq   [[#%#x,]] ## symbol stub for: _my_friend

## Like ld64, bind interposable definitions to this image under the default
## two-level namespace, and look them up in the flat namespace with
## -flat_namespace. Weak definitions, dynamic lookups and dylib imports keep
## their own lookups, and private externs are not interposable.
# RUN: llvm-mc -filetype=obj -triple=x86_64-apple-darwin %t/lib.s -o %t/lib.o
# RUN: %lld -dylib -interposable -U _dynamic -o %t/lib.dylib %t/lib.o
# RUN: llvm-objdump --macho --bind --lazy-bind --weak-bind %t/lib.dylib | \
# RUN:   FileCheck %s --check-prefix=BIND -DSCOPE=this-image \
# RUN:   -DDYLIB=libSystem --implicit-check-not=_hidden
# RUN: %lld -dylib -interposable -flat_namespace -U _dynamic \
# RUN:   -o %t/flat.dylib %t/lib.o
# RUN: llvm-objdump --macho --bind --lazy-bind --weak-bind %t/flat.dylib | \
# RUN:   FileCheck %s --check-prefix=BIND -DSCOPE=flat-namespace \
# RUN:   -DDYLIB=flat-namespace --implicit-check-not=_hidden
# RUN: %lld -dylib -interposable -fixup_chains -U _dynamic \
# RUN:   -o %t/chained.dylib %t/lib.o
# RUN: llvm-objdump --macho --dyld-info %t/chained.dylib | \
# RUN:   FileCheck %s --check-prefix=CHAINED -DSCOPE=this-image \
# RUN:   --implicit-check-not=_hidden
# RUN: %lld -dylib -interposable -flat_namespace -fixup_chains -U _dynamic \
# RUN:   -o %t/flat-chained.dylib %t/lib.o
# RUN: llvm-objdump --macho --dyld-info %t/flat-chained.dylib | \
# RUN:   FileCheck %s --check-prefix=CHAINED -DSCOPE=flat-namespace \
# RUN:   --implicit-check-not=_hidden

## The bind of _interposable comes first, so it must set the ordinal even when
## that ordinal is this-image (0).
# BIND-LABEL: Bind table:
# BIND-NEXT:  segment      section address type    addend dylib     symbol
# BIND-NEXT:  __DATA_CONST __got   {{.*}}  pointer      0 [[SCOPE]] _interposable
# BIND-NEXT:  __DATA_CONST __got   {{.*}}  pointer      0 [[DYLIB]] dyld_stub_binder
# BIND-EMPTY:
# BIND-NEXT:  Lazy bind table:
# BIND-NEXT:  segment  section         address dylib          symbol
# BIND-DAG:   __DATA   __la_symbol_ptr {{.*}}  flat-namespace _dynamic
# BIND-DAG:   __DATA   __la_symbol_ptr {{.*}}  [[SCOPE]]      _interposable
# BIND-LABEL: Weak bind table:
# BIND-NEXT:  segment  section         address type    addend symbol
# BIND-NEXT:  __DATA   __la_symbol_ptr {{.*}}  pointer      0 _weak

# CHAINED-LABEL: dyld information:
# CHAINED-DAG:   __DATA_CONST __got {{.*}} bind 0x0 flat-namespace _dynamic
# CHAINED-DAG:   __DATA_CONST __got {{.*}} bind 0x0 weak           _weak
# CHAINED-DAG:   __DATA_CONST __got {{.*}} bind 0x0 [[SCOPE]]      _interposable

#--- 2.s
# my_lib: This contains the exported function
.globl _my_friend
_my_friend:
  retq

#--- 3.s
# _my_user.s: This is the user/caller of the
#             exported function
.text
_my_user:
  callq _my_friend()
  retq

#--- main.s
# main.s: dummy exec/main loads the exported function.
# This is basically a way to say `my_user` should get
# `my_func` from this executable.
.globl _main
.text
 _main:
  retq

#--- lib.s
.globl _interposable, _weak, _hidden
.weak_definition _weak
.private_extern _hidden

_interposable:
  retq
_weak:
  retq
_hidden:
  retq

_caller:
  movq _interposable@GOTPCREL(%rip), %rax
  callq _interposable
  callq _weak
  callq _hidden
  callq _dynamic
  retq
