# Debug

## Step by step

Example:

```c++
// test1.cpp
template<typename T> int test(T x) {
  return x;
}
template int test<int>(int);

// test2.cpp
template<typename T> int test(T x);
int main() {
  return test(1);
}
```

Debug command:

```shell
llvm-link test1.ll test2.ll -o merge.bc
```

Entry: llvm/tools/llvm-link/llvm-link.cpp, main

Create the composite module:

```c++
auto Composite = std::make_unique<Module>("llvm-link", Context);
```

Create the linker:

```c++
Linker L(*Composite);
```

Related comment:

```c++
/// This class provides the core functionality of linking in LLVM. It keeps a
/// pointer to the merged module so far. It doesn't take ownership of the
/// module since it is assumed that the user of this class will want to do
/// something with it after the linking.
class Linker {
  IRMover Mover;
```

Call `linkFiles` to link input files:

```c++
  // First add all the regular input files
  if (!linkFiles(argv[0], Context, L, InputFilenames, Flags))
    return 1;
```

For each file, read its module:

```c++
for (const auto &File : Files) {
  auto BufferOrErr = MemoryBuffer::getFileOrSTDIN(File);

  // When we encounter a missing file, make sure we expose its name.
  if (auto EC = BufferOrErr.getError())
    if (EC == std::errc::no_such_file_or_directory)
      ExitOnErr(createStringError(EC, "No such file or directory: '%s'",
                                  File.c_str()));

  std::unique_ptr<MemoryBuffer> Buffer =
      ExitOnErr(errorOrToExpected(std::move(BufferOrErr)));

  std::unique_ptr<Module> M =
      identify_magic(Buffer->getBuffer()) == file_magic::archive
          ? loadArFile(argv0, std::move(Buffer), Context)
          : loadFile(argv0, std::move(Buffer), Context);
```

Call `Linker::linkInModule` to link input files:

```c++
    } else {
      Err = L.linkInModule(std::move(M), ApplicableFlags);
    }
```

Related comment:

```c++
  /// Link \p Src into the composite.
  ///
  /// Passing OverrideSymbols as true will have symbols from Src
  /// shadow those in the Dest.
  ///
  /// Passing InternalizeCallback will have the linker call the function with
  /// the new module and a list of global value names to be internalized by the
  /// callback.
  ///
  /// Returns true on error.
  bool linkInModule(std::unique_ptr<Module> Src, unsigned Flags = Flags::None,
                    std::function<void(Module &, const StringSet<> &)>
                        InternalizeCallback = {});
```

Call `ModuleLinker::run`:

```c++
bool Linker::linkInModule(
    std::unique_ptr<Module> Src, unsigned Flags,
    std::function<void(Module &, const StringSet<> &)> InternalizeCallback) {
  ModuleLinker ModLinker(Mover, std::move(Src), Flags,
                         std::move(InternalizeCallback));
  return ModLinker.run();
}
```

The comment of `ModuleLinker`

```c++
/// This is an implementation class for the LinkModules function, which is the
/// entrypoint for this file.
class ModuleLinker {
  IRMover &Mover;
```

Traverse comdat symtab:

```c++
for (const auto &SMEC : SrcM->getComdatSymbolTable()) {
```

test1.ll's comdat:

```c++
$_Z4testIiEiT_ = comdat any
```

test2.ll's comdat:

```c++
none
```

Debug print:

```shell
(gdb) print C.getName()
$8 = {static npos = 18446744073709551615, Data = 0x5555555dbd70 "_Z4testIiEiT_", Length = 13}
```

The comment of `Comdat`:

