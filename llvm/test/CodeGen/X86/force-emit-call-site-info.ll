; RUN: llc -mtriple=x86_64-linux-gnu %s -o - -stop-before=finalize-isel | FileCheck %s --check-prefix=WITHOUT
; RUN: llc -mtriple=x86_64-linux-gnu -force-emit-call-site-info %s -o - -stop-before=finalize-isel | FileCheck %s --check-prefix=WITH

; Verify that -force-emit-call-site-info preserves the machine call site info
; side table without requiring the frontend to set EmitCallSiteInfo. Debug-style
; argument forwarding info remains controlled by EmitCallSiteInfo.

; WITHOUT: callSites:       []
; WITH: callSites:
; WITH-NEXT:   - { bb: {{.*}}, offset: {{.*}} }
; WITH-NOT: fwdArgRegs

define i32 @caller(i32 %a, i32 %b, i32 %c) {
entry:
  %add = add nsw i32 %b, %a
  %call = tail call i32 @callee(i32 %add, i32 %c, i32 10)
  ret i32 %call
}

declare i32 @callee(i32, i32, i32)
