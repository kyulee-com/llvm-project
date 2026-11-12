; RUN: llc -mtriple=arm64e-apple-macosx -mattr=+pauth -enable-machine-outliner -verify-machineinstrs %s -o - | FileCheck %s

; The repeated body contains a real non-tail call. Its outlined function must
; sign LR, but PACIBSP + RETAB only adds four bytes to the saved-LR frame.
; CHECK-LABEL: _f0:
; CHECK: bl [[OUTLINE:_OUTLINED_FUNCTION_[0-9]+]]
; CHECK-LABEL: _f1:
; CHECK: bl [[OUTLINE]]
; CHECK: [[OUTLINE]]:
; CHECK: pacibsp
; CHECK: bl _step
; CHECK: retab

define i64 @step(i64 %x) #0 {
  %r = add i64 %x, 1
  ret i64 %r
}

declare i64 @g0(i64)
declare i64 @g1(i64)

define i64 @f0(i64 %x, ptr %p) #0 {
  store volatile i64 %x, ptr %p
  %p1 = getelementptr i64, ptr %p, i64 1
  store volatile i64 %x, ptr %p1
  %p2 = getelementptr i64, ptr %p, i64 2
  store volatile i64 %x, ptr %p2
  %p3 = getelementptr i64, ptr %p, i64 3
  store volatile i64 %x, ptr %p3
  %p4 = getelementptr i64, ptr %p, i64 4
  store volatile i64 %x, ptr %p4
  %y = call i64 @step(i64 %x)
  store volatile i64 %y, ptr %p
  %z = call i64 @g0(i64 %y)
  %r = add i64 %z, 8
  ret i64 %r
}

define i64 @f1(i64 %x, ptr %p) #0 {
  store volatile i64 %x, ptr %p
  %p1 = getelementptr i64, ptr %p, i64 1
  store volatile i64 %x, ptr %p1
  %p2 = getelementptr i64, ptr %p, i64 2
  store volatile i64 %x, ptr %p2
  %p3 = getelementptr i64, ptr %p, i64 3
  store volatile i64 %x, ptr %p3
  %p4 = getelementptr i64, ptr %p, i64 4
  store volatile i64 %x, ptr %p4
  %y = call i64 @step(i64 %x)
  store volatile i64 %y, ptr %p
  %z = call i64 @g1(i64 %y)
  %r = add i64 %z, 16
  ret i64 %r
}

attributes #0 = { minsize noinline noredzone nounwind uwtable "ptrauth-returns" "ptrauth-auth-traps" }
