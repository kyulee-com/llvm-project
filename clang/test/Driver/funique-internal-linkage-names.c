// RUN: %clang -### %s -c 2>&1 \
// RUN:   | FileCheck %s --check-prefix=DEFAULT \
// RUN:       --implicit-check-not='"-funique-internal-linkage-names='
// RUN: %clang -### -funique-internal-linkage-names %s -c 2>&1 \
// RUN:   | FileCheck %s --check-prefix=FUNCTIONS
// RUN: %clang -### -funique-internal-linkage-names=functions %s -c 2>&1 \
// RUN:   | FileCheck %s --check-prefix=FUNCTIONS
// RUN: %clang -### -funique-internal-linkage-names=all %s -c 2>&1 \
// RUN:   | FileCheck %s --check-prefix=ALL
// RUN: %clang -### -funique-internal-linkage-names=none %s -c 2>&1 \
// RUN:   | FileCheck %s --check-prefix=NONE
// RUN: %clang -### -fno-unique-internal-linkage-names %s -c 2>&1 \
// RUN:   | FileCheck %s --check-prefix=NONE
// RUN: %clang -### -funique-internal-linkage-names=all \
// RUN:   -funique-internal-linkage-names %s -c 2>&1 \
// RUN:   | FileCheck %s --check-prefix=FUNCTIONS
// RUN: %clang -### -funique-internal-linkage-names \
// RUN:   -fno-unique-internal-linkage-names %s -c 2>&1 \
// RUN:   | FileCheck %s --check-prefix=NONE
// RUN: %clang -### -fno-unique-internal-linkage-names \
// RUN:   -funique-internal-linkage-names=all %s -c 2>&1 \
// RUN:   | FileCheck %s --check-prefix=ALL
// RUN: not %clang -funique-internal-linkage-names=invalid %s -c -o %t 2>&1 \
// RUN:   | FileCheck %s --check-prefix=INVALID

// FUNCTIONS: "-funique-internal-linkage-names=functions"
// ALL: "-funique-internal-linkage-names=all"
// NONE: "-funique-internal-linkage-names=none"
// INVALID: error: invalid value 'invalid' in '-funique-internal-linkage-names=invalid'

// DEFAULT: "-cc1"
