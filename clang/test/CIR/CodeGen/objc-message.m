// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -fclangir -emit-cir %s -o %t.cir
// RUN: FileCheck --input-file=%t.cir %s -check-prefix=CIR
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -fclangir -emit-llvm %s -o %t.ll
// RUN: FileCheck --input-file=%t.ll %s -check-prefix=LLVM

@interface MyClass
- (int)getValue;
- (int)addX:(int)x andY:(int)y;
+ (id)alloc;
- (id)init;
@end

// Test 1: Instance message - simple method call
int testInstanceMessage(MyClass *obj) {
  return [obj getValue];
}

// CIR-LABEL: cir.func @testInstanceMessage
// CIR: cir.objc.message %{{.*}}, "getValue"
// CIR-SAME: receiverType = !cir.objc.interface<"MyClass">
// CIR-SAME: -> !cir.int<s, 32>

// LLVM-LABEL: define {{.*}} @testInstanceMessage
// LLVM: call ptr @sel_registerName(ptr @.str.getValue)
// LLVM: call ptr (ptr, ptr, ...) @objc_msgSend(ptr %{{.*}}, ptr %{{.*}})
// LLVM: ptrtoint ptr %{{.*}} to i32

// Test 2: Message with multiple arguments
int testMessageWithArgs(MyClass *obj) {
  return [obj addX:10 andY:20];
}

// CIR-LABEL: cir.func @testMessageWithArgs
// CIR: cir.objc.message %{{.*}}, "addX:andY:"(%{{.*}}, %{{.*}})
// CIR-SAME: receiverType = !cir.objc.interface<"MyClass">
// CIR-SAME: -> !cir.int<s, 32>

// LLVM-LABEL: define {{.*}} @testMessageWithArgs
// LLVM: call ptr @sel_registerName(ptr @".str.addX:andY:")
// LLVM: call ptr (ptr, ptr, ...) @objc_msgSend(ptr %{{.*}}, ptr %{{.*}}, i32 10, i32 20)

// Test 3: Class message
MyClass *testClassMessage(void) {
  return [MyClass alloc];
}

// CIR-LABEL: cir.func @testClassMessage
// CIR: cir.objc.class_ref "MyClass"
// CIR: cir.objc.message %{{.*}}, "alloc"
// CIR-SAME: isClassMessage
// CIR-SAME: receiverType = !cir.objc.class

// LLVM-LABEL: define {{.*}} @testClassMessage
// LLVM: call ptr @objc_getClass(ptr @.str.MyClass)
// LLVM: call ptr @sel_registerName(ptr @.str.alloc)
// LLVM: call ptr (ptr, ptr, ...) @objc_msgSend(ptr %{{.*}}, ptr %{{.*}})

// Test 4: Message chain
MyClass *testMessageChain(void) {
  return [[MyClass alloc] init];
}

// CIR-LABEL: cir.func @testMessageChain
// CIR: cir.objc.class_ref "MyClass"
// CIR: cir.objc.message %{{.*}}, "alloc"
// CIR: cir.objc.message %{{.*}}, "init"

// LLVM-LABEL: define {{.*}} @testMessageChain
// LLVM: call ptr @objc_getClass(ptr @.str.MyClass)
// LLVM: call ptr @sel_registerName(ptr @.str.alloc)
// LLVM: call ptr (ptr, ptr, ...) @objc_msgSend
// LLVM: call ptr @sel_registerName(ptr @.str.init)
// LLVM: call ptr (ptr, ptr, ...) @objc_msgSend

// Verify runtime function declarations
// LLVM-DAG: declare ptr @objc_msgSend(ptr, ptr, ...)
// LLVM-DAG: declare ptr @sel_registerName(ptr)
// LLVM-DAG: declare ptr @objc_getClass(ptr)
