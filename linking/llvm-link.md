# Debug

Example:

```c++
// test1.cpp
int g = 1;
class A {
public:
  int foo() { return 1; }
};
template<typename T> int test(T x) {
  const char *s = "hello";
  A a;
  return x + s[0] + g + a.foo();
}
int test2() { return 0;}
template int test<int>(int);

// test2.cpp
class A {
public:
  int foo() { return 1; }
};
template<typename T> int test(T x);
int main() {
  const char *s = "world";
  A a;
  return test(1) + s[0] + a.foo();
}
```

Debug command:

```shell
llvm-link test1.ll test2.ll -o merge.bc
```

test1.ll:

```c++
; ModuleID = 'test1.cpp'
source_filename = "test1.cpp"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-linux-gnu"

%class.A = type { i8 }

$_Z4testIiEiT_ = comdat any

$_ZN1A3fooEv = comdat any

@g = dso_local global i32 1, align 4
@.str = private unnamed_addr constant [6 x i8] c"hello\00", align 1

; Function Attrs: mustprogress noinline nounwind optnone uwtable
define dso_local noundef i32 @_Z5test2v() #0 {
  ret i32 0
}

; Function Attrs: mustprogress noinline optnone uwtable
define weak_odr dso_local noundef i32 @_Z4testIiEiT_(i32 noundef %0) #1 comdat {
  %2 = alloca i32, align 4
  %3 = alloca ptr, align 8
  %4 = alloca %class.A, align 1
  store i32 %0, ptr %2, align 4
  store ptr @.str, ptr %3, align 8
  %5 = load i32, ptr %2, align 4
  %6 = load ptr, ptr %3, align 8
  %7 = getelementptr inbounds i8, ptr %6, i64 0
  %8 = load i8, ptr %7, align 1
  %9 = sext i8 %8 to i32
  %10 = add nsw i32 %5, %9
  %11 = load i32, ptr @g, align 4
  %12 = add nsw i32 %10, %11
  %13 = call noundef i32 @_ZN1A3fooEv(ptr noundef nonnull align 1 dereferenceable(1) %4)
  %14 = add nsw i32 %12, %13
  ret i32 %14
}

; Function Attrs: mustprogress noinline nounwind optnone uwtable
define linkonce_odr dso_local noundef i32 @_ZN1A3fooEv(ptr noundef nonnull align 1 dereferenceable(1) %0) #0 comdat align 2 {
  %2 = alloca ptr, align 8
  store ptr %0, ptr %2, align 8
  %3 = load ptr, ptr %2, align 8
  ret i32 1
}

attributes #0 = { mustprogress noinline nounwind optnone uwtable "frame-pointer"="all" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #1 = { mustprogress noinline optnone uwtable "frame-pointer"="all" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }

!llvm.module.flags = !{!0, !1, !2, !3, !4}
!llvm.ident = !{!5}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 8, !"PIC Level", i32 2}
!2 = !{i32 7, !"PIE Level", i32 2}
!3 = !{i32 7, !"uwtable", i32 2}
!4 = !{i32 7, !"frame-pointer", i32 2}
!5 = !{!"Deepin clang version 17.0.6 (5deepin4)"}
```

test2.ll:

```c++
; ModuleID = 'test2.cpp'
source_filename = "test2.cpp"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-linux-gnu"

%class.A = type { i8 }

$_ZN1A3fooEv = comdat any

@.str = private unnamed_addr constant [6 x i8] c"world\00", align 1

; Function Attrs: mustprogress noinline norecurse optnone uwtable
define dso_local noundef i32 @main() #0 {
  %1 = alloca i32, align 4
  %2 = alloca ptr, align 8
  %3 = alloca %class.A, align 1
  store i32 0, ptr %1, align 4
  store ptr @.str, ptr %2, align 8
  %4 = call noundef i32 @_Z4testIiEiT_(i32 noundef 1)
  %5 = load ptr, ptr %2, align 8
  %6 = getelementptr inbounds i8, ptr %5, i64 0
  %7 = load i8, ptr %6, align 1
  %8 = sext i8 %7 to i32
  %9 = add nsw i32 %4, %8
  %10 = call noundef i32 @_ZN1A3fooEv(ptr noundef nonnull align 1 dereferenceable(1) %3)
  %11 = add nsw i32 %9, %10
  ret i32 %11
}

declare noundef i32 @_Z4testIiEiT_(i32 noundef) #1

; Function Attrs: mustprogress noinline nounwind optnone uwtable
define linkonce_odr dso_local noundef i32 @_ZN1A3fooEv(ptr noundef nonnull align 1 dereferenceable(1) %0) #2 comdat align 2 {
  %2 = alloca ptr, align 8
  store ptr %0, ptr %2, align 8
  %3 = load ptr, ptr %2, align 8
  ret i32 1
}

attributes #0 = { mustprogress noinline norecurse optnone uwtable "frame-pointer"="all" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #1 = { "frame-pointer"="all" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #2 = { mustprogress noinline nounwind optnone uwtable "frame-pointer"="all" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }

!llvm.module.flags = !{!0, !1, !2, !3, !4}
!llvm.ident = !{!5}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 8, !"PIC Level", i32 2}
!2 = !{i32 7, !"PIE Level", i32 2}
!3 = !{i32 7, !"uwtable", i32 2}
!4 = !{i32 7, !"frame-pointer", i32 2}
!5 = !{!"Deepin clang version 17.0.6 (5deepin4)"}
```

