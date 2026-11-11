//===- ObjCMethodLists.cpp ------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Discover Objective-C method lists for relative encoding by following the
// references in ObjC metadata, instead of matching compiler-private symbol
// names. A method list is converted only if it is reachable from a class,
// metaclass, category, class_ro_t or (opt-in) protocol root, has the exact
// layout of an absolute method_list_t, and occupies a whole input section that
// nothing else references at an interior offset. Anything else stays absolute.
//
//===----------------------------------------------------------------------===//

#include "Config.h"
#include "InputFiles.h"
#include "InputSection.h"
#include "ObjC.h"
#include "OutputSegment.h"
#include "Symbols.h"
#include "SyntheticSections.h"
#include "Target.h"

#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/Support/Endian.h"

using namespace llvm;
using namespace llvm::support::endian;
using namespace lld;
using namespace lld::macho;

namespace {
// A location inside an input section: (section, byte offset).
using Ref = std::pair<ConcatInputSection *, uint64_t>;

class MethodListCollector {
public:
  void run();

private:
  std::pair<InputSection *, uint64_t> targetOf(const Relocation &r) const;
  bool isPlainPointer(const Relocation *r) const;
  const Relocation *relocAt(const ConcatInputSection *isec,
                            uint64_t offset) const;
  Ref follow(Ref from, uint64_t field) const;
  void visitClass(Ref cls);
  void visitClassRO(Ref ro);
  void visitCategory(Ref cat, bool followClass);
  void visitProtocol(Ref proto);
  void visitProtocolList(Ref list);
  void addCandidate(Ref list, bool isProtocol = false);
  bool isEligible(const ConcatInputSection *isec, bool isProtocol) const;
  void dropInteriorReferences();
  void moveToMethListSection();

  const uint64_t wordSize = target->wordSize;
  DenseSet<Ref> visited;
  SetVector<ConcatInputSection *> lists;
};
} // namespace

// Resolve a relocation to (section, offset), following symbol definitions.
std::pair<InputSection *, uint64_t>
MethodListCollector::targetOf(const Relocation &r) const {
  InputSection *isec;
  int64_t offset = r.getAddend();
  if (auto *sym = dyn_cast<Symbol *>(r.referent)) {
    auto *def = dyn_cast<Defined>(sym);
    if (!def || !def->isec())
      return {};
    isec = def->isec();
    offset += def->value;
  } else {
    isec = cast<InputSection *>(r.referent);
  }
  if (offset < 0 || static_cast<uint64_t>(offset) > isec->data.size())
    return {};
  return {isec, offset};
}

// A pointer-sized absolute pointer the relative encoder can rewrite: it must
// not need dyld binding, since a relative offset cannot be bound.
bool MethodListCollector::isPlainPointer(const Relocation *r) const {
  if (!r || r->pcrel || r->length != target->p2WordSize ||
      !target->hasAttr(r->type, RelocAttrBits::UNSIGNED))
    return false;
  if (auto *sym = dyn_cast<Symbol *>(r->referent))
    return isa<Defined>(sym) && !needsBinding(sym);
  return true;
}

const Relocation *MethodListCollector::relocAt(const ConcatInputSection *isec,
                                               uint64_t offset) const {
  const Relocation *found = nullptr;
  for (const Relocation &r : isec->relocs) {
    if (r.offset != offset)
      continue;
    if (found)
      return nullptr; // Paired or duplicate relocations: not a plain pointer.
    found = &r;
  }
  return found;
}

// Follow the pointer stored at `from + field` to a live ConcatInputSection.
Ref MethodListCollector::follow(Ref from, uint64_t field) const {
  auto [isec, offset] = from;
  if (!isec || offset + field + wordSize > isec->data.size())
    return {};
  const Relocation *r = relocAt(isec, offset + field);
  if (!r || r->pcrel || r->length != target->p2WordSize ||
      !target->hasAttr(r->type, RelocAttrBits::UNSIGNED))
    return {};
  // Roots may point at a locally defined weak symbol whose final pointer is
  // bound by dyld; following the local definition is still correct.
  auto [dest, destOffset] = targetOf(*r);
  auto *concat = dyn_cast_or_null<ConcatInputSection>(dest);
  if (!concat || concat->shouldOmitFromOutput())
    return {};
  return {concat, destOffset};
}