```c++
// This is a Name X SelectionKind pair. The reason for having this be an
// independent object instead of just adding the name and the SelectionKind
// to a GlobalObject is that it is invalid to have two Comdats with the same
// name but different SelectionKind. This structure makes that unrepresentable.
class Comdat {
public:
  enum SelectionKind {
    Any,           ///< The linker may choose any COMDAT.
    ExactMatch,    ///< The data referenced by the COMDAT must be the same.
    Largest,       ///< The linker will choose the largest COMDAT.
    NoDeduplicate, ///< No deduplication is performed.
    SameSize,      ///< The data referenced by the COMDAT must be the same size.
  };

  Comdat(const Comdat &) = delete;
  Comdat(Comdat &&C);

  SelectionKind getSelectionKind() const { return SK; }
  void setSelectionKind(SelectionKind Val) { SK = Val; }
  StringRef getName() const;
  void print(raw_ostream &OS, bool IsForDebug = false) const;
  void dump() const;
  const SmallPtrSetImpl<GlobalObject *> &getUsers() const { return Users; }
```

Get `SelectionKind`:

```c++
    Comdat::SelectionKind SK;
    LinkFrom From;
    if (getComdatResult(&C, SK, From))
      return true;
```

Debug print:

```shell
(gdb) print SK
$9 = llvm::Comdat::Any
```

Here we link `SrcM` to `DstM`, If a `Comdat C` in `SrcM` can be found in `DstM`, we will replace the dest `Comdat` to src `Comdat` (add it into `ReplacedDstComdats`):

```c++
    if (From != LinkFrom::Src)
      continue;

    Module::ComdatSymTabType &ComdatSymTab = DstM.getComdatSymbolTable();
    Module::ComdatSymTabType::iterator DstCI = ComdatSymTab.find(C.getName());
    if (DstCI == ComdatSymTab.end())
      continue;

    // The source comdat is replacing the dest one.
    const Comdat *DstC = &DstCI->second;
    ReplacedDstComdats.insert(DstC);
```

Drop and replace comdat (we will debug these code detail later):

```c++
// Alias have to go first, since we are not able to find their comdats
// otherwise.
for (GlobalAlias &GV : llvm::make_early_inc_range(DstM.aliases()))
  dropReplacedComdat(GV, ReplacedDstComdats);

for (GlobalVariable &GV : llvm::make_early_inc_range(DstM.globals()))
  dropReplacedComdat(GV, ReplacedDstComdats);

for (Function &GV : llvm::make_early_inc_range(DstM))
  dropReplacedComdat(GV, ReplacedDstComdats);
```

Handle link once:

```c++
for (GlobalVariable &GV : SrcM->globals())
  if (GV.hasLinkOnceLinkage())
    if (const Comdat *SC = GV.getComdat())
      LazyComdatMembers[SC].push_back(&GV);

for (Function &SF : *SrcM)
  if (SF.hasLinkOnceLinkage())
    if (const Comdat *SC = SF.getComdat())
      LazyComdatMembers[SC].push_back(&SF);

for (GlobalAlias &GA : SrcM->aliases())
  if (GA.hasLinkOnceLinkage())
    if (const Comdat *SC = GA.getComdat())
      LazyComdatMembers[SC].push_back(&GA);
```

Traverse functions, call `linkIfNeeded`, it will update `ValuesToLink`:

```c++
// Insert all of the globals in src into the DstM module... without linking
// initializers (which could refer to functions not yet mapped over).
SmallVector<GlobalValue *, 0> GVToClone;
for (GlobalVariable &GV : SrcM->globals())
  if (linkIfNeeded(GV, GVToClone))
    return true;

for (Function &SF : *SrcM)
  if (linkIfNeeded(SF, GVToClone))
    return true;

for (GlobalAlias &GA : SrcM->aliases())
  if (linkIfNeeded(GA, GVToClone))
    return true;
```

Important function: `getLinkedToGlobal`, it will match src value to dest value (if exists):

```c++
bool ModuleLinker::linkIfNeeded(GlobalValue &GV,
                                SmallVectorImpl<GlobalValue *> &GVToClone) {
  GlobalValue *DGV = getLinkedToGlobal(&GV);
```