Entry: 

```shell
llvm/tools/llvm-link/llvm-link.cpp, main
```

```c++
int main(int argc, char **argv) {
  // 1. Init.
  // 1.1 Init LLVM.
  InitLLVM X(argc, argv);
  ...
  // 1.2 Create the composite module, we will link all the input modules to this moudle.
  auto Composite = std::make_unique<Module>("llvm-link", Context);
  // 1.3 Create the linker.
  Linker L(*Composite);
  ...
  // 2. Link all the input modules to Composite.
  if (!linkFiles(argv[0], Context, L, InputFilenames, Flags))
    return 1;
  ...
  // 3. Write Composite to bc.
  std::error_code EC;
  // TooOutputFile can automatically delete the output file if the process is killed.
  ToolOutputFile Out(OutputFilename, EC,
                     OutputAssembly ? sys::fs::OF_TextWithCRLF
                                    : sys::fs::OF_None);
  ...
  else if (Force || !CheckBitcodeOutputToConsole(Out.os())) {
    SetFormat(UseNewDbgInfoFormat && WriteNewDbgInfoFormatToBitcode);
    WriteBitcodeToFile(*Composite, Out.os(), PreserveBitcodeUseListOrder);
  }
  
  // Declare success.
  // Otherwise TooOutputFile will delete the output file automatically.
  Out.keep();
  
  return 0;
}
```

The structure of `Linker`:

```shell
llvm/include/llvm/Linker/Linker.h
```

```c++
class Linker(Composite) {
    class IRMover Mover(Composite) {
        class Module Composite(Composite);
    }
}
```

Link all the input modules to Composite:

```shell
llvm/tools/llvm-link/llvm-link.cpp, main
|--llvm/tools/llvm-link/llvm-link.cpp, linkFiles
```

```c++
static bool linkFiles(const char *argv0, LLVMContext &Context, Linker &L,
                      const cl::list<std::string> &Files, unsigned Flags) {
  ...
  for (const auto &File : Files) {
    // 1. Read bc file.
    // 1.1 Open the input file as a MemoryBuffer.
    auto BufferOrErr = MemoryBuffer::getFileOrSTDIN(File);
	...
    std::unique_ptr<MemoryBuffer> Buffer =
        ExitOnErr(errorOrToExpected(std::move(BufferOrErr)));
	// 1.2 Load module from the input buffer.
    std::unique_ptr<Module> M =
        identify_magic(Buffer->getBuffer()) == file_magic::archive
            ? loadArFile(argv0, std::move(Buffer), Context)
            : loadFile(argv0, std::move(Buffer), Context);
    if (!M) {
      errs() << argv0 << ": ";
      WithColor::error() << " loading file '" << File << "'\n";
      return false;
    }
	...
    else {
      // 2. Link all the input modules to the dest module (Composite).
      Err = L.linkInModule(std::move(M), ApplicableFlags);
    }
    ...
  }

  return true;
}
```

Link all the input modules to the dest module (Composite):

```shell
|--llvm/tools/llvm-link/llvm-link.cpp, linkFiles
|--|--llvm/lib/Linker/LinkModules.cpp, linkInModule
```

```c++
bool Linker::linkInModule(
    std::unique_ptr<Module> Src, unsigned Flags,
    std::function<void(Module &, const StringSet<> &)> InternalizeCallback) {
  ModuleLinker ModLinker(Mover, std::move(Src), Flags,
                         std::move(InternalizeCallback));
  return ModLinker.run();
}
```

The structure of `ModuleLinker`:

```shell
llvm/lib/Linker/LinkModules.cpp
```

```c++
class ModuleLinker(Mover, Src) {
  class IRMover Mover(Mover);
  class Module SrcM(Src);
}
```

`ModLinker.run()`:

```shell
|--|--llvm/lib/Linker/LinkModules.cpp, linkInModule
|--|--|--llvm/lib/Linker/LinkModules.cpp, ModuleLinker::run
```

