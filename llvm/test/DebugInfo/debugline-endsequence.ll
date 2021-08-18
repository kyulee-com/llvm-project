; RUN: llc %s -filetype=obj -o - | llvm-dwarfdump --debug-line - | FileCheck %s

target datalayout = "e-m:o-i64:64-i128:128-n32:64-S128"
target triple = "arm64-apple-macosx12.0.0"

; Check if the end_sequences are differently emitted per CU.

; CHECK: 0x0000000000000004 [[T:.*]] end_sequence
define void @f1() !dbg !15 {
  ret void, !dbg !18
}

; CHECK: 0x0000000000000008 [[T:.*]] end_sequence
define void @f2() !dbg !19 {
  ret void, !dbg !20
}

!llvm.dbg.cu = !{!0, !3}
!llvm.ident = !{!5, !5}
!llvm.module.flags = !{!6, !7, !8, !9, !10, !11, !12, !13, !14}

!0 = distinct !DICompileUnit(language: DW_LANG_C99, file: !1, producer: "LLVM", isOptimized: true, runtimeVersion: 0, emissionKind: FullDebug, enums: !2, nameTableKind: None, sysroot: "/")
!1 = !DIFile(filename: "<stdin1>", directory: "/")
!2 = !{}
!3 = distinct !DICompileUnit(language: DW_LANG_C99, file: !4, producer: "LLVM", isOptimized: true, runtimeVersion: 0, emissionKind: FullDebug, enums: !2, nameTableKind: None, sysroot: "/")
!4 = !DIFile(filename: "<stdin2>", directory: "/")
!5 = !{!"clang"}
!6 = !{i32 2, !"SDK Version", [2 x i32] [i32 11, i32 3]}
!7 = !{i32 7, !"Dwarf Version", i32 4}
!8 = !{i32 2, !"Debug Info Version", i32 3}
!9 = !{i32 1, !"wchar_size", i32 4}
!10 = !{i32 1, !"branch-target-enforcement", i32 0}
!11 = !{i32 1, !"sign-return-address", i32 0}
!12 = !{i32 1, !"sign-return-address-all", i32 0}
!13 = !{i32 1, !"sign-return-address-with-bkey", i32 0}
!14 = !{i32 7, !"PIC Level", i32 2}
!15 = distinct !DISubprogram(name: "f1", scope: !1, file: !1, line: 2, type: !16, scopeLine: 2, spFlags: DISPFlagDefinition, unit: !0, retainedNodes: !2)
!16 = !DISubroutineType(types: !17)
!17 = !{null}
!18 = !DILocation(line: 3, column: 1, scope: !15)
!19 = distinct !DISubprogram(name: "f2", scope: !4, file: !4, line: 2, type: !16, scopeLine: 2, spFlags: DISPFlagDefinition, unit: !3, retainedNodes: !2)
!20 = !DILocation(line: 3, column: 1, scope: !19)
