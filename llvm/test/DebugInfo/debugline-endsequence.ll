; RUN: %llc_dwarf %s -filetype=obj -o - | llvm-dwarfdump -all - | FileCheck %s

; CHECK: DW_AT_name ("<stdin>")
; CHECK: DW_AT_high_pc ([[EndSeq:.*]])

; Expect the line table ends with the highest address for each CU.
; CHECK: [[EndSeq]] [[T:.*]] end_sequence

target triple = "x86_64-apple-macosx11.0.0"

define void @f1() !dbg !10 {
  ret void, !dbg !13
}

define void @f2() {
  ret void
}

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!3, !4, !5, !6, !7, !8}
!llvm.ident = !{!9}

!0 = distinct !DICompileUnit(language: DW_LANG_C99, file: !1, producer: "LLVM", isOptimized: true, runtimeVersion: 0, emissionKind: FullDebug, enums: !2, splitDebugInlining: false, nameTableKind: None, sysroot: "/")
!1 = !DIFile(filename: "<stdin>", directory: "/")
!2 = !{}
!3 = !{i32 7, !"Dwarf Version", i32 4}
!4 = !{i32 2, !"Debug Info Version", i32 3}
!5 = !{i32 1, !"wchar_size", i32 4}
!6 = !{i32 7, !"PIC Level", i32 2}
!7 = !{i32 7, !"uwtable", i32 1}
!8 = !{i32 7, !"frame-pointer", i32 2}
!9 = !{!"clang"}
!10 = distinct !DISubprogram(name: "f1", scope: !1, file: !1, line: 1, type: !11, scopeLine: 1, flags: DIFlagAllCallsDescribed, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0, retainedNodes: !2)
!11 = !DISubroutineType(types: !12)
!12 = !{null}
!13 = !DILocation(line: 1, column: 13, scope: !10)