```c++
bool ModuleLinker::run() {
  Module &DstM = Mover.getModule();
  DenseSet<const Comdat *> ReplacedDstComdats;
  DenseSet<const Comdat *> NonPrevailingComdats;

  // 1. Handle comdat conflict according to the selection kind (Any, ExactMatch, Largest, NoDeduplicate, SameSize). 
  for (const auto &SMEC : SrcM->getComdatSymbolTable()) {
    const Comdat &C = SMEC.getValue();
    if (ComdatsChosen.count(&C))
      continue;
    Comdat::SelectionKind SK;
    LinkFrom From;
    // Determine the comdat should come from source or dest.
    if (getComdatResult(&C, SK, From))
      return true;
    ComdatsChosen[&C] = std::make_pair(SK, From);

    if (From == LinkFrom::Dst)
      NonPrevailingComdats.insert(&C);

    if (From != LinkFrom::Src)
      continue;

    Module::ComdatSymTabType &ComdatSymTab = DstM.getComdatSymbolTable();
    Module::ComdatSymTabType::iterator DstCI = ComdatSymTab.find(C.getName());
    if (DstCI == ComdatSymTab.end())
      continue;

    // The source comdat is replacing the dest one.
    const Comdat *DstC = &DstCI->second;
    ReplacedDstComdats.insert(DstC);
  }

  
  // Replace dest with source.
  for (GlobalAlias &GV : llvm::make_early_inc_range(DstM.aliases()))
    dropReplacedComdat(GV, ReplacedDstComdats);

  for (GlobalVariable &GV : llvm::make_early_inc_range(DstM.globals()))
    dropReplacedComdat(GV, ReplacedDstComdats);

  for (Function &GV : llvm::make_early_inc_range(DstM))
    dropReplacedComdat(GV, ReplacedDstComdats);
  ...
  // 2. Determine which symbols need to be linked, add them to ValuesToLink.
  // (1) Add the global definitions from source to dest. local / weak / UND symbols will not be considered for now, they will be added later with the dependencies of global definitions.
  // (2) For a conflict weak (linkonce) symbol, if it comes from dest, we will not add it to ValuesToLink.
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

  for (GlobalIFunc &GI : SrcM->ifuncs())
    if (linkIfNeeded(GI, GVToClone))
      return true;
  ...
  // 3. Copy the symbols in ValuesToLink from source to dest.
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
  ...
  return false;
}
```

`Mover.move`:

```shell
|--|--|--llvm/lib/Linker/LinkModules.cpp, ModuleLinker::run
|--|--|--|--llvm/lib/Linker/IRMover.cpp
```

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

The structure of `IRLinker`:

```shell
llvm/lib/Linker/IRMover.cpp
```

```c++
//IRLinker(Module &DstM, MDMapT &SharedMDs,
//         IRMover::IdentifiedStructTypeSet &Set, std::unique_ptr<Module> SrcM,
//         ArrayRef<GlobalValue *> ValuesToLink,
//         IRMover::LazyCallback AddLazyFor, bool IsPerformingImport)
//    : DstM(DstM), SrcM(std::move(SrcM)), AddLazyFor(std::move(AddLazyFor)),
//      TypeMap(Set), GValMaterializer(*this), LValMaterializer(*this),
//      SharedMDs(SharedMDs), IsPerformingImport(IsPerformingImport),
//      Mapper(ValueMap, RF_ReuseAndMutateDistinctMDs | RF_IgnoreMissingLocals,
//             &TypeMap, &GValMaterializer),
//      IndirectSymbolMCID(Mapper.registerAlternateMappingContext(
//          IndirectSymbolValueMap, &LValMaterializer)) {
//  ValueMap.getMDMap() = std::move(SharedMDs);
//  for (GlobalValue *GV : ValuesToLink)
//    maybeAdd(GV);
//  if (IsPerformingImport)
//    prepareCompileUnitsForImport();
//}
class IRLinker(Composite, Src, ValuesToLink) {
	class Module DstM(Composite);
  	class Module SrcM(Src);
    // src value -> dest value.
  	class ValueToValueMapTy ValueMap({});
    // src type -> dest type.
    class TypeMapTy TypeMap({});
    std::vector class GlobalValue Worklist(ValuesToLink);
    class GlobalValueMaterializer GValMaterializer(this) {
        class IRLinker TheIRLinker(this);
    }
    class ValueMapper Mapper(ValueMap, TypeMap, GValMaterializer) {
        class Mapper pImpl(ValueMap, TypeMap, GValMaterializer) {
            class ValueMapTypeRemapper TypeMapper(TypeMap);
  			SmallVector class MappingContext MCs(ValueMap, GValMaterializer);
        }
    }
}
```

`TheIRLinker.run`:

```shell
|--|--|--|--llvm/lib/Linker/IRMover.cpp
|--|--|--|--|--llvm/lib/Linker/IRMover.cpp, IRLinker::run
```

