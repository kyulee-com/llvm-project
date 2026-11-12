; RUN: llc -mtriple=arm64e-apple-darwin -enable-machine-outliner \
; RUN:   -verify-machineinstrs %s -o - | FileCheck %s
; RUN: llc -mtriple=arm64e-apple-darwin -enable-machine-outliner \
; RUN:   -global-isel -global-isel-abort=1 -verify-machineinstrs %s -o - \
; RUN:   | FileCheck %s

; Check that a sequence ending in an authenticated indirect call (BLRA) is
; outlined as a thunk, and that the call becomes an authenticated tail call
; that keeps the key and the integer and address discriminators.

; Zero discriminator, IA key.
define i32 @ia_zero_a(ptr %fp) #0 {
; CHECK-LABEL: _ia_zero_a:
; CHECK:         bl _OUTLINED_FUNCTION_3
; CHECK-NEXT:    add w0, w0, #8
  %r = tail call i32 %fp(i32 1, i32 2, i32 3, i32 4) [ "ptrauth"(i32 0, i64 0) ]
  %x = add i32 %r, 8
  ret i32 %x
}

define i32 @ia_zero_b(ptr %fp) #0 {
; CHECK-LABEL: _ia_zero_b:
; CHECK:         bl _OUTLINED_FUNCTION_3
; CHECK-NEXT:    add w0, w0, #88
  %r = tail call i32 %fp(i32 1, i32 2, i32 3, i32 4) [ "ptrauth"(i32 0, i64 0) ]
  %x = add i32 %r, 88
  ret i32 %x
}

; Small constant discriminator, IB key.
define i32 @ib_const_a(ptr %fp) #0 {
; CHECK-LABEL: _ib_const_a:
; CHECK:         bl _OUTLINED_FUNCTION_2
; CHECK-NEXT:    add w0, w0, #8
  %r = tail call i32 %fp(i32 5, i32 6, i32 7, i32 8) [ "ptrauth"(i32 1, i64 1234) ]
  %x = add i32 %r, 8
  ret i32 %x
}

define i32 @ib_const_b(ptr %fp) #0 {
; CHECK-LABEL: _ib_const_b:
; CHECK:         bl _OUTLINED_FUNCTION_2
; CHECK-NEXT:    add w0, w0, #88
  %r = tail call i32 %fp(i32 5, i32 6, i32 7, i32 8) [ "ptrauth"(i32 1, i64 1234) ]
  %x = add i32 %r, 88
  ret i32 %x
}

; Address-diversified (blended) discriminator, IA key.
define i32 @ia_blend_a(ptr %slot) #0 {
; CHECK-LABEL: _ia_blend_a:
; CHECK:         bl _OUTLINED_FUNCTION_0
; CHECK-NEXT:    add w0, w0, #8
  %fp = load ptr, ptr %slot
  %a = ptrtoint ptr %slot to i64
  %d = tail call i64 @llvm.ptrauth.blend(i64 %a, i64 42)
  %r = tail call i32 %fp(i32 9, i32 10, i32 11, i32 12) [ "ptrauth"(i32 0, i64 %d) ]
  %x = add i32 %r, 8
  ret i32 %x
}

define i32 @ia_blend_b(ptr %slot) #0 {
; CHECK-LABEL: _ia_blend_b:
; CHECK:         bl _OUTLINED_FUNCTION_0
; CHECK-NEXT:    add w0, w0, #88
  %fp = load ptr, ptr %slot
  %a = ptrtoint ptr %slot to i64
  %d = tail call i64 @llvm.ptrauth.blend(i64 %a, i64 42)
  %r = tail call i32 %fp(i32 9, i32 10, i32 11, i32 12) [ "ptrauth"(i32 0, i64 %d) ]
  %x = add i32 %r, 88
  ret i32 %x
}

; Address-diversified (blended) discriminator, IB key.
define i32 @ib_blend_a(ptr %slot) #0 {
; CHECK-LABEL: _ib_blend_a:
; CHECK:         bl _OUTLINED_FUNCTION_1
; CHECK-NEXT:    add w0, w0, #8
  %fp = load ptr, ptr %slot
  %a = ptrtoint ptr %slot to i64
  %d = tail call i64 @llvm.ptrauth.blend(i64 %a, i64 4242)
  %r = tail call i32 %fp(i32 13, i32 14, i32 15, i32 16) [ "ptrauth"(i32 1, i64 %d) ]
  %x = add i32 %r, 8
  ret i32 %x
}

define i32 @ib_blend_b(ptr %slot) #0 {
; CHECK-LABEL: _ib_blend_b:
; CHECK:         bl _OUTLINED_FUNCTION_1
; CHECK-NEXT:    add w0, w0, #88
  %fp = load ptr, ptr %slot
  %a = ptrtoint ptr %slot to i64
  %d = tail call i64 @llvm.ptrauth.blend(i64 %a, i64 4242)
  %r = tail call i32 %fp(i32 13, i32 14, i32 15, i32 16) [ "ptrauth"(i32 1, i64 %d) ]
  %x = add i32 %r, 88
  ret i32 %x
}

; CHECK-LABEL: _OUTLINED_FUNCTION_0:
; CHECK-SAME:    Thunk
; CHECK:         mov x8, x0
; CHECK-NEXT:    ldr x9, [x0]
; CHECK-NEXT:    mov w0, #9
; CHECK-NEXT:    mov w1, #10
; CHECK-NEXT:    mov w2, #11
; CHECK-NEXT:    mov w3, #12
; CHECK-NEXT:    mov x16, x8
; CHECK-NEXT:    movk x16, #42, lsl #48
; CHECK-NEXT:    braa x9, x16

; CHECK-LABEL: _OUTLINED_FUNCTION_1:
; CHECK-SAME:    Thunk
; CHECK:         mov x8, x0
; CHECK-NEXT:    ldr x9, [x0]
; CHECK-NEXT:    mov w0, #13
; CHECK-NEXT:    mov w1, #14
; CHECK-NEXT:    mov w2, #15
; CHECK-NEXT:    mov w3, #16
; CHECK-NEXT:    mov x16, x8
; CHECK-NEXT:    movk x16, #4242, lsl #48
; CHECK-NEXT:    brab x9, x16

; CHECK-LABEL: _OUTLINED_FUNCTION_2:
; CHECK-SAME:    Thunk
; CHECK:         mov x8, x0
; CHECK-NEXT:    mov w0, #5
; CHECK-NEXT:    mov w1, #6
; CHECK-NEXT:    mov w2, #7
; CHECK-NEXT:    mov w3, #8
; CHECK-NEXT:    mov x16, #1234
; CHECK-NEXT:    brab x8, x16

; CHECK-LABEL: _OUTLINED_FUNCTION_3:
; CHECK-SAME:    Thunk
; CHECK:         mov x8, x0
; CHECK-NEXT:    mov w0, #1
; CHECK-NEXT:    mov w1, #2
; CHECK-NEXT:    mov w2, #3
; CHECK-NEXT:    mov w3, #4
; CHECK-NEXT:    braaz x8

declare i64 @llvm.ptrauth.blend(i64, i64)

attributes #0 = { minsize "ptrauth-calls" }