void MethodListCollector::visitClass(Ref cls) {
  if (!cls.first || !visited.insert(cls).second)
    return;
  // class_t: isa, superclass, cache, vtable, data (class_ro_t | flags).
  Ref ro = follow(cls, 4 * wordSize);
  if (ro.first) {
    // The runtime and Swift keep flags in the low bits of the data pointer.
    ro.second &= ~(wordSize - 1);
    visitClassRO(ro);
  }
  visitClass(follow(cls, 0)); // The metaclass holds the class methods.
}

void MethodListCollector::visitClassRO(Ref ro) {
  if (!ro.first || !visited.insert(ro).second)
    return;
  // class_ro_t: flags, instanceStart, instanceSize, [reserved on LP64],
  // ivarLayout, name, baseMethods, ...
  uint64_t header = wordSize == 8 ? 16 : 12;
  addCandidate(follow(ro, header + 2 * wordSize));
  visitProtocolList(follow(ro, header + 3 * wordSize));
}

void MethodListCollector::visitCategory(Ref cat, bool followClass) {
  if (!cat.first || !visited.insert(cat).second)
    return;
  // category_t: name, cls, instanceMethods, classMethods, protocols, ...
  addCandidate(follow(cat, 2 * wordSize));
  addCandidate(follow(cat, 3 * wordSize));
  visitProtocolList(follow(cat, 4 * wordSize));
  // __objc_catlist2 categories point at a Swift class stub, not a class_t.
  if (followClass)
    visitClass(follow(cat, wordSize));
}

void MethodListCollector::visitProtocol(Ref proto) {
  if (!config->enableObjCProtocolRelativeMethodLists || !proto.first ||
      !visited.insert(proto).second)
    return;
  // protocol_t: isa, name, protocols, instanceMethods, classMethods,
  // optionalInstanceMethods, optionalClassMethods, ...
  visitProtocolList(follow(proto, 2 * wordSize));
  for (uint64_t field = 3; field != 7; ++field)
    addCandidate(follow(proto, field * wordSize), /*isProtocol=*/true);
}

void MethodListCollector::visitProtocolList(Ref list) {
  auto [isec, offset] = list;
  if (!config->enableObjCProtocolRelativeMethodLists || !isec ||
      !visited.insert(list).second || !isec->data.data() ||
      offset + wordSize > isec->data.size())
    return;
  // protocol_list_t: count, protocol_t *list[count].
  uint64_t count = wordSize == 8 ? read64le(isec->data.data() + offset)
                                 : read32le(isec->data.data() + offset);
  if (count > (isec->data.size() - offset - wordSize) / wordSize)
    return;
  for (uint64_t i = 1; i <= count; ++i)
    visitProtocol(follow(list, i * wordSize));
}

void MethodListCollector::addCandidate(Ref list, bool isProtocol) {
  // Only whole sections are converted; an embedded list stays absolute.
  if (list.first && list.second == 0 && isEligible(list.first, isProtocol))
    lists.insert(list.first);
}

