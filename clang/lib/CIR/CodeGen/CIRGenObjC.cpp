//===--- CIRGenObjC.cpp - Emit CIR Code for Objective-C ------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This contains code to emit Objective-C code as CIR operations.
//
// ✨ OPTIMIZATION RESEARCH NOTE:
// ================================
// This file is the KEY to preserving high-level ObjC semantics in ClangIR!
//
// Traditional Clang (AST → LLVM IR):
//   ObjCMessageExpr → call i8* @objc_msgSend(i8* %receiver, i8* %sel, ...)
//   ↑ All type info lost! Can't tell what class, what method, etc.
//
// ClangIR (AST → CIR → LLVM IR):
//   ObjCMessageExpr → cir.objc.message %receiver, "length" : (!cir.objc.interface<"NSString">) → !s64i
//   ↑ Preserves: receiver type, selector name, message kind
//
// This enables optimization passes to:
// 1. Devirtualize message sends to direct calls
// 2. Inline known methods
// 3. Eliminate nil checks
// 4. Hoist selector lookups out of loops
// 5. Recognize alloc/init patterns
//
//===----------------------------------------------------------------------===//

#include "CIRGenFunction.h"
#include "CIRGenModule.h"
#include "clang/AST/DeclObjC.h"
#include "clang/AST/ExprObjC.h"
#include "clang/AST/StmtObjC.h"
#include "clang/CIR/Dialect/IR/CIRDialect.h"
#include "clang/CIR/Dialect/IR/CIROpsEnums.h"
#include "clang/CIR/Dialect/IR/CIRTypes.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinTypes.h"

using namespace cir;
using namespace clang;
using namespace clang::CIRGen;

namespace {

/// Helper to get the Objective-C runtime - for now we assume Apple/Darwin runtime
/// In the future, this could be abstracted to support GNU runtime as well
class CIRGenObjCRuntime {
  CIRGenModule &CGM;

public:
  CIRGenObjCRuntime(CIRGenModule &cgm) : CGM(cgm) {}

  // Get the CIR type for 'id'
  mlir::Type getIdType() {
    return cir::ObjCIdType::get(CGM.getBuilder().getContext());
  }

  // Get the CIR type for 'Class'
  mlir::Type getClassType() {
    return cir::ObjCClassType::get(CGM.getBuilder().getContext());
  }

  // Get the CIR type for 'SEL'
  mlir::Type getSELType() {
    return cir::ObjCSELType::get(CGM.getBuilder().getContext());
  }
};

} // anonymous namespace

//===--------------------------------------------------------------------===//
// Message Send Implementation
//===--------------------------------------------------------------------===//

/// Load the 'self' parameter in an Objective-C method
/// This is the receiver of the message when we're inside a method
mlir::Value CIRGenFunction::loadObjCSelf() {
  // In ObjC methods, 'self' is always the first parameter
  // It's implicitly declared and available in the function
  VarDecl *selfDecl = cast<ObjCMethodDecl>(curFuncDecl)->getSelfDecl();

  // Look up the self parameter in our local variable map
  auto selfAddr = getAddrOfLocalVar(selfDecl);

  // Load the value
  return builder.create<cir::LoadOp>(
      getLoc(selfDecl->getLocation()),
      selfAddr.getElementType(),
      selfAddr.getPointer());
}

/// Generate a reference to an Objective-C class by name
/// Example: [NSString alloc] → needs class reference for "NSString"
mlir::Value CIRGenFunction::emitObjCClassRef(const ObjCInterfaceDecl *ID) {
  assert(ID && "emitObjCClassRef called with null interface");

  std::string className = ID->getNameAsString();
  auto classType = cir::ObjCClassType::get(builder.getContext());

  // 🎯 OPTIMIZATION OPPORTUNITY:
  // This operation preserves the class name! A future optimization pass
  // could cache class lookups (objc_getClass is expensive) or even
  // replace them with compile-time constants if the class is known.
  auto classRef = builder.create<cir::ObjCClassRefOp>(
      getLoc(ID->getLocation()), classType, llvm::StringRef(className));

  return classRef.getResult();
}