For this simple example, it just return nullptr:

```c++
/// Given a global in the source module, return the global in the
/// destination module that is being linked to, if any.
GlobalValue *getLinkedToGlobal(const GlobalValue *SrcGV) {
  Module &DstM = Mover.getModule();
  // If the source has no name it can't link.  If it has local linkage,
  // there is no name match-up going on.
  if (!SrcGV->hasName() || GlobalValue::isLocalLinkage(SrcGV->getLinkage()))
    return nullptr;

  // Otherwise see if we have a match in the destination module's symtab.
  GlobalValue *DGV = DstM.getNamedValue(SrcGV->getName());
  if (!DGV)
    return nullptr;

  // If we found a global with the same name in the dest module, but it has
  // internal linkage, we are really not doing any linkage here.
  if (DGV->hasLocalLinkage())
    return nullptr;

  // Otherwise, we do in fact link to the destination global.
  return DGV;
}
```

Update `ValuesToLink`:

```c++
if (LinkFromSrc)
  ValuesToLink.insert(&GV);
return false;
```

Return to `ModuleLinker::run`.

Call `IRMover::move`:

```c++
bool HasErrors = false;
if (Error E =
        Mover.move(std::move(SrcM), ValuesToLink.getArrayRef(),
                   IRMover::LazyCallback(
                       [this](GlobalValue &GV, IRMover::ValueAdder Add) {
                         addLazyFor(GV, Add);
                       }),
                   /* IsPerformingImport */ false)) {
  handleAllErrors(std::move(E), [&](ErrorInfoBase &EIB) {
    DstM.getContext().diagnose(LinkDiagnosticInfo(DS_Error, EIB.message()));
    HasErrors = true;
  });
}
```

Related comment:

```c++
/// Move in the provide values in \p ValuesToLink from \p Src.
///
/// - \p AddLazyFor is a call back that the IRMover will call when a global
///   value is referenced by one of the ValuesToLink (transitively) but was
///   not present in ValuesToLink. The GlobalValue and a ValueAdder callback
///   are passed as an argument, and the callback is expected to be called
///   if the GlobalValue needs to be added to the \p ValuesToLink and linked.
///   Pass nullptr if there's no work to be done in such cases.
/// - \p IsPerformingImport is true when this IR link is to perform ThinLTO
///   function importing from Src.
Error move(std::unique_ptr<Module> Src, ArrayRef<GlobalValue *> ValuesToLink,
           LazyCallback AddLazyFor, bool IsPerformingImport);
```

Call `IRLinker::run`:

```c++
Error IRMover::move(std::unique_ptr<Module> Src,
                    ArrayRef<GlobalValue *> ValuesToLink,
                    LazyCallback AddLazyFor, bool IsPerformingImport) {
  IRLinker TheIRLinker(Composite, SharedMDs, IdentifiedStructTypes,
                       std::move(Src), ValuesToLink, std::move(AddLazyFor),
                       IsPerformingImport);
  Error E = TheIRLinker.run();
  Composite.dropTriviallyDeadConstantArrays();
  return E;
}
```

Call `mapValue` to link:

```c++
std::reverse(Worklist.begin(), Worklist.end());
while (!Worklist.empty()) {
  GlobalValue *GV = Worklist.back();
  Worklist.pop_back();

  // Already mapped.
  if (ValueMap.find(GV) != ValueMap.end() ||
      IndirectSymbolValueMap.find(GV) != IndirectSymbolValueMap.end())
    continue;

  assert(!GV->isDeclaration());
  Mapper.mapValue(*GV);
  if (FoundError)
    return std::move(*FoundError);
  flushRAUWWorklist();
}
```

## Summary

```
llvm/tools/llvm-link/llvm-link.cpp, main
|linkFiles
||Linker::linkInModule
|||ModuleLinker::run
||||linkIfNeeded
||||IRMover::move
|||||IRLinker::run
||||||mapValue
```