```c++
Error IRLinker::run() {
  ...
  // 1. Update data layout, triple.
  if (DstM.getDataLayout().isDefault())
    DstM.setDataLayout(SrcM->getDataLayout());
  if (DstM.getTargetTriple().empty() && !SrcM->getTargetTriple().empty())
    DstM.setTargetTriple(SrcM->getTargetTriple());
  Triple SrcTriple(SrcM->getTargetTriple()), DstTriple(DstM.getTargetTriple());
  ...
  DstM.setTargetTriple(SrcTriple.merge(DstTriple));
  // 2. Map src type to dest type, update TypeMap.
  // (1) For a matched symbol pair (src Value, dest Value), map their type.
  // (2) For a named type, map according to its name.
  // (3) For a matched type (src: DEF, dest: UND), link the type body from src to dest.
  computeTypeMapping();
  // Worklist is first-in-last-out, so we reverse it here to ensure that the procssing order is consistent with the value appearance order in src bc.
  std::reverse(Worklist.begin(), Worklist.end());
  while (!Worklist.empty()) {
    GlobalValue *GV = Worklist.back();
    Worklist.pop_back();

    // Already mapped.
    if (ValueMap.find(GV) != ValueMap.end() ||
        IndirectSymbolValueMap.find(GV) != IndirectSymbolValueMap.end())
      continue;

    assert(!GV->isDeclaration());
    // 3. Copy GV from src to dest.
    Mapper.mapValue(*GV);
    if (FoundError)
      return std::move(*FoundError);
    flushRAUWWorklist();
  }
  ...
  // Merge the module flags into the DstM module.
  return linkModuleFlagsMetadata();
}
```

`Mapper.mapValue`:

```shell
|--|--|--|--|--llvm/lib/Linker/IRMover.cpp, IRLinker::run
|--|--|--|--|--|--llvm/lib/Transforms/Utils/ValueMapper.cpp, ValueMapper::mapValue
```

```c++
Value *ValueMapper::mapValue(const Value &V) {
  // 1. mapValue will copy V from src to dest (without its dependencies),
  // 2. ~FlushingMapper() will copy dependencies of V from src to dest and update the releated symbol references.
  return FlushingMapper(pImpl)->mapValue(&V);
}
```

`mapValue`:

```shell
|--|--|--|--|--|--llvm/lib/Transforms/Utils/ValueMapper.cpp, ValueMapper::mapValue
|--|--|--|--|--|--|--llvm/lib/Transforms/Utils/ValueMapper.cpp, Mapper::mapValue
```

```c++
Value *Mapper::mapValue(const Value *V) {
  ValueToValueMapTy::iterator I = getVM().find(V);

  // If the value already exists in the map, use it.
  if (I != getVM().end()) {
    assert(I->second && "Unexpected null mapping");
    return I->second;
  }

  // If we have a materializer and it can materialize a value, use that.
  if (auto *Materializer = getMaterializer()) {
    // 1. Copy V from src to dest.
    if (Value *NewV = Materializer->materialize(const_cast<Value *>(V))) {
      // 2. Update ValueMap.
      getVM()[V] = NewV;
      return NewV;
    }
  }
  ...
```

`Materializer->materialize`:

```shell
|--|--|--|--|--|--|--llvm/lib/Transforms/Utils/ValueMapper.cpp, Mapper::mapValue
|--|--|--|--|--|--|--|--llvm/lib/Linker/IRMover.cpp, GlobalValueMaterializer::materialize
```

```c++
Value *LocalValueMaterializer::materialize(Value *SGV) {
  return TheIRLinker.materialize(SGV, true);
}
```

`TheIRLinker.materialize`:

```shell
|--|--|--|--|--|--|--|--llvm/lib/Linker/IRMover.cpp, GlobalValueMaterializer::materialize
|--|--|--|--|--|--|--|--|--llvm/lib/Linker/IRMover.cpp, IRLinker::materialize
```

```c++
Value *IRLinker::materialize(Value *V, bool ForIndirectSymbol) {
  auto *SGV = dyn_cast<GlobalValue>(V);
  if (!SGV)
    return nullptr;
  ... 
  // 1. Link decl.
  Expected<Constant *> NewProto = linkGlobalValueProto(SGV, ForIndirectSymbol);
  if (!NewProto) {
    setError(NewProto.takeError());
    return nullptr;
  }
  if (!*NewProto)
    return nullptr;

  GlobalValue *New = dyn_cast<GlobalValue>(*NewProto);
  if (!New)
    return *NewProto;

  // If we already created the body, just return.
  if (auto *F = dyn_cast<Function>(New)) {
    if (!F->isDeclaration())
      return New;
  } else if (auto *V = dyn_cast<GlobalVariable>(New)) {
    if (V->hasInitializer() || V->hasAppendingLinkage())
      return New;
  } else if (auto *GA = dyn_cast<GlobalAlias>(New)) {
    if (GA->getAliasee())
      return New;
  } else if (auto *GI = dyn_cast<GlobalIFunc>(New)) {
    if (GI->getResolver())
      return New;
  } else {
    llvm_unreachable("Invalid GlobalValue type");
  }
  ...
  // 2. Link def.
  if (ForIndirectSymbol || shouldLink(New, *SGV))
    setError(linkGlobalValueBody(*New, *SGV));

  updateAttributes(*New);
  return New;
}
```

`linkGlobalValueProto`:

```shell
|--|--|--|--|--|--|--|--|--llvm/lib/Linker/IRMover.cpp, IRLinker::materialize
|--|--|--|--|--|--|--|--|--|--llvm/lib/Linker/IRMover.cpp, IRLinker::linkGlobalValueProto
```

