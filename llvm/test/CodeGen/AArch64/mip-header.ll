; RUN: llc < %s -enable-machine-instrumentation -enable-machine-call-graph -mtriple=arm64-linux | FileCheck %s --check-prefix ELF
; RUN: llc < %s -enable-machine-instrumentation -enable-machine-call-graph -mtriple=arm64-apple-ios | FileCheck %s --check-prefix MACHO

define i32 @_Z3fooii(i32 %a, i32 %b) #0 {
  ret i32 0
}

;================================= .mipraw Header =====================================;
; ELF:       .section __llvm_mipraw,"aGw",@progbits,Header,comdat
; ELF:       .p2align 3
; ELF:      [[RAW_REF:.*]]:
; ELF:       .word   0x50494dfb                      // Magic
; ELF-NEXT:  .hword  8                               // Version
; ELF-NEXT:  .hword  0x1                             // File Type
; ELF-NEXT:  .word   0xc                             // Profile Type
; ELF-NEXT:  .word   [[MODULE_HASH:.*]]              // Module Hash
; ELF-NEXT:  .zero   4
; ELF-NEXT:  .word   0x18                            // Offset To Data

;================================= .mipmap Header =====================================;
; ELF:       .section __llvm_mipmap,"GwR",@note,Header,comdat
; ELF:       .p2align 3
; ELF:       [[MAP_REF:.*]]:
; ELF:       .word   0x50494dfb                      // Magic
; ELF-NEXT:  .hword  8                               // Version
; ELF-NEXT:  .hword  0x2                             // File Type
; ELF-NEXT:  .word   0xc                             // Profile Type
; ELF-NEXT:  .word   [[MODULE_HASH]]                 // Module Hash
; ELF-NEXT:  .zero   4
; ELF-NEXT:  .word   0x18                            // Offset To Data

;================================= .mipraw Header =====================================;
; MACHO:       .section __DATA,__llvm_mipraw,regular,no_dead_strip
; MACHO:       .weak_definition __header$__llvm_mipraw
; MACHO-LABEL: __header$__llvm_mipraw:
; MACHO:       .p2align 3
; MACHO:       [[RAW_REF:.*]]:
; MACHO:       .long     0x50494dfb              ; Magic
; MACHO-NEXT:  .short    8                       ; Version
; MACHO-NEXT:  .short    0x1                     ; File Type
; MACHO-NEXT:  .long     0xc                     ; Profile Type
; MACHO-NEXT:  .long     [[MODULE_HASH:.*]]      ; Module Hash
; MACHO-NEXT:  .space    4
; MACHO-NEXT:  .long     0x18                    ; Offset To Data

;================================= .mipmap Header =====================================;
; MACHO:       .section __DATA,__llvm_mipmap,regular,no_dead_strip
; MACHO:       .weak_definition __header$__llvm_mipmap
; MACHO-LABEL: __header$__llvm_mipmap:
; MACHO:       .p2align 3
; MACHO:       [[MAP_REF:.*]]:
; MACHO:       .long     0x50494dfb              ; Magic
; MACHO-NEXT:  .short    8                       ; Version
; MACHO-NEXT:  .short    0x2                     ; File Type
; MACHO-NEXT:  .long     0xc                     ; Profile Type
; MACHO-NEXT:  .long     [[MODULE_HASH]]         ; Module Hash
; MACHO-NEXT:  .space    4
; MACHO-NEXT:  .long     0x18                    ; Offset To Data