/// Emit code for a message send to 'super'
/// Example: [super dealloc] in a method
///
/// 🎯 OPTIMIZATION GOLD MINE:
/// Super calls are statically resolvable! We know EXACTLY which method
/// will be called (the superclass's implementation). This is much easier
/// to devirtualize than instance messages.
RValue CIRGenFunction::emitObjCMessageSendSuper(
    const ObjCMessageExpr *E,
    mlir::Value self,
    const ObjCInterfaceDecl *currentClass,
    const ObjCInterfaceDecl *superClass) {

  assert(currentClass && superClass && "Super message with null class info");

  // Get selector string
  Selector sel = E->getSelector();
  std::string selectorStr = sel.getAsString();

  // Build argument list
  llvm::SmallVector<mlir::Value, 8> args;
  for (unsigned i = 0; i < E->getNumArgs(); ++i) {
    RValue argRV = emitAnyExpr(E->getArg(i));
    args.push_back(argRV.getValue());
  }

  // Get class names
  std::string currentClassName = currentClass->getNameAsString();
  std::string superClassName = superClass->getNameAsString();

  // Determine result type
  QualType resultType = E->getType();
  mlir::Type cirResultType = convertType(resultType);

  // Build the super message send operation
  // ✨ This preserves both current class and super class names!
  // A devirtualization pass can look up the method in the superclass
  // and potentially replace this with a direct call.
  auto msgOp = builder.create<cir::ObjCMessageSuperOp>(
      getLoc(E->getExprLoc()),
      cirResultType,
      self,
      llvm::StringRef(selectorStr),
      llvm::StringRef(currentClassName),
      llvm::StringRef(superClassName),
      args,
      mlir::UnitAttr());  // isClassMessage - always false for super instance messages

  if (resultType->isVoidType()) {
    return RValue::get(nullptr);
  }

  return RValue::get(msgOp.getResult());
}

