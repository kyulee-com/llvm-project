; RUN: llc -mtriple aarch64 -enable-machine-outliner \
; RUN:  -verify-machineinstrs %s -o - | FileCheck %s

@v = common dso_local global ptr null, align 8

; CHECK-LABEL:  foo:                                    // @foo
; CHECK-NEXT:   // %bb.0:                               // %entry
; CHECK-NEXT:       paciasp
; CHECK:            bl [[OUTLINE:OUTLINED_FUNCTION_[0-9]+]]
; CHECK:            retaa
define dso_local void @foo(i32 %x) #0 {
entry:
  %0 = zext i32 %x to i64
  %vla = alloca i32, i64 %0, align 4
  store volatile ptr %vla, ptr @v, align 8
  store volatile ptr %vla, ptr @v, align 8
  store volatile ptr %vla, ptr @v, align 8
  store volatile ptr %vla, ptr @v, align 8
  store volatile ptr %vla, ptr @v, align 8
  store volatile ptr %vla, ptr @v, align 8
  ret void
}

; CHECK-LABEL:  bar:                                    // @bar
; CHECK-NEXT:   // %bb.0:                               // %entry
; CHECK-NEXT:       paciasp
; CHECK:            bl [[OUTLINE:OUTLINED_FUNCTION_[0-9]+]]
; CHECK:            retaa
define dso_local void @bar(i32 %x) #0 {
entry:
  %0 = zext i32 %x to i64
  %vla = alloca i32, i64 %0, align 4
  store volatile ptr null, ptr @v, align 8
  store volatile ptr %vla, ptr @v, align 8
  store volatile ptr %vla, ptr @v, align 8
  store volatile ptr %vla, ptr @v, align 8
  store volatile ptr %vla, ptr @v, align 8
  store volatile ptr %vla, ptr @v, align 8
  ret void
}

attributes #0 = { nounwind "target-features"="+v8.3a" "frame-pointer"="all" "sign-return-address"="all" "sign-return-address-key"="a_key" }

; The signed helper only contains stores, not the dynamic SP adjustment.
; CHECK-LABEL: OUTLINED_FUNCTION_0:
; CHECK: paciasp
; CHECK-NEXT: str x8, [x9, :lo12:v]
; CHECK-NEXT: str x8, [x9, :lo12:v]
; CHECK-NEXT: str x8, [x9, :lo12:v]
; CHECK-NEXT: retaa
