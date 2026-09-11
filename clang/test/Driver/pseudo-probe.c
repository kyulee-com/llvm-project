// RUN: %clang -### -fpseudo-probe-for-profiling %s 2>&1 | FileCheck %s --check-prefix=AUTO
// RUN: %clang -### -fno-pseudo-probe-for-profiling %s 2>&1 | FileCheck %s --check-prefix=NOPROBE
// RUN: %clang -### -fpseudo-probe-for-profiling -fdebug-info-for-profiling %s 2>&1 | FileCheck %s --check-prefix=AUTO --check-prefix=YESDEBUG
// RUN: %clang -### -fpseudo-probe-for-profiling \
// RUN:   -funique-internal-linkage-names %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=FUNCTIONS
// RUN: %clang -### -fpseudo-probe-for-profiling \
// RUN:   -funique-internal-linkage-names=all %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=ALL
// RUN: %clang -### -fpseudo-probe-for-profiling \
// RUN:   -fno-unique-internal-linkage-names %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=NONE

// YESDEBUG: -fdebug-info-for-profiling
// AUTO-DAG: "-fpseudo-probe-for-profiling"
// AUTO-DAG: "-funique-internal-linkage-names=functions"
// FUNCTIONS-DAG: "-fpseudo-probe-for-profiling"
// FUNCTIONS-DAG: "-funique-internal-linkage-names=functions"
// ALL-DAG: "-fpseudo-probe-for-profiling"
// ALL-DAG: "-funique-internal-linkage-names=all"
// NONE-DAG: "-fpseudo-probe-for-profiling"
// NONE-DAG: "-funique-internal-linkage-names=none"
// NOPROBE-NOT: -fpseudo-probe-for-profiling
// NOPROBE-NOT: -funique-internal-linkage-names

// On Darwin, -fpseudo-probe-for-profiling should trigger dsymutil
// RUN: %clang -target arm64-apple-darwin -### -o foo -fpseudo-probe-for-profiling %s 2>&1 | FileCheck %s --check-prefix=CHECK-DSYMUTIL-PSEUDO-PROBE
// CHECK-DSYMUTIL-PSEUDO-PROBE: "-cc1"
// CHECK-DSYMUTIL-PSEUDO-PROBE: ld
// CHECK-DSYMUTIL-PSEUDO-PROBE: dsymutil

// RUN: %clang -target arm64-apple-darwin -### -o foo -fno-pseudo-probe-for-profiling %s 2>&1 | FileCheck %s --check-prefix=CHECK-NO-DSYMUTIL-PSEUDO-PROBE
// CHECK-NO-DSYMUTIL-PSEUDO-PROBE: "-cc1"
// CHECK-NO-DSYMUTIL-PSEUDO-PROBE: ld
// CHECK-NO-DSYMUTIL-PSEUDO-PROBE-NOT: dsymutil

// On Darwin, -fdebug-info-for-profiling should trigger dsymutil
// RUN: %clang -target arm64-apple-darwin -### -o foo -fdebug-info-for-profiling %s 2>&1 | FileCheck %s --check-prefix=CHECK-DSYMUTIL-DEBUG-PROF
// CHECK-DSYMUTIL-DEBUG-PROF: "-cc1"
// CHECK-DSYMUTIL-DEBUG-PROF: ld
// CHECK-DSYMUTIL-DEBUG-PROF: dsymutil

// RUN: %clang -target arm64-apple-darwin -### -o foo -fno-debug-info-for-profiling %s 2>&1 | FileCheck %s --check-prefix=CHECK-NO-DSYMUTIL-DEBUG-PROF
// CHECK-NO-DSYMUTIL-DEBUG-PROF: "-cc1"
// CHECK-NO-DSYMUTIL-DEBUG-PROF: ld
// CHECK-NO-DSYMUTIL-DEBUG-PROF-NOT: dsymutil
