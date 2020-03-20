; RUN: llc < %s -mtriple=arm64-apple-ios7.0 -homogeneous-prolog-epilog| FileCheck %s
; RUN: llc < %s -mtriple=aarch64-unknown-linux-gnu  -homogeneous-prolog-epilog | FileCheck %s --check-prefixes=CHECK-SAVELR

; CHECK-LABEL: __Z3hooii:
; CHECK:      stp     x29, x30, [sp, #-16]!
; CHECK-NEXT: bl      _OUTLINED_FUNCTION_PROLOG_x19x20x21x22
; CHECK:      bl      __Z3gooi
; CHECK:      bl      __Z3gooi
; CHECK:      bl      _OUTLINED_FUNCTION_EPILOG_x30x29x19x20x21x22
; CHECK-NEXT: b __Z3gooi

; CHECK-SAVELR-LABEL: _Z3hooii:
; CHECK-SAVELR:      stp     x29, x30, [sp, #-16]!
; CHECK-SAVELR-NEXT: bl      OUTLINED_FUNCTION_PROLOG_x19x20x21x22x30x29
; CHECK-SAVELR:      bl      _Z3gooi
; CHECK-SAVELR:      bl      _Z3gooi
; CHECK-SAVELR:      bl      OUTLINED_FUNCTION_EPILOG_x19x20x21x22x30x29
; CHECK-SAVELR-NEXT: b _Z3gooi

define i32 @_Z3hooii(i32 %b, i32 %a) nounwind ssp optsize {
  %1 = tail call i32 @_Z3gooi(i32 %b)
  %2 = tail call i32 @_Z3gooi(i32 %a)
  %3 = add i32 %a, %b
  %4 = add i32 %3, %1
  %5 = add i32 %4, %2
  %6 = tail call i32 @_Z3gooi(i32 %5)
  ret i32 %6
}

declare i32 @_Z3gooi(i32);


; CHECK-LABEL: _OUTLINED_FUNCTION_PROLOG_x19x20x21x22:
; CHECK:      stp     x20, x19, [sp, #-16]!
; CHECK-NEXT: stp     x22, x21, [sp, #-16]!
; CHECK-NEXT: ret

; CHECK-LABEL: _OUTLINED_FUNCTION_EPILOG_x30x29x19x20x21x22:
; CHECK:      mov     x16, x30
; CHECK-NEXT: ldp     x22, x21, [sp], #16
; CHECK-NEXT: ldp     x20, x19, [sp], #16
; CHECK-NEXT: ldp     x29, x30, [sp], #16
; CHECK-NEXT: br      x16

; CHECK-SAVELR-LABEL: OUTLINED_FUNCTION_PROLOG_x19x20x21x22x30x29:
; CHECK-SAVELR:      mov     x16, x30
; CHECK-SAVELR-NEXT: ldp     x29, x30, [sp], #16
; CHECK-SAVELR-NEXT: stp     x20, x19, [sp, #-16]!
; CHECK-SAVELR-NEXT: stp     x22, x21, [sp, #-16]!
; CHECK-SAVELR-NEXT: stp     x29, x30, [sp, #-16]!
; CHECK-SAVELR-NEXT: br      x16

; CHECK-SAVELR-LABEL: OUTLINED_FUNCTION_EPILOG_x19x20x21x22x30x29:
; CHECK-SAVELR:      mov     x16, x30
; CHECK-SAVELR-NEXT: ldp     x29, x30, [sp], #16
; CHECK-SAVELR-NEXT: ldp     x22, x21, [sp], #16
; CHECK-SAVELR-NEXT: ldp     x20, x19, [sp], #16
; CHECK-SAVELR-NEXT: br      x16
