; RUN: opt < %s -S -forceattrs | FileCheck %s --check-prefix=CHECK-CONTROL
; RUN: opt < %s -S -forceattrs -force-add-attribute foo:noinline | FileCheck %s --check-prefix=CHECK-FOO
; RUN: opt < %s -S -passes=forceattrs -force-add-attribute foo:noinline | FileCheck %s --check-prefix=CHECK-FOO
; RUN: opt < %s -S -passes=forceattrs -force-remove-attribute goo:noinline | FileCheck %s --check-prefix=REMOVE
; RUN: opt < %s -S -passes=forceattrs -force-add-attribute foo:noinline -force-remove-attribute goo:noinline | FileCheck %s --check-prefix=ADD-REMOVE

; CHECK-CONTROL: define void @foo() {
; CHECK-FOO: define void @foo() #0 {
define void @foo() {
  ret void
}

; REMOVE: define void @goo() {
; `remove` takes precedence over `add`.
; ADD-REMOVE: define void @goo() {
define void @goo() noinline {
  ret void
}

; CHECK-FOO: attributes #0 = { noinline }