```c++
Expected<Constant *> IRLinker::linkGlobalValueProto(GlobalValue *SGV,
                                                    bool ForIndirectSymbol) {
  GlobalValue *DGV = getLinkedToGlobal(SGV);

  bool ShouldLink = shouldLink(DGV, *SGV);

  // just missing from map
  if (ShouldLink) {
    auto I = ValueMap.find(SGV);
    if (I != ValueMap.end())
      return cast<Constant>(I->second);

    I = IndirectSymbolValueMap.find(SGV);
    if (I != IndirectSymbolValueMap.end())
      return cast<Constant>(I->second);
  }

  if (!ShouldLink && ForIndirectSymbol)
    DGV = nullptr;

  // Handle the ultra special appending linkage case first.
  if (SGV->hasAppendingLinkage() || (DGV && DGV->hasAppendingLinkage()))
    return linkAppendingVarProto(cast_or_null<GlobalVariable>(DGV),
                                 cast<GlobalVariable>(SGV));

  bool NeedsRenaming = false;
  GlobalValue *NewGV;
  ...
  else {
    // If we are done linking global value bodies (i.e. we are performing
    // metadata linking), don't link in the global value due to this
    // reference, simply map it to null.
    if (DoneLinkingBodies)
      return nullptr;

    // Copy SGV from src, create a new global value in dest.
    NewGV = copyGlobalValueProto(SGV, ShouldLink || ForIndirectSymbol);
    if (ShouldLink || !ForIndirectSymbol)
      NeedsRenaming = true;
  }
  ...
```

`copyGlobalValueProto`:

```shell
|--|--|--|--|--|--|--|--|--|--llvm/lib/Linker/IRMover.cpp, IRLinker::linkGlobalValueProto
|--|--|--|--|--|--|--|--|--|--|--llvm/lib/Linker/IRMover.cpp, IRLinker::copyGlobalValueProto
```

```c++
GlobalValue *IRLinker::copyGlobalValueProto(const GlobalValue *SGV,
                                            bool ForDefinition) {
  GlobalValue *NewGV;
  if (auto *SGVar = dyn_cast<GlobalVariable>(SGV)) {
    NewGV = copyGlobalVariableProto(SGVar);
  } else if (auto *SF = dyn_cast<Function>(SGV)) {
    NewGV = copyFunctionProto(SF);
  }
  ...
  return NewGV;
}
```

`copyGlobalVariableProto`:

```shell
|--|--|--|--|--|--|--|--|--|--|--llvm/lib/Linker/IRMover.cpp, IRLinker::copyGlobalValueProto
|--|--|--|--|--|--|--|--|--|--|--|--llvm/lib/Linker/IRMover.cpp, IRLinker::copyGlobalVariableProto
```

```c++
/// Loop through the global variables in the src module and merge them into the
/// dest module.
GlobalVariable *IRLinker::copyGlobalVariableProto(const GlobalVariable *SGVar) {
  // No linking to be performed or linking from the source: simply create an
  // identical version of the symbol over in the dest module... the
  // initializer will be filled in later by LinkGlobalInits.
  GlobalVariable *NewDGV =
      new GlobalVariable(DstM, TypeMap.get(SGVar->getValueType()),
                         SGVar->isConstant(), GlobalValue::ExternalLinkage,
                         /*init*/ nullptr, SGVar->getName(),
                         /*insertbefore*/ nullptr, SGVar->getThreadLocalMode(),
                         SGVar->getAddressSpace());
  NewDGV->setAlignment(SGVar->getAlign());
  NewDGV->copyAttributesFrom(SGVar);
  return NewDGV;
}
```

`copyFunctionProto`:

```shell
|--|--|--|--|--|--|--|--|--|--|--llvm/lib/Linker/IRMover.cpp, IRLinker::copyGlobalValueProto
|--|--|--|--|--|--|--|--|--|--|--|--llvm/lib/Linker/IRMover.cpp, IRLinker::copyFunctionProto
```

```c++
/// Link the function in the source module into the destination module if
/// needed, setting up mapping information.
Function *IRLinker::copyFunctionProto(const Function *SF) {
  // If there is no linkage to be performed or we are linking from the source,
  // bring SF over.
  auto *F = Function::Create(TypeMap.get(SF->getFunctionType()),
                             GlobalValue::ExternalLinkage,
                             SF->getAddressSpace(), SF->getName(), &DstM);
  F->copyAttributesFrom(SF);
  F->setAttributes(mapAttributeTypes(F->getContext(), F->getAttributes()));
  F->IsNewDbgInfoFormat = SF->IsNewDbgInfoFormat;
  return F;
}
```

Back to `IRLinker::materialize`, call `linkGlobalValueBody` to link def body:

```shell
|--|--|--|--|--|--|--|--|--llvm/lib/Linker/IRMover.cpp, IRLinker::materialize
|--|--|--|--|--|--|--|--|--|--llvm/lib/Linker/IRMover.cpp, IRLinker::linkGlobalValueBody
```

