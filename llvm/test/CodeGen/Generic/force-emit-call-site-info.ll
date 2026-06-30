; RUN: llc -mtriple=aarch64-linux-gnu %s -o - -stop-before=finalize-isel | FileCheck %s --check-prefix=WITHOUT
; RUN: llc -mtriple=aarch64-linux-gnu -force-emit-call-site-info %s -o - -stop-before=finalize-isel | FileCheck %s --check-prefix=WITH

; Verify that -force-emit-call-site-info enables call site info production
; without requiring the frontend to set EmitCallSiteInfo.

; WITHOUT: callSites:       []
; WITH: callSites:
; WITH-NEXT:   - { bb: {{.*}}, offset: {{.*}}, fwdArgRegs:
; WITH-NEXT:       - { arg: 0, reg: '$w0' }
; WITH-NEXT:       - { arg: 1, reg: '$w1' }
; WITH-NEXT:       - { arg: 2, reg: '$w2' } }

define i32 @caller(i32 %a, i32 %b, i32 %c) {
entry:
  %add = add nsw i32 %b, %a
  %call = tail call i32 @callee(i32 %add, i32 %c, i32 10)
  ret i32 %call
}

declare i32 @callee(i32, i32, i32)