/// Main entry point: Emit code for an Objective-C message expression
/// Handles all 4 kinds of message sends:
///   1. Instance: [obj method]
///   2. Class: [MyClass alloc]
///   3. Super (instance): [super dealloc]
///   4. Super (class): [super initialize]
///
/// ✨ KEY OPTIMIZATION PRESERVATION:
/// Unlike LLVM IR which makes all message sends look identical, we preserve:
/// - Receiver type (NSString* vs id)
/// - Selector name ("length" vs "count")
/// - Message kind (instance vs class vs super)
/// - Static type information from the AST
RValue CIRGenFunction::emitObjCMessageExpr(const ObjCMessageExpr *E) {

  // Get selector string - this is preserved in the CIR operation!
  Selector sel = E->getSelector();
  std::string selectorStr = sel.getAsString();

  // Build arguments
  llvm::SmallVector<mlir::Value, 8> args;
  for (unsigned i = 0; i < E->getNumArgs(); ++i) {
    RValue argRV = emitAnyExpr(E->getArg(i));
    args.push_back(argRV.getValue());
  }

  // Determine result type
  QualType resultType = E->getType();
  mlir::Type cirResultType = resultType->isVoidType()
      ? mlir::Type()
      : convertType(resultType);

  mlir::Value receiver;
  mlir::Type receiverType;
  bool isClassMessage = false;

  // Handle different receiver kinds
  switch (E->getReceiverKind()) {

  case ObjCMessageExpr::Instance: {
    // Instance message: [obj method]
    // Build the receiver expression
    RValue receiverRV = emitAnyExpr(E->getInstanceReceiver());
    receiver = receiverRV.getValue();

    // 🎯 CRITICAL: Preserve the receiver's static type!
    // If receiver is NSString*, preserve that - don't just treat as id!
    receiverType = convertType(E->getInstanceReceiver()->getType());

    // Example optimization this enables:
    // If we know receiver is NSString* and method is 'length',
    // a later pass could check if NSString is a sealed class and
    // if so, replace cir.objc.message with a direct call to
    // NSString's length implementation!
    break;
  }

  case ObjCMessageExpr::Class: {
    // Class message: [MyClass alloc]
    // Get the class being messaged
    const ObjCInterfaceDecl *classDecl = E->getClassReceiver()->getAs<ObjCInterfaceType>()->getDecl();
    receiver = emitObjCClassRef(classDecl);
    receiverType = receiver.getType();
    isClassMessage = true;

    // 🎯 OPTIMIZATION: Class messages to known classes!
    // Example: [NSString alloc]
    // We know the exact class, so could potentially:
    // 1. Replace with direct call to +[NSString alloc]
    // 2. Cache the class reference
    // 3. Inline the allocation if size is known
    break;
  }

  case ObjCMessageExpr::SuperInstance: {
    // Super instance message: [super dealloc]
    // Load self
    mlir::Value self = loadObjCSelf();

    // Get current class and superclass
    const ObjCMethodDecl *method = cast<ObjCMethodDecl>(curFuncDecl);
    const ObjCInterfaceDecl *currentClass = method->getClassInterface();
    const ObjCInterfaceDecl *superClass = currentClass->getSuperClass();

    // Use specialized super message send
    return emitObjCMessageSendSuper(E, self, currentClass, superClass);
  }

  case ObjCMessageExpr::SuperClass: {
    // Super class message: [super initialize]
    // Load self's class
    mlir::Value self = loadObjCSelf();

    // Get class info
    const ObjCMethodDecl *method = cast<ObjCMethodDecl>(curFuncDecl);
    const ObjCInterfaceDecl *currentClass = method->getClassInterface();
    const ObjCInterfaceDecl *superClass = currentClass->getSuperClass();

    // Use specialized super message send
    return emitObjCMessageSendSuper(E, self, currentClass, superClass);
  }
  }

  // Create the message send operation
  // ✨ This is where the magic happens - we preserve ALL this information:
  // - receiver value and its type
  // - selector as a string
  // - whether it's a class message
  // - whether it's a direct method (objc_direct attribute)
  // - argument types
  // - return type
  //
  // Compare to LLVM IR which just has:
  // call i8* @objc_msgSend(i8* %receiver, i8* %sel, ...)
  // ↑ Everything is i8*, no selector string, no class info!

  // 🎯 DEVIRTUALIZATION OPPORTUNITY: Check for objc_direct methods
  // Direct methods bypass the dynamic dispatch machinery and compile to
  // direct function calls, providing 3-5x performance improvement
  bool isDirect = false;
  std::string directMethodSymbol;

  // Try to get the method declaration
  const ObjCMethodDecl *method = E->getMethodDecl();

  // If getMethodDecl() returns null (common for interface-only declarations),
  // look up the method in the receiver's interface
  if (!method && E->getReceiverKind() == ObjCMessageExpr::Instance) {
    if (const ObjCObjectPointerType *objPtrType =
            E->getInstanceReceiver()->getType()->getAs<ObjCObjectPointerType>()) {
      if (const ObjCInterfaceDecl *iface = objPtrType->getInterfaceDecl()) {
        method = iface->lookupInstanceMethod(E->getSelector());
      }
    }
  } else if (!method && E->getReceiverKind() == ObjCMessageExpr::Class) {
    if (const ObjCInterfaceType *ifaceType = E->getClassReceiver()->getAs<ObjCInterfaceType>()) {
      if (const ObjCInterfaceDecl *iface = ifaceType->getDecl()) {
        method = iface->lookupClassMethod(E->getSelector());
      }
    }
  }

  if (method) {
    // If the method is statically known and has objc_direct attribute,
    // we can generate a direct call instead of objc_msgSend
    isDirect = method->isDirectMethod();

    if (isDirect) {
      // Generate the method implementation symbol name
      // Format: "\01-[ClassName selectorName]" for instance methods
      //         "\01+[ClassName selectorName]" for class methods
      const ObjCInterfaceDecl *classInterface = method->getClassInterface();
      std::string className = classInterface->getNameAsString();

      llvm::raw_string_ostream symbolStream(directMethodSymbol);
      symbolStream << "\01" << (method->isInstanceMethod() ? '-' : '+')
                   << "[" << className << " " << selectorStr << "]";
      symbolStream.flush();
    }
  }

  auto msgOp = builder.create<cir::ObjCMessageOp>(
      getLoc(E->getExprLoc()),
      cirResultType,
      receiver,
      llvm::StringRef(selectorStr),
      args,
      receiverType,
      isClassMessage ? builder.getUnitAttr() : mlir::UnitAttr(),
      isDirect ? builder.getUnitAttr() : mlir::UnitAttr(),
      directMethodSymbol.empty() ? mlir::StringAttr() : builder.getStringAttr(directMethodSymbol));

  if (resultType->isVoidType()) {
    return RValue::get(nullptr);
  }

  return RValue::get(msgOp.getResult());
}

