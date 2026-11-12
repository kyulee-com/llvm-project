; RUN: llc -mtriple=arm64e-apple-macosx -mattr=+pauth -enable-machine-outliner -outliner-leaf-descendants=false -verify-machineinstrs %s -o - | FileCheck %s --check-prefix=TRAPS
; RUN: sed 's/"ptrauth-auth-traps"//' %s | llc -mtriple=arm64e-apple-macosx -mattr=+pauth -enable-machine-outliner -outliner-leaf-descendants=false -verify-machineinstrs -o - | FileCheck %s --check-prefix=NO-CHECK

; Outlined functions do not have the callee-saved info used by AsmPrinter to
; decide whether a NonLeaf tail call needs an authenticated-LR check. Do not
; outline a signed thunk requiring that check until the info is preserved.
; Returns through RETAB are fine; the unsafe thunk would use AUTIBSP then B.
; TRAPS-NOT: autibsp
; NO-CHECK: autibsp
; NO-CHECK-NEXT: b _g0

define i64 @step(i64 %x) #0 {
  %r = add i64 %x, 1
  ret i64 %r
}

declare i64 @g0(i64)

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
  %p5 = getelementptr i64, ptr %p, i64 5
  store volatile i64 %x, ptr %p5
  %p6 = getelementptr i64, ptr %p, i64 6
  store volatile i64 %x, ptr %p6
  %p7 = getelementptr i64, ptr %p, i64 7
  store volatile i64 %x, ptr %p7
  %p8 = getelementptr i64, ptr %p, i64 8
  store volatile i64 %x, ptr %p8
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
  %p5 = getelementptr i64, ptr %p, i64 5
  store volatile i64 %x, ptr %p5
  %p6 = getelementptr i64, ptr %p, i64 6
  store volatile i64 %x, ptr %p6
  %p7 = getelementptr i64, ptr %p, i64 7
  store volatile i64 %x, ptr %p7
  %p8 = getelementptr i64, ptr %p, i64 8
  store volatile i64 %x, ptr %p8
  %y = call i64 @step(i64 %x)
  store volatile i64 %y, ptr %p
  %z = call i64 @g0(i64 %y)
  %r = add i64 %z, 16
  ret i64 %r
}

attributes #0 = { minsize noinline noredzone nounwind uwtable "ptrauth-returns" "ptrauth-auth-traps" }