```c++
Error IRLinker::linkGlobalValueBody(GlobalValue &Dst, GlobalValue &Src) {
  if (auto *F = dyn_cast<Function>(&Src))
    return linkFunctionBody(cast<Function>(Dst), *F);
  if (auto *GVar = dyn_cast<GlobalVariable>(&Src)) {
    linkGlobalVariable(cast<GlobalVariable>(Dst), *GVar);
    return Error::success();
  }
  if (auto *GA = dyn_cast<GlobalAlias>(&Src)) {
    linkAliasAliasee(cast<GlobalAlias>(Dst), *GA);
    return Error::success();
  }
  linkIFuncResolver(cast<GlobalIFunc>(Dst), cast<GlobalIFunc>(Src));
  return Error::success();
}
```

`linkFunctionBody`:

```shell
|--|--|--|--|--|--|--|--|--|--llvm/lib/Linker/IRMover.cpp, IRLinker::linkGlobalValueBody
|--|--|--|--|--|--|--|--|--|--|--llvm/lib/Linker/IRMover.cpp, IRLinker::linkFunctionBody
```

```c++
/// Copy the source function over into the dest function and fix up references
/// to values. At this point we know that Dest is an external function, and
/// that Src is not.
Error IRLinker::linkFunctionBody(Function &Dst, Function &Src) {
  assert(Dst.isDeclaration() && !Src.isDeclaration());

  // Materialize if needed.
  if (Error Err = Src.materialize())
    return Err;

  // Link in the operands without remapping.
  if (Src.hasPrefixData())
    Dst.setPrefixData(Src.getPrefixData());
  if (Src.hasPrologueData())
    Dst.setPrologueData(Src.getPrologueData());
  if (Src.hasPersonalityFn())
    Dst.setPersonalityFn(Src.getPersonalityFn());
  assert(Src.IsNewDbgInfoFormat == Dst.IsNewDbgInfoFormat);

  // Copy over the metadata attachments without remapping.
  Dst.copyMetadata(&Src, 0);

  // Steal arguments and splice the body of Src into Dst.
  Dst.stealArgumentListFrom(Src);
  // (1) Here we copy the function body from src to dest.
  Dst.splice(Dst.end(), &Src);

  // (2) Everything has been moved over.  Remap it.
  // We will add this function to Worklist and handle its dependencies later in ~FlushingMapper().
  Mapper.scheduleRemapFunction(Dst);
  return Error::success();
}
```

`scheduleRemapFunction`:

```shell
|--|--|--|--|--|--|--|--|--|--|--llvm/lib/Linker/IRMover.cpp, IRLinker::linkFunctionBody
|--|--|--|--|--|--|--|--|--|--|--|--llvm/lib/Transforms/Utils/ValueMapper.cpp, ValueMapper::scheduleRemapFunction
```

```c++
void ValueMapper::scheduleRemapFunction(Function &F, unsigned MCID) {
  getAsMapper(pImpl)->scheduleRemapFunction(F, MCID);
}
```

`scheduleRemapFunction`:

```shell
|--|--|--|--|--|--|--|--|--|--|--|--llvm/lib/Transforms/Utils/ValueMapper.cpp, ValueMapper::scheduleRemapFunction
|--|--|--|--|--|--|--|--|--|--|--|--|--llvm/lib/Transforms/Utils/ValueMapper.cpp, Mapper::scheduleRemapFunction
```

```c++
void Mapper::scheduleRemapFunction(Function &F, unsigned MCID) {
  assert(AlreadyScheduled.insert(&F).second && "Should not reschedule");
  assert(MCID < MCs.size() && "Invalid mapping context");

  WorklistEntry WE;
  WE.Kind = WorklistEntry::RemapFunction;
  WE.MCID = MCID;
  WE.Data.RemapF = &F;
  // Note that this Worklist is Mapper::Worlist but not IRLinker::Worklist.
  Worklist.push_back(WE);
}
```

Back to `ValueMapper::mapValue`, call `~FlushingMapper()`:

```shell
|--|--|--|--|--|--llvm/lib/Transforms/Utils/ValueMapper.cpp, ValueMapper::mapValue
|--|--|--|--|--|--|--llvm/lib/Transforms/Utils/ValueMapper.cpp, ~FlushingMapper
```

```c++
~FlushingMapper() { M.flush(); }
```

`M.flush()`:

```shell
|--|--|--|--|--|--|--llvm/lib/Transforms/Utils/ValueMapper.cpp, ~FlushingMapper
|--|--|--|--|--|--|--|--llvm/lib/Transforms/Utils/ValueMapper.cpp, Mapper::flush
```

```c++
void Mapper::flush() {
  // Flush out the worklist of global values.
  while (!Worklist.empty()) {
    WorklistEntry E = Worklist.pop_back_val();
    CurrentMCID = E.MCID;
    switch (E.Kind) {
    ...
    case WorklistEntry::RemapFunction:
      remapFunction(*E.Data.RemapF);
      break;
    }
  }
  CurrentMCID = 0;
  ...
}
```

