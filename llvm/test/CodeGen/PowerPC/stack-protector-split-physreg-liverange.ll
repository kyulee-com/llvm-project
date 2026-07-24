; RUN: llc -mcpu=pwr10 -verify-machineinstrs < %s | FileCheck %s

; Reduced reproducer from llvm/llvm-project#195561.
;
; The stack-protector guard check clobbers the condition register (CR). When the
; split point returned by findSplitPointForStackProtector leaves a CR live range
; crossing it (a definition in the parent block and a use, here the indirect
; tail call, in the split-off block), the split makes that use read an undefined
; physical register. The machine verifier reports "Using an undefined physical
; register"; without verification the scheduler asserts in
; LiveIntervals::handleMove. Sinking the split point to keep the live range
; intact makes codegen verify.

target datalayout = "e-m:e-Fn32-i64:64-i128:128-n32:64-S128-v256:256:256-v512:512:512"
target triple = "powerpc64le-unknown-linux5.10.0-musl"

%heap.SafeAllocator.FormatMemory = type { { ptr, i64 }, i6, [7 x i8] }
%debug.FormatStackTrace = type { %debug.StackTrace, i2, [7 x i8] }
%debug.StackTrace = type { { ptr, i64 }, i64 }

; CHECK-LABEL: heap.SafeAllocator.deinitLargeAlloc:
; CHECK: bl __stack_chk_fail
define fastcc void @heap.SafeAllocator.deinitLargeAlloc() #0 {
Entry:
  %0 = alloca { %heap.SafeAllocator.FormatMemory, %debug.FormatStackTrace }, align 8
  %1 = getelementptr i8, ptr %0, i64 24
  tail call fastcc void null(ptr null, ptr null, i64 0, i6 0, i64 0)
  ret void
}

attributes #0 = { sspstrong "target-cpu"="pwr10" }
