// RUN: %clang_cc1 -triple x86_64-apple-macosx10.14.0 -fclangir -emit-cir %s -o %t.cir
// RUN: FileCheck --input-file=%t.cir %s -check-prefix=CIR

// Test basic Objective-C message sending in ClangIR
//
// ✨ OPTIMIZATION RESEARCH NOTE:
// These tests verify that we preserve high-level ObjC semantic information
// in the CIR that would be lost in traditional LLVM IR lowering.
//
// Key things we're checking:
// 1. Receiver type is preserved (!cir.objc.interface<"NSString"> not just i8*)
// 2. Selector name is preserved as string attribute
// 3. Message send is a high-level operation, not just objc_msgSend call
//
// This preserved information enables future optimization passes!

@interface NSString
+ (instancetype)alloc;
- (instancetype)init;
- (int)length;
- (void)setValue:(int)value;
@end

@interface MyClass : NSString
- (int)compute:(int)x with:(int)y;
@end

@implementation MyClass

// Test 1: Instance message with no arguments
// CIR-LABEL: cir.func @"\01-[MyClass testInstanceMessage]"
- (int)testInstanceMessage {
  // CIR: [[OBJ:%.*]] = cir.load
  // CIR: [[RESULT:%.*]] = cir.objc.message [[OBJ]], "length"
  // CIR-SAME: : (!cir.objc.interface<"NSString">) -> !s32i
  // CIR: cir.return [[RESULT]]

  NSString *str = [[NSString alloc] init];
  return [str length];
}

// Test 2: Instance message with arguments
// CIR-LABEL: cir.func @"\01-[MyClass testMessageWithArgs]"
- (void)testMessageWithArgs {
  // CIR: [[OBJ:%.*]] = cir.load
  // CIR: [[VAL:%.*]] = cir.const #cir.int<42>
  // CIR: cir.objc.message [[OBJ]], "setValue:"([[VAL]])
  // CIR-SAME: : (!cir.objc.interface<"NSString">, !s32i) -> ()

  NSString *str = [[NSString alloc] init];
  [str setValue:42];
}

// Test 3: Class message (alloc)
// CIR-LABEL: cir.func @"\01-[MyClass testClassMessage]"
- (instancetype)testClassMessage {
  // CIR: [[CLASS:%.*]] = cir.objc.class_ref "NSString" : !cir.objc.class
  // CIR: [[RESULT:%.*]] = cir.objc.message [[CLASS]], "alloc"
  // CIR-SAME: : (!cir.objc.class) -> !cir.objc.id
  // CIR: cir.return [[RESULT]]

  return [NSString alloc];
}

// Test 4: Multiple arguments
// CIR-LABEL: cir.func @"\01-[MyClass testMultipleArgs]"
- (int)testMultipleArgs {
  // CIR: [[SELF:%.*]] = cir.load
  // CIR: [[X:%.*]] = cir.const #cir.int<10>
  // CIR: [[Y:%.*]] = cir.const #cir.int<20>
  // CIR: [[RESULT:%.*]] = cir.objc.message [[SELF]], "compute:with:"([[X]], [[Y]])
  // CIR-SAME: : (!cir.objc.interface<"MyClass">, !s32i, !s32i) -> !s32i
  // CIR: cir.return [[RESULT]]

  return [self compute:10 with:20];
}

// Test 5: Super message
// CIR-LABEL: cir.func @"\01-[MyClass testSuperMessage]"
- (int)testSuperMessage {
  // CIR: [[SELF:%.*]] = cir.load
  // CIR: [[RESULT:%.*]] = cir.objc.message_super [[SELF]], "length", "MyClass", "NSString"()
  // CIR-SAME: : (!cir.objc.interface<"MyClass">) -> !s32i
  // CIR: cir.return [[RESULT]]

  return [super length];
}

// Test 6: Chained message sends (alloc + init pattern)
// CIR-LABEL: cir.func @"\01-[MyClass testChainedMessages]"
- (instancetype)testChainedMessages {
  // First: [NSString alloc]
  // CIR: [[CLASS:%.*]] = cir.objc.class_ref "NSString"
  // CIR: [[ALLOC_RESULT:%.*]] = cir.objc.message [[CLASS]], "alloc"

  // Then: [... init]
  // CIR: [[INIT_RESULT:%.*]] = cir.objc.message [[ALLOC_RESULT]], "init"
  // CIR: cir.return [[INIT_RESULT]]

  return [[NSString alloc] init];
}

@end

// ===----------------------------------------------------------------------===//
// Summary of what we're testing:
// ===----------------------------------------------------------------------===//
//
// ✅ Instance messages: [obj method]
// ✅ Class messages: [MyClass alloc]
// ✅ Super messages: [super method]
// ✅ Messages with no args: [obj length]
// ✅ Messages with one arg: [obj setValue:42]
// ✅ Messages with multiple args: [self compute:10 with:20]
// ✅ Chained messages: [[Class alloc] init]
// ✅ Type preservation: !cir.objc.interface<"NSString"> not i8*
// ✅ Selector preservation: "length" not mangled/hashed
//
// What optimization opportunities does this preserve?
// 1. **Devirtualization**: We can see the receiver type is NSString*
// 2. **Selector hoisting**: We can identify repeated "alloc" calls
// 3. **Alloc/init pattern**: We can detect [[Class alloc] init] idiom
// 4. **Super call devirtualization**: We know exact target for super calls
// 5. **Type-based analysis**: Can track NSString* vs generic id types
//
// Compare to traditional LLVM IR:
// Traditional: call i8* @objc_msgSend(i8* %obj, i8* %sel, ...)
//              ↑ Everything is i8*, no selector string, no semantic info!
// ClangIR:     cir.objc.message %obj, "length" : (!cir.objc.interface<"NSString">) -> !s32i
//              ↑ Preserves type, selector, and high-level operation!
//
// ===----------------------------------------------------------------------===//