`remapFunction`:

```shell
|--|--|--|--|--|--|--|--llvm/lib/Transforms/Utils/ValueMapper.cpp, Mapper::flush
|--|--|--|--|--|--|--|--|--llvm/lib/Transforms/Utils/ValueMapper.cpp, llvm/lib/Transforms/Utils/ValueMapper.cpp
```

```c++
void Mapper::remapFunction(Function &F) {
  // Remap the operands.
  for (Use &Op : F.operands())
    if (Op)
      Op = mapValue(Op);

  // Remap the metadata attachments.
  remapGlobalObjectMetadata(F);

  // Remap the argument types.
  if (TypeMapper)
    for (Argument &A : F.args())
      A.mutateType(TypeMapper->remapType(A.getType()));

  // Remap the instructions.
  for (BasicBlock &BB : F) {
    for (Instruction &I : BB) {
      remapInstruction(&I);
      for (DbgRecord &DR : I.getDbgRecordRange())
        remapDbgRecord(DR);
    }
  }
}
```

Here we can dump the dest module with the following gdb command:

```shell
(gdb) print getMaterializer()->TheIRLinker->DstM.dump()
```

Case 1: test1.ll: `_Z4testIiEiT_` -> `_ZN1A3fooEv`.

```c++
; Function Attrs: mustprogress noinline optnone uwtable
define weak_odr dso_local noundef i32 @_Z4testIiEiT_(i32 noundef %0) #1 comdat {
  %2 = alloca i32, align 4
  %3 = alloca ptr, align 8
  %4 = alloca %class.A, align 1
  store i32 %0, ptr %2, align 4
  store ptr @.str, ptr %3, align 8
  %5 = load i32, ptr %2, align 4
  %6 = load ptr, ptr %3, align 8
  %7 = getelementptr inbounds i8, ptr %6, i64 0
  %8 = load i8, ptr %7, align 1
  %9 = sext i8 %8 to i32
  %10 = add nsw i32 %5, %9
  %11 = load i32, ptr @g, align 4
  %12 = add nsw i32 %10, %11
  %13 = call noundef i32 @_ZN1A3fooEv(ptr noundef nonnull align 1 dereferenceable(1) %4)
  %14 = add nsw i32 %12, %13
  ret i32 %14
}

; Function Attrs: mustprogress noinline nounwind optnone uwtable
define linkonce_odr dso_local noundef i32 @_ZN1A3fooEv(ptr noundef nonnull align 1 dereferenceable(1) %0) #0 comdat align 2 {
  %2 = alloca ptr, align 8
  store ptr %0, ptr %2, align 8
  %3 = load ptr, ptr %2, align 8
  ret i32 1
}
```

We focus on the following code in `Mapper::remapFunction`:

```c++
  // Remap the instructions.
  for (BasicBlock &BB : F) {
    for (Instruction &I : BB) {
      remapInstruction(&I);
      for (DbgRecord &DR : I.getDbgRecordRange())
        remapDbgRecord(DR);
    }
  }
```

Come to instruction ` %13 = call noundef i32 @_ZN1A3fooEv(ptr noundef nonnull align 1 dereferenceable(1) %4)`, call `remapInstruction`:

```shell
|--|--|--|--|--|--|--|--|--llvm/lib/Transforms/Utils/ValueMapper.cpp, llvm/lib/Transforms/Utils/ValueMapper.cpp
|--|--|--|--|--|--|--|--|--|--llvm/lib/Transforms/Utils/ValueMapper.cpp, Mapper::remapInstruction
```

```c++
void Mapper::remapInstruction(Instruction *I) {
  // Remap operands.
  for (Use &Op : I->operands()) {
    Value *V = mapValue(Op);
    // If we aren't ignoring missing entries, assert that something happened.
    if (V)
      Op = V;
    else
      assert((Flags & RF_IgnoreMissingLocals) &&
             "Referenced value not in value map!");
  }
  ...
}
```

There are two operands of ` %13 = call noundef i32 @_ZN1A3fooEv(ptr noundef nonnull align 1 dereferenceable(1) %4)`: `%4` and `_ZN1A3fooEv`. We focus on `_ZN1A3fooEv`, call `mapValue`:

```shell
|--|--|--|--|--|--|--|--|--|--llvm/lib/Transforms/Utils/ValueMapper.cpp, Mapper::remapInstruction
|--|--|--|--|--|--|--|--|--|--|--llvm/lib/Transforms/Utils/ValueMapper.cpp, Mapper::mapValue
```

```c++
Value *Mapper::mapValue(const Value *V) {
  ValueToValueMapTy::iterator I = getVM().find(V);

  // If the value already exists in the map, use it.
  if (I != getVM().end()) {
    assert(I->second && "Unexpected null mapping");
    return I->second;
  }

  // If we have a materializer and it can materialize a value, use that.
  if (auto *Materializer = getMaterializer()) {
    if (Value *NewV = Materializer->materialize(const_cast<Value *>(V))) {
      getVM()[V] = NewV;
      return NewV;
    }
  }
  ...
```

