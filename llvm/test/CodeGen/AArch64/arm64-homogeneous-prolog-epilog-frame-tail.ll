; RUN: llc < %s -mtriple=arm64-apple-ios7.0  -homogeneous-prolog-epilog | FileCheck %s
; RUN: llc < %s -mtriple=aarch64-unknown-linux-gnu  -homogeneous-prolog-epilog | FileCheck %s --check-prefixes=CHECK-SAVELR

; CHECK-LABEL: __Z3foofffi:
; CHECK:      stp     x29, x30, [sp, #-16]!
; CHECK-NEXT: bl      _OUTLINED_FUNCTION_PROLOG_FRAME48_x19x20d8d9d10d11
; CHECK:      bl      __Z3goof
; CHECK:      bl      __Z3goof
; CHECK:      b       _OUTLINED_FUNCTION_EPILOG_TAIL_x30x29x19x20d8d9d10d11

; CHECK-SAVELR-LABEL: _Z3foofffi:
; CHECK-SAVELR:      stp     x29, x30, [sp, #-16]!
; CHECK-SAVELR-NEXT: bl      OUTLINED_FUNCTION_PROLOG_FRAME0_x19x20x30x29d8d9d10d11
; CHECK-SAVELR:      bl      _Z3goof
; CHECK-SAVELR:      bl      _Z3goof
; CHECK-SAVELR:      b       OUTLINED_FUNCTION_EPILOG_TAIL_x19x20x30x29d8d9d10d11

define float @_Z3foofffi(float %b, float %x, float %y, i32 %z) ssp optsize "frame-pointer"="non-leaf" {
entry:
  %inc = fadd float %b, 1.000000e+00
  %add = fadd float %inc, %x
  %add1 = fadd float %add, %y
  %conv = sitofp i32 %z to float
  %sub = fsub float %add1, %conv
  %dec = add nsw i32 %z, -1
  %call = tail call float @_Z3goof(float %inc) #2
  %call2 = tail call float @_Z3goof(float %sub) #2
  %add3 = fadd float %call, %call2
  %mul = fmul float %inc, %add3
  %add4 = fadd float %sub, %mul
  %conv5 = sitofp i32 %dec to float
  %sub6 = fsub float %add4, %conv5
  ret float %sub6
}

; CHECK-LABEL: _Z3zoov:
; CHECK:      stp     x29, x30, [sp, #-16]!
; CHECK:      bl      __Z3hoo
; CHECK:      b       _OUTLINED_FUNCTION_EPILOG_TAIL_x30x29

define i32 @_Z3zoov() nounwind ssp optsize {
  %1 = tail call i32 @_Z3hoov() #2
  %2 = add nsw i32 %1, 1
  ret i32 %2
}


declare float @_Z3goof(float) nounwind ssp optsize
declare i32 @_Z3hoov() nounwind ssp optsize

; CHECK-LABEL:  _OUTLINED_FUNCTION_PROLOG_FRAME48_x19x20d8d9d10d11:
; CHECK:      stp     x20, x19, [sp, #-16]!
; CHECK-NEXT: stp     d9, d8, [sp, #-16]!
; CHECK-NEXT: stp     d11, d10, [sp, #-16]!
; CHECK-NEXT: add     x29, sp, #48
; CHECK-NEXT: ret

; CHECK-LABEL: _OUTLINED_FUNCTION_EPILOG_TAIL_x30x29x19x20d8d9d10d11:
; CHECK:      ldp     d11, d10, [sp], #16
; CHECK-NEXT: ldp     d9, d8, [sp], #16
; CHECK-NEXT: ldp     x20, x19, [sp], #16
; CHECK-NEXT: ldp     x29, x30, [sp], #16
; CHECK-NEXT: ret

; CHECK-LABEL: _OUTLINED_FUNCTION_EPILOG_TAIL_x30x29:
; CHECK:      ldp     x29, x30, [sp], #16
; CHECK-NEXT: ret

; CHECK-SAVELR-LABEL:  OUTLINED_FUNCTION_PROLOG_FRAME0_x19x20x30x29d8d9d10d11:
; CHECK-SAVELR:      mov     x16, x30
; CHECK-SAVELR-NEXT: ldp     x29, x30, [sp], #16
; CHECK-SAVELR-NEXT: stp     x20, x19, [sp, #-16]!
; CHECK-SAVELR-NEXT: stp     x29, x30, [sp, #-16]!
; CHECK-SAVELR-NEXT: stp     d9, d8, [sp, #-16]!
; CHECK-SAVELR-NEXT: stp     d11, d10, [sp, #-16]!
; CHECK-SAVELR-NEXT: mov     x29, sp
; CHECK-SAVELR-NEXT: br      x16

; CHECK-SAVELR-LABEL: OUTLINED_FUNCTION_EPILOG_TAIL_x19x20x30x29d8d9d10d11:
; CHECK-SAVELR:      ldp     d11, d10, [sp], #16
; CHECK-SAVELR-NEXT: ldp     d9, d8, [sp], #16
; CHECK-SAVELR-NEXT: ldp     x29, x30, [sp], #16
; CHECK-SAVELR-NEXT: ldp     x20, x19, [sp], #16
; CHECK-SAVELR-NEXT: ret

; CHECK-SAVELR-LABEL: OUTLINED_FUNCTION_EPILOG_TAIL_x30x29:
; CHECK-SAVELR:      ldp     x29, x30, [sp], #16
; CHECK-SAVELR-NEXT: ret
