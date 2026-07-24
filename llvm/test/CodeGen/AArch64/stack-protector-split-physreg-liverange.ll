; RUN: llc -mtriple=arm64-apple-ios16.3.0 -pre-RA-sched=list-burr \
; RUN:     -verify-machineinstrs < %s | FileCheck %s

; Regression test for llvm/llvm-project#195561.
;
; The stack-protector guard check is emitted at the point returned by
; findSplitPointForStackProtector and clobbers the flags register (NZCV). If the
; instruction scheduler leaves a flags-defining compare in the parent block and
; the select that consumes it (lowered to FCSEL) in the split-off block, the
; split makes the FCSEL read an undefined NZCV across the guard check. The
; machine verifier reports "Using an undefined physical register"; without
; verification the scheduler asserts in LiveIntervals::handleMove.
;
; Here the icmp condition has a second use (a counter increment, standing in for
; PGO instrumentation) which keeps the compare in the parent while the select
; is scheduled into the tail-call sequence. The fix must keep the compare and
; the FCSEL together so codegen succeeds and verifies.

target datalayout = "e-m:o-i64:64-i128:128-n32:64-S128-Fn32"
target triple = "arm64-apple-ios16.3.0"

@ctr = private global i64 0

declare swifttailcc void @callee(ptr, ptr, i64, i64, i64, i8, ptr, i64, i1, double)

; CHECK-LABEL: _f:
; CHECK: fcsel
define swifttailcc void @f(i8 %x) sspreq {
  %c = icmp eq i8 %x, 0
  %z = zext i1 %c to i64
  %l = load i64, ptr @ctr, align 8
  %a = add i64 %l, %z
  store i64 %a, ptr @ctr, align 8
  %s = select i1 %c, double 1.000000e+00, double 0.000000e+00
  tail call swifttailcc void @callee(ptr null, ptr null, i64 0, i64 0, i64 0,
                                     i8 0, ptr null, i64 0, i1 false, double %s)
  ret void
}