Here we meet `Mapper::mapValue` again, we can call `Materializer->materialize` to copy `_ZN1A3fooEv` from src to dest.

Case 2: test2.ll: `_Z4testIiEiT_` -> test1.ll:  `_ZN1A3fooEv`.

```c++
; Function Attrs: mustprogress noinline norecurse optnone uwtable
define dso_local noundef i32 @main() #0 {
  %1 = alloca i32, align 4
  %2 = alloca ptr, align 8
  %3 = alloca %class.A, align 1
  store i32 0, ptr %1, align 4
  store ptr @.str, ptr %2, align 8
  %4 = call noundef i32 @_Z4testIiEiT_(i32 noundef 1)
  %5 = load ptr, ptr %2, align 8
  %6 = getelementptr inbounds i8, ptr %5, i64 0
  %7 = load i8, ptr %6, align 1
  %8 = sext i8 %7 to i32
  %9 = add nsw i32 %4, %8
  %10 = call noundef i32 @_ZN1A3fooEv(ptr noundef nonnull align 1 dereferenceable(1) %3)
  %11 = add nsw i32 %9, %10
  ret i32 %11
}
```

We still focus on the following code in `Mapper::remapFunction`:

```c++
  // Remap the instructions.
  for (BasicBlock &BB : F) {
    for (Instruction &I : BB) {
      remapInstruction(&I);
      for (DbgRecord &DR : I.getDbgRecordRange())
        remapDbgRecord(DR);
    }
  }
```

Come to instruction `%10 = call noundef i32 @_ZN1A3fooEv(ptr noundef nonnull align 1 dereferenceable(1) %3)`, call `remapInstruction`:

```shell
|--|--|--|--|--|--|--|--|--llvm/lib/Transforms/Utils/ValueMapper.cpp, llvm/lib/Transforms/Utils/ValueMapper.cpp
|--|--|--|--|--|--|--|--|--|--llvm/lib/Transforms/Utils/ValueMapper.cpp, Mapper::remapInstruction
```

```c++
void Mapper::remapInstruction(Instruction *I) {
  // Remap operands.
  for (Use &Op : I->operands()) {
    Value *V = mapValue(Op);
    // If we aren't ignoring missing entries, assert that something happened.
    if (V)
      Op = V;
    else
      assert((Flags & RF_IgnoreMissingLocals) &&
             "Referenced value not in value map!");
  }
  ...
}
```

There are two operands of ` %10 = call noundef i32 @_ZN1A3fooEv(ptr noundef nonnull align 1 dereferenceable(1) %3)`: `%3` and `_ZN1A3fooEv`. We focus on `_ZN1A3fooEv`, call `mapValue`:

```shell
|--|--|--|--|--|--|--|--|--|--llvm/lib/Transforms/Utils/ValueMapper.cpp, Mapper::remapInstruction
|--|--|--|--|--|--|--|--|--|--|--llvm/lib/Transforms/Utils/ValueMapper.cpp, Mapper::mapValue
```

```c++
Value *Mapper::mapValue(const Value *V) {
  ValueToValueMapTy::iterator I = getVM().find(V);

  // If the value already exists in the map, use it.
  if (I != getVM().end()) {
    assert(I->second && "Unexpected null mapping");
    return I->second;
  }

  // If we have a materializer and it can materialize a value, use that.
  if (auto *Materializer = getMaterializer()) {
    if (Value *NewV = Materializer->materialize(const_cast<Value *>(V))) {
      getVM()[V] = NewV;
      return NewV;
    }
  }
  ...
```

`Materializer->materialize` -> `IRLinker::linkGlobalValueProto`:

```shell
|--|--|--|--|--|--|--|--|--|--|--llvm/lib/Transforms/Utils/ValueMapper.cpp, Mapper::mapValue
|--|--|--|--|--|--|--|--|--|--|--|--llvm/lib/Linker/IRMover.cpp, GlobalValueMaterializer::materialize
|--|--|--|--|--|--|--|--|--|--|--|--|--llvm/lib/Linker/IRMover.cpp, IRLinker::materialize
|--|--|--|--|--|--|--|--|--|--|--|--|--|--llvm/lib/Linker/IRMover.cpp, IRLinker::linkGlobalValueProto
```

```c++
Expected<Constant *> IRLinker::linkGlobalValueProto(GlobalValue *SGV,
                                                    bool ForIndirectSymbol) {
  GlobalValue *DGV = getLinkedToGlobal(SGV);
  ...
  GlobalValue *NewGV;
  if (DGV && !ShouldLink) {
    NewGV = DGV;
  } else {
```

Here we can find `_ZN1A3fooEv` in the dest module, so `DGV` is not null, and we can directly set `NewGV` with `DGV` instead of creating a new one.