//===--------------------------------------------------------------------===//
// Property Access (Stub for future implementation)
//===--------------------------------------------------------------------===//

/// Emit code for property getter
/// Example: obj.property → [obj property]
/// For now, properties desugar to message sends, so this redirects there
RValue CIRGenFunction::emitObjCPropertyGet(const Expr *E) {
  // Properties are syntactic sugar for method calls
  // The AST will have already converted obj.property to [obj property]
  // So we should never hit this in the current implementation
  llvm_unreachable("Property access should be desugared to message send");
}

/// Emit code for property setter
/// Example: obj.property = value → [obj setProperty:value]
void CIRGenFunction::emitObjCPropertySet(const Expr *base, RValue value) {
  // Properties are syntactic sugar for method calls
  // The AST will have already converted obj.property = val to [obj setProperty:val]
  llvm_unreachable("Property access should be desugared to message send");
}

//===--------------------------------------------------------------------===//
// Instance Variable Access (Future work)
//===--------------------------------------------------------------------===//

/// Emit code for instance variable access
/// Example: obj->ivar
/// This would use the cir.objc.ivar operation we defined, but requires
/// more infrastructure to calculate ivar offsets
LValue CIRGenFunction::emitObjCIvarRefLValue(const ObjCIvarRefExpr *E) {
  // TODO: Implement this using cir.objc.ivar operation
  // For now, fall back to base implementation
  (void)E;
  llvm_unreachable("ObjC ivar access not yet implemented in ClangIR");
}

//===--------------------------------------------------------------------===//
// Literals (Future work - String literals, collection literals, boxing)
//===--------------------------------------------------------------------===//

/// Emit code for @"string" literals
/// Example: NSString *s = @"hello";
mlir::Value CIRGenFunction::emitObjCStringLiteral(const ObjCStringLiteral *E) {
  // TODO: Implement using cir.objc.string operation
  // For now, not implemented
  (void)E;
  llvm_unreachable("ObjC string literals not yet implemented in ClangIR");
}

/// Emit code for @[] array literals
/// Example: NSArray *arr = @[obj1, obj2];
mlir::Value CIRGenFunction::emitObjCArrayLiteral(const ObjCArrayLiteral *E) {
  // TODO: Implement using cir.objc.array operation
  (void)E;
  llvm_unreachable("ObjC array literals not yet implemented in ClangIR");
}

/// Emit code for @{} dictionary literals
/// Example: NSDictionary *dict = @{key: value};
mlir::Value CIRGenFunction::emitObjCDictionaryLiteral(const ObjCDictionaryLiteral *E) {
  // TODO: Implement using cir.objc.dictionary operation
  (void)E;
  llvm_unreachable("ObjC dictionary literals not yet implemented in ClangIR");
}

