; RUN: opt -mergefunc -S < %s | FileCheck %s

declare void @dummy()

; CHECK-NOT: tail call void @foo()
; CHECK-NOT: tail call void @bar()

define void @foo() nomerge {
  call void @dummy()
  call void @dummy()
  ret void
}

define void @bar() nomerge {
  call void @dummy()
  call void @dummy()
  ret void
}