bool MethodListCollector::isEligible(const ConcatInputSection *isec,
                                     bool isProtocol) const {
  if (lists.contains(const_cast<ConcatInputSection *>(isec)))
    return false;
  StringRef name = isec->getName();
  if ((name != section_names::objcConst && name != section_names::objcData) ||
      sectionType(isec->getFlags()) != MachO::S_REGULAR ||
      isec->shouldOmitFromOutput() || !isec->data.data() ||
      isec->data.size() < 8)
    return false;
  // method_list_t header: entsizeAndFlags, count. Already-relative, uniqued
  // or fixed-up lists are left alone.
  uint32_t entsizeAndFlags = read32le(isec->data.data());
  uint32_t count = read32le(isec->data.data() + 4);
  uint64_t entrySize = 3 * wordSize;
  if ((entsizeAndFlags & 0xffff0003) != 0 ||
      (entsizeAndFlags & 0xfffc) != entrySize ||
      isec->data.size() != 8 + uint64_t(count) * entrySize)
    return false;
  // The encoder moves the whole section and rewrites symbol sizes; a symbol
  // inside the list would point at the wrong place afterwards.
  for (const Defined *sym : isec->symbols)
    if (sym->value != 0 || (sym->size && sym->size != isec->data.size()))
      return false;
  // Every relocation must be a plain pointer at an entry slot.
  for (const Relocation &r : isec->relocs)
    if (r.offset < 8 || (r.offset - 8) % wordSize != 0 || !isPlainPointer(&r))
      return false;
  for (uint64_t entry = 8; entry < isec->data.size(); entry += entrySize) {
    // Selector: the encoder makes a selref from the string, so it must be a
    // whole string in __objc_methname.
    const Relocation *sel = relocAt(isec, entry);
    if (!isPlainPointer(sel))
      return false;
    auto [selIsec, selOff] = targetOf(*sel);
    auto *selStrings = dyn_cast_or_null<CStringInputSection>(selIsec);
    if (!selStrings || selStrings->getName() != section_names::objcMethname ||
        selOff >= selStrings->data.size() ||
        selStrings->getStringPiece(selOff).inSecOff != selOff)
      return false;
    // Types: any C string.
    const Relocation *types = relocAt(isec, entry + wordSize);
    if (!isPlainPointer(types) ||
        !isa_and_nonnull<CStringInputSection>(targetOf(*types).first))
      return false;
    // IMP. Protocol method descriptions have a null IMP, which the encoder
    // writes as a zero relative offset.
    const Relocation *imp = relocAt(isec, entry + 2 * wordSize);
    if (isProtocol && !imp &&
        llvm::all_of(isec->data.slice(entry + 2 * wordSize, wordSize),
                     [](uint8_t b) { return b == 0; }))
      continue;
    if (!isPlainPointer(imp))
      return false;
  }
  return true;
}

// The encoder cannot remap an address inside a list, so a list referenced at
// a nonzero offset stays absolute.
void MethodListCollector::dropInteriorReferences() {
  if (lists.empty())
    return;
  DenseSet<ConcatInputSection *> drop;
  for (const ConcatInputSection *isec : inputSections) {
    if (isec->shouldOmitFromOutput())
      continue;
    for (const Relocation &r : isec->relocs) {
      auto [dest, offset] = targetOf(r);
      auto *concat = dyn_cast_or_null<ConcatInputSection>(dest);
      if (concat && offset != 0 && lists.contains(concat))
        drop.insert(concat);
    }
  }
  lists.remove_if([&](ConcatInputSection *isec) { return drop.count(isec); });
}

void MethodListCollector::moveToMethListSection() {
  // Keep the input order, like the name-based path, for deterministic output.
  llvm::erase_if(inputSections, [&](ConcatInputSection *isec) {
    if (!lists.contains(isec))
      return false;
    isec->parent = in.objcMethList;
    in.objcMethList->addInput(isec);
    in.objcMethList->inputOrder =
        std::min<int>(in.objcMethList->inputOrder, isec->outSecOff);
    return true;
  });
}

void MethodListCollector::run() {
  for (ConcatInputSection *isec : inputSections) {
    if (isec->shouldOmitFromOutput())
      continue;
    StringRef name = isec->getName();
    void (MethodListCollector::*visit)(Ref) = nullptr;
    bool category = false, followClass = true;
    if (name == section_names::objcClassList ||
        name == section_names::objcNonLazyClassList)
      visit = &MethodListCollector::visitClass;
    else if (name == section_names::objcClassROList)
      visit = &MethodListCollector::visitClassRO;
    else if (name == section_names::objcCatList ||
             name == section_names::objcNonLazyCatList)
      category = true;
    else if (name == section_names::objcCatList2)
      category = true, followClass = false;
    else if (name == section_names::objcProtoList ||
             name == section_names::objcProtoRefs)
      visit = &MethodListCollector::visitProtocol;
    else
      continue;
    for (uint64_t off = 0; off + wordSize <= isec->data.size();
         off += wordSize) {
      Ref ref = follow({isec, 0}, off);
      if (category)
        visitCategory(ref, followClass);
      else
        (this->*visit)(ref);
    }
  }
  dropInteriorReferences();
  moveToMethListSection();
}

void objc::collectMethodLists() {
  if (config->emitRelativeMethodLists &&
      config->objcMethodListMetadataDiscovery)
    MethodListCollector().run();
}