/// Emit code for @(expr) boxing expressions
/// Example: NSNumber *num = @(42);
mlir::Value CIRGenFunction::emitObjCBoxedExpr(const ObjCBoxedExpr *E) {
  // TODO: Implement using cir.objc.box operation
  (void)E;
  llvm_unreachable("ObjC boxed expressions not yet implemented in ClangIR");
}

//===--------------------------------------------------------------------===//
// ARC / Memory Management (Future work)
//===--------------------------------------------------------------------===//

/// Emit retain operation for ARC
/// Example: implicit retain when assigning to strong reference
mlir::Value CIRGenFunction::emitObjCRetainExpr(const Expr *E) {
  // TODO: Implement ARC retain/release operations
  // For now, manual memory management only
  (void)E;
  llvm_unreachable("ARC not yet implemented in ClangIR");
}

//===--------------------------------------------------------------------===//
// Exception Handling (Future work)
//===--------------------------------------------------------------------===//

/// Emit code for @try statement
void CIRGenFunction::emitObjCAtTryStmt(const ObjCAtTryStmt &S) {
  // TODO: Implement using cir.objc.try/catch operations
  (void)S;
  llvm_unreachable("ObjC exception handling not yet implemented in ClangIR");
}

/// Emit code for @throw statement
void CIRGenFunction::emitObjCAtThrowStmt(const ObjCAtThrowStmt &S) {
  // TODO: Implement using cir.objc.throw operation
  (void)S;
  llvm_unreachable("ObjC @throw not yet implemented in ClangIR");
}

//===--------------------------------------------------------------------===//
// Fast Enumeration (Future work)
//===--------------------------------------------------------------------===//

/// Emit code for for-in loops (fast enumeration)
/// Example: for (id obj in collection) { ... }
void CIRGenFunction::emitObjCForCollectionStmt(const ObjCForCollectionStmt &S) {
  // TODO: Implement using cir.objc.for_in operation
  (void)S;
  llvm_unreachable("ObjC fast enumeration not yet implemented in ClangIR");
}

//===--------------------------------------------------------------------===//
// Synchronization (Future work)
//===--------------------------------------------------------------------===//

/// Emit code for @synchronized blocks
/// Example: @synchronized(obj) { ... }
void CIRGenFunction::emitObjCAtSynchronizedStmt(const ObjCAtSynchronizedStmt &S) {
  // TODO: Implement using cir.objc.synchronized operation
  (void)S;
  llvm_unreachable("ObjC @synchronized not yet implemented in ClangIR");
}

//===--------------------------------------------------------------------===//
// Summary of Implementation Status
//===--------------------------------------------------------------------===//
//
// ✅ IMPLEMENTED (MVP - Message Sending):
//   - Instance messages: [obj method:arg]
//   - Class messages: [MyClass alloc]
//   - Super messages: [super dealloc]
//   - Preserves receiver types (NSString* vs id)
//   - Preserves selector names
//   - All the foundation for optimization!
//
// ❌ NOT YET IMPLEMENTED (Future work):
//   - Instance variable access (obj->ivar)
//   - Properties (obj.property)
//   - String literals (@"string")
//   - Collection literals (@[], @{})
//   - Boxing (@(expr))
//   - ARC (retain/release)
//   - Exception handling (@try/@catch/@throw)
//   - Fast enumeration (for-in)
//   - Synchronization (@synchronized)
//   - Blocks (^{ })
//
// 🎯 OPTIMIZATION OPPORTUNITIES PRESERVED:
//   1. Devirtualization - receiver type + selector name enables this
//   2. Super call devirtualization - statically resolvable!
//   3. Selector hoisting - can identify repeated selectors
//   4. Class reference caching - can identify repeated class lookups
//   5. Alloc/init pattern recognition - can see [X alloc] followed by [init]
//   6. Nil-receiver optimization - can track null checks
//   7. Cross-language optimization - could interface with Swift SIL
//
// This is just the beginning - the foundation is here to build ALL of these!
//
//===----------------------------------------------------------------------===//
