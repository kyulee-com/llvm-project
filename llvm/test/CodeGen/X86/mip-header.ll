; RUN: llc < %s -enable-machine-instrumentation -enable-machine-function-coverage -mtriple=x86_64-linux | FileCheck %s --check-prefix ELF
; RUN: llc < %s -enable-machine-instrumentation -enable-machine-function-coverage -mtriple=x86_64-apple-macosx | FileCheck %s --check-prefix MACHO

define i32 @_Z3fooii(i32 %a, i32 %b) #0 {
  ret i32 0
}

;================================= .mipraw Header =====================================;
; ELF-LABEL:   .section  __llvm_mipraw,"aGw",@progbits,Header,comdat
; ELF:         .p2align  3
; ELF:       [[RAW_REF:.*]]:
; ELF-NEXT:    .long    0x50494dfb                      # Magic
; ELF-NEXT:    .short   8                               # Version
; ELF-NEXT:    .short   0x1                             # File Type
; ELF-NEXT:    .long    0x1                             # Profile Type
; ELF-NEXT:    .long    [[MODULE_HASH:.*]]              # Module Hash
; ELF-NEXT:    .zero    4
; ELF-NEXT:    .long    0x18                            # Offset To Data

;================================= .mipmap Header =====================================;
; ELF-LABEL:   .section  __llvm_mipmap,"GwR",@note,Header,comdat
; ELF:         .p2align  3
; ELF:       [[MAP_REF:.*]]:
; ELF-NEXT:    .long    0x50494dfb                      # Magic
; ELF-NEXT:    .short   8                               # Version
; ELF-NEXT:    .short   0x2                             # File Type
; ELF-NEXT:    .long    0x1                             # Profile Type
; ELF-NEXT:    .long    [[MODULE_HASH]]                 # Module Hash
; ELF-NEXT:    .zero    4
; ELF-NEXT:    .long    0x18                            # Offset To Data

;================================= .mipraw Header =====================================;
; MACHO:       .section __DATA,__llvm_mipraw,regular,no_dead_strip
; MACHO:       .globl   __header$__llvm_mipraw
; MACHO:       .weak_definition  __header$__llvm_mipraw
; MACHO-LABEL: __header$__llvm_mipraw:
; MACHO:       .p2align 3
; MACHO:       .long   0x50494dfb              ## Magic
; MACHO-NEXT:  .short  8                       ## Version
; MACHO-NEXT:  .short  0x1                     ## File Type
; MACHO-NEXT:  .long   0x1                     ## Profile Type
; MACHO-NEXT:  .long   [[MODULE_HASH:.*]]      ## Module Hash
; MACHO-NEXT:  .space  4
; MACHO-NEXT:  .long   0x18                    ## Offset To Data


;================================= .mipmap Header =====================================;
; MACHO:       .section __DATA,__llvm_mipmap,regular,no_dead_strip
; MACHO:       .globl   __header$__llvm_mipmap
; MACHO:       .weak_definition  __header$__llvm_mipmap
; MACHO-LABEL: __header$__llvm_mipmap:
; MACHO:       .p2align 3
; MACHO:       .long   0x50494dfb              ## Magic
; MACHO-NEXT:  .short  8                       ## Version
; MACHO-NEXT:  .short  0x2                     ## File Type
; MACHO-NEXT:  .long   0x1                     ## Profile Type
; MACHO-NEXT:  .long   [[MODULE_HASH]]         ## Module Hash
; MACHO-NEXT:  .space  4
; MACHO-NEXT:  .long   0x18                    ## Offset To Data
