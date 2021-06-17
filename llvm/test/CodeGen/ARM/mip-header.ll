; RUN: llc < %s -enable-machine-instrumentation -enable-machine-function-coverage -mtriple=armv7-linux | FileCheck %s

define i32 @_Z3fooii(i32 %a, i32 %b) #0 {
  ret i32 0
}

;================================= .mipraw Header =====================================;
; CHECK:       .section __llvm_mipraw,"aGw",%progbits,Header,comdat
; CHECK:       .p2align 3
; CHECK:      [[RAW_REF:.*]]:
; CHECK:       .long   0x50494dfb                      @ Magic
; CHECK-NEXT:  .short  8                               @ Version
; CHECK-NEXT:  .short  0x1                             @ File Type
; CHECK-NEXT:  .long   0x1                             @ Profile Type
; CHECK-NEXT:  .long   [[MODULE_HASH:.*]]              @ Module Hash
; CHECK-NEXT:  .zero   4
; CHECK-NEXT:  .long   0x18                            @ Offset To Data

;================================= .mipmap Header =====================================;
; CHECK:       .section __llvm_mipmap,"GwR",%note,Header,comdat
; CHECK:       .p2align 3
; CHECK:      [[RAW_REF:.*]]:
; CHECK:       .long   0x50494dfb                      @ Magic
; CHECK-NEXT:  .short  8                               @ Version
; CHECK-NEXT:  .short  0x2                             @ File Type
; CHECK-NEXT:  .long   0x1                             @ Profile Type
; CHECK-NEXT:  .long   [[MODULE_HASH:.*]]              @ Module Hash
; CHECK-NEXT:  .zero   4
; CHECK-NEXT:  .long   0x18                            @ Offset To Data
