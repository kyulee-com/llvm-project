// RUN: %clang_cc1 -triple x86_64-apple-darwin -fclangir -emit-cir %s -o %t.cir
// RUN: FileCheck --input-file=%t.cir %s -check-prefix=CIR
// RUN: %clang_cc1 -triple x86_64-apple-darwin -fclangir -emit-llvm %s -o %t.ll
// RUN: FileCheck --input-file=%t.ll %s -check-prefix=LLVM

@interface MyClass
- (int)getValue __attribute__((objc_direct));
- (int)normalMethod;
+ (id)directClassMethod __attribute__((objc_direct));
@end

// Test 1: Direct instance method call
int testDirectMethod(MyClass *obj) {
  return [obj getValue];
}

// CIR-LABEL: cir.func @testDirectMethod
// CIR: cir.objc.message %{{.*}}, "getValue"
// CIR-SAME: isDirect
// CIR-SAME: directMethodImplSymbol = "\01-[MyClass getValue]"

// LLVM-LABEL: define {{.*}} @testDirectMethod
// LLVM: call i32 @"\01-[MyClass getValue]"(ptr %{{.*}})
// LLVM-NOT: objc_msgSend

// Test 2: Normal (non-direct) method call
int testNormalMethod(MyClass *obj) {
  return [obj normalMethod];
}

// CIR-LABEL: cir.func @testNormalMethod
// CIR: cir.objc.message %{{.*}}, "normalMethod"
// CIR-NOT: isDirect

// LLVM-LABEL: define {{.*}} @testNormalMethod
// LLVM: load ptr, ptr @OBJC_SELECTOR_REFERENCES_
// LLVM: call ptr @objc_msgSend(ptr %{{.*}}, ptr %{{.*}})

// Test 3: Direct class method call
id testDirectClassMethod(void) {
  return [MyClass directClassMethod];
}

// CIR-LABEL: cir.func @testDirectClassMethod
// CIR: cir.objc.message %{{.*}}, "directClassMethod"
// CIR-SAME: isDirect
// CIR-SAME: directMethodImplSymbol = "\01+[MyClass directClassMethod]"

// LLVM-LABEL: define {{.*}} @testDirectClassMethod
// LLVM: call ptr @"\01+[MyClass directClassMethod]"(ptr %{{.*}})
// LLVM-NOT: objc_msgSend

@implementation MyClass
- (int)getValue {
  return 42;
}

- (int)normalMethod {
  return 100;
}

+ (id)directClassMethod {
  return (id)0;
}
@end
