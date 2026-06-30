; RUN: llc -mtriple=aarch64-apple-ios -force-emit-call-site-info %s -o - -stop-before=finalize-isel | FileCheck %s

; CHECK-LABEL: name: no_stack_args
; CHECK: callSites:
; CHECK-NEXT:   - { bb: {{.*}}, offset: {{.*}}, hasStackArguments: false }
define void @no_stack_args(ptr %p) {
entry:
  call void @callee_one(ptr %p)
  ret void
}

; CHECK-LABEL: name: stack_args
; CHECK: callSites:
; CHECK-NEXT:   - { bb: {{.*}}, offset: {{.*}}, hasStackArguments: true }
define void @stack_args(i64 %a0, i64 %a1, i64 %a2, i64 %a3, i64 %a4, i64 %a5, i64 %a6, i64 %a7, i64 %a8) {
entry:
  call void @callee_many(i64 %a0, i64 %a1, i64 %a2, i64 %a3, i64 %a4, i64 %a5, i64 %a6, i64 %a7, i64 %a8)
  ret void
}

declare void @callee_one(ptr)
declare void @callee_many(i64, i64, i64, i64, i64, i64, i64, i64, i64)
