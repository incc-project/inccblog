# LLVM Bitcode File Format

https://llvm.org/docs/BitCodeFormat.html

# LTO

## [Description](https://llvm.org/docs/LinkTimeOptimization.html#id2)

LLVM features powerful intermodular optimizations which can be used at link time. Link Time Optimization (LTO) is another name for intermodular optimization when performed during the link stage. This document describes the interface and design between the LTO optimizer and the linker.

## [Design Philosophy](https://llvm.org/docs/LinkTimeOptimization.html#id3)

The LLVM Link Time Optimizer provides complete transparency, while doing intermodular optimization, in the compiler tool chain. Its main goal is to let the developer take advantage of intermodular optimizations without making any significant changes to the developer’s makefiles or build system. This is achieved through tight integration with the linker. In this model, the linker treats LLVM bitcode files like native object files and allows mixing and matching among them. The linker uses [libLTO](https://llvm.org/docs/LinkTimeOptimization.html#liblto), a shared object, to handle LLVM bitcode files. This tight integration between the linker and LLVM optimizer helps to do optimizations that are not possible in other models. The linker input allows the optimizer to avoid relying on conservative escape analysis.



### [Example of link time optimization](https://llvm.org/docs/LinkTimeOptimization.html#id4)

The following example illustrates the advantages of LTO’s integrated approach and clean interface. This example requires a system linker which supports LTO through the interface described in this document. Here, clang transparently invokes system linker.

- Input source file `a.c` is compiled into LLVM bitcode form.
- Input source file `main.c` is compiled into native object code.

```
--- a.h ---
extern int foo1(void);
extern void foo2(void);
extern void foo4(void);

--- a.c ---
#include "a.h"

static signed int i = 0;

void foo2(void) {
  i = -1;
}

static int foo3() {
  foo4();
  return 10;
}

int foo1(void) {
  int data = 0;

  if (i < 0)
    data = foo3();

  data = data + 42;
  return data;
}

--- main.c ---
#include <stdio.h>
#include "a.h"

void foo4(void) {
  printf("Hi\n");
}

int main() {
  return foo1();
}
```

To compile, run:

```
% clang -flto -c a.c -o a.o        # <-- a.o is LLVM bitcode file
% clang -c main.c -o main.o        # <-- main.o is native object file
% clang -flto a.o main.o -o main   # <-- standard link command with -flto
```

- In this example, the linker recognizes that `foo2()` is an externally visible symbol defined in LLVM bitcode file. The linker completes its usual symbol resolution pass and finds that `foo2()` is not used anywhere. This information is used by the LLVM optimizer and it removes `foo2()`.
- As soon as `foo2()` is removed, the optimizer recognizes that condition `i < 0` is always false, which means `foo3()` is never used. Hence, the optimizer also removes `foo3()`.
- And this in turn, enables linker to remove `foo4()`.

This example illustrates the advantage of tight integration with the linker. Here, the optimizer can not remove `foo3()` without the linker’s input.

## [Multi-phase communication between `libLTO` and linker](https://llvm.org/docs/LinkTimeOptimization.html#id6)

The linker collects information about symbol definitions and uses in various link objects which is more accurate than any information collected by other tools during typical build cycles. The linker collects this information by looking at the definitions and uses of symbols in native .o files and using symbol visibility information. The linker also uses user-supplied information, such as a list of exported symbols. LLVM optimizer collects control flow information, data flow information and knows much more about program structure from the optimizer’s point of view. Our goal is to take advantage of tight integration between the linker and the optimizer by sharing this information during various linking phases.

### [Phase 1 : Read LLVM Bitcode Files](https://llvm.org/docs/LinkTimeOptimization.html#id7)

The linker first reads all object files in natural order and collects symbol information. This includes native object files as well as LLVM bitcode files. To minimize the cost to the linker in the case that all .o files are native object files, the linker only calls `lto_module_create()` when a supplied object file is found to not be a native object file. If `lto_module_create()` returns that the file is an LLVM bitcode file, the linker then iterates over the module using `lto_module_get_symbol_name()` and `lto_module_get_symbol_attribute()` to get all symbols defined and referenced. This information is added to the linker’s global symbol table.

The lto* functions are all implemented in a shared object libLTO. This allows the LLVM LTO code to be updated independently of the linker tool. On platforms that support it, the shared object is lazily loaded.

### [Phase 2 : Symbol Resolution](https://llvm.org/docs/LinkTimeOptimization.html#id8)

In this stage, the linker resolves symbols using global symbol table. It may report undefined symbol errors, read archive members, replace weak symbols, etc. The linker is able to do this seamlessly even though it does not know the exact content of input LLVM bitcode files. If dead code stripping is enabled then the linker collects the list of live symbols.

### [Phase 3 : Optimize Bitcode Files](https://llvm.org/docs/LinkTimeOptimization.html#id9)

After symbol resolution, the linker tells the LTO shared object which symbols are needed by native object files. In the example above, the linker reports that only `foo1()` is used by native object files using `lto_codegen_add_must_preserve_symbol()`. Next the linker invokes the LLVM optimizer and code generators using `lto_codegen_compile()` which returns a native object file creating by <font color="red">merging</font> the LLVM bitcode files and applying various optimization passes.

> 注意， 这里会合并出一个大的ir！

### [Phase 4 : Symbol Resolution after optimization](https://llvm.org/docs/LinkTimeOptimization.html#id10)

In this phase, the linker reads optimized a native object file and updates the internal global symbol table to reflect any changes. The linker also collects information about any changes in use of external symbols by LLVM bitcode files. In the example above, the linker notes that `foo4()` is not used any more. If dead code stripping is enabled then the linker refreshes the live symbol information appropriately and performs dead code stripping.

After this phase, the linker continues linking as if it never saw LLVM bitcode files.

# Debug

## Step by step

Test program:

```c++
--- a.h ---
extern int foo1(void);
extern void foo2(void);
extern void foo4(void);

--- a.c ---
#include "a.h"

static signed int i = 0;

void foo2(void) {
  i = -1;
}

static int foo3() {
  foo4();
  return 10;
}

int foo1(void) {
  int data = 0;

  if (i < 0)
    data = foo3();

  data = data + 42;
  return data;
}

--- main.c ---
#include <stdio.h>
#include "a.h"

void foo4(void) {
  printf("Hi\n");
}

int main() {
  return foo1();
}
```

Here we only care about the linking process, so we compile a.c and main.c first:

```shell
clang -flto -c a.c main.c
```

Now we get two bc file a.o and main.o. Then, we link them to an executable file through lto:

``` shell
clang -flto a.o main.o
```

The lto module is integrated into the lld module. To debug lto, we need to get the lld command:

```shell
clang -v -flto a.o main.o

Deepin clang version 17.0.6 (5deepin4)
Target: x86_64-pc-linux-gnu
Thread model: posix
InstalledDir: /usr/bin
Found candidate GCC installation: /usr/bin/../lib/gcc/x86_64-linux-gnu/12
Found candidate GCC installation: /usr/bin/../lib/gcc/x86_64-linux-gnu/13
Selected GCC installation: /usr/bin/../lib/gcc/x86_64-linux-gnu/13
Candidate multilib: .;@m64
Selected multilib: .;@m64
 "/usr/bin/ld" -pie --hash-style=gnu --build-id --eh-frame-hdr -m elf_x86_64 -dynamic-linker /lib64/ld-linux-x86-64.so.2 -o a.out /lib/x86_64-linux-gnu/Scrt1.o /lib/x86_64-linux-gnu/crti.o /usr/bin/../lib/gcc/x86_64-linux-gnu/13/crtbeginS.o -L/usr/bin/../lib/gcc/x86_64-linux-gnu/13 -L/usr/bin/../lib/gcc/x86_64-linux-gnu/13/../../../../lib64 -L/lib/x86_64-linux-gnu -L/lib/../lib64 -L/usr/lib/x86_64-linux-gnu -L/usr/lib/../lib64 -L/lib -L/usr/lib -plugin /usr/lib/llvm-17/bin/../lib/LLVMgold.so -plugin-opt=mcpu=x86-64 a.o main.o -lgcc --as-needed -lgcc_s --no-as-needed -lc -lgcc --as-needed -lgcc_s --no-as-needed /usr/bin/../lib/gcc/x86_64-linux-gnu/13/crtendS.o /lib/x86_64-linux-gnu/crtn.o
```

Copy the arguments above to configure target lld in clion.

Now we can debug lld lto in clion.

Enter `linkerMain`, jump here:

```c++
  {
    llvm::TimeTraceScope timeScope("ExecuteLinker");

    initLLVM();
    createFiles(args);
    if (errorCount())
      return;

    inferMachineType();
    setConfigs(args);
    checkOptions();
    if (errorCount())
      return;

    link(args);
  }
```

Call `createFiles` to read a.o and main.o, here we focus on main.o.

There is a  `for args: switch` structure here, set conditional break point, filter main.o:

``` shell
arg->Spelling[0]=='m'
```

Check:

```shell
(gdb) print arg->Spelling
$1 = {static npos = 18446744073709551615, Data = 0x7fffffffe7fb "main.o", Length = 6}
```

Jump to `case OPT_INPUT`:

```c++
    case OPT_INPUT:
      addFile(arg->getValue(), /*withLOption=*/false);
      hasInput = true;
      break;
```

Call `addFile`.

Read file content to memory buffer:

```c++
std::optional<MemoryBufferRef> buffer = readFile(path);
if (!buffer)
  return;
MemoryBufferRef mbref = *buffer;
```

Jump to `case file_magic::bitcode`:

```c++
  case file_magic::bitcode:
    files.push_back(make<BitcodeFile>(mbref, "", 0, inLib));
    break;
```

Call `BitcodeFile::BitcodeFile` to create the `BitcodeFile` object for main.o.

Call `lto::InputFile::create`:

```c++
obj = CHECK(lto::InputFile::create(mbref), this);
```

Call `readIRSymtab` to create `IRSymtabFile`(bc file with symbol table, string table, modules):

```
Expected<IRSymtabFile> FOrErr = readIRSymtab(Object);
```

The related structure:

```c++
/// The contents of a bitcode file and its irsymtab. Any underlying data
/// for the irsymtab are owned by Symtab and Strtab.
struct IRSymtabFile {
  std::vector<BitcodeModule> Mods;
  SmallVector<char, 0> Symtab, Strtab;
  irsymtab::Reader TheReader;
};
```

Move `FOrErr` to `File`:

```c++
File->TargetTriple = FOrErr->TheReader.getTargetTriple();
File->SourceFileName = FOrErr->TheReader.getSourceFileName();
File->COFFLinkerOpts = FOrErr->TheReader.getCOFFLinkerOpts();
File->DependentLibraries = FOrErr->TheReader.getDependentLibraries();
File->ComdatTable = FOrErr->TheReader.getComdatTable();

for (unsigned I = 0; I != FOrErr->Mods.size(); ++I) {
  size_t Begin = File->Symbols.size();
  for (const irsymtab::Reader::SymbolRef &Sym :
       FOrErr->TheReader.module_symbols(I))
    // Skip symbols that are irrelevant to LTO. Note that this condition needs
    // to match the one in Skip() in LTO::addRegularLTO().
    if (Sym.isGlobal() && !Sym.isFormatSpecific())
      File->Symbols.push_back(Sym);
  File->ModuleSymIndices.push_back({Begin, File->Symbols.size()});
}

File->Mods = FOrErr->Mods;
File->Strtab = std::move(FOrErr->Strtab);
return std::move(File);
```

Return to `BitcodeFile::BitcodeFile`, set some architectural information according to `obj` created by `lto::InputFile::create`:

```c++
Triple t(obj->getTargetTriple());
ekind = getBitcodeELFKind(t);
emachine = getBitcodeMachineKind(mb.getBufferIdentifier(), t);
osabi = getOsAbi(t);
```

Return to `addFile`, return to `createFiles`, return to `linkerMain`:

```c++
  {
    llvm::TimeTraceScope timeScope("ExecuteLinker");

    initLLVM();
    createFiles(args);
    if (errorCount())
      return;

    inferMachineType();
    setConfigs(args);
    checkOptions();
    if (errorCount())
      return;

    link(args);
  }
```

<font color="red">Finish Phase1. Read LLVM Bitcode Files.</font>

Call `link`, parse bc file:

```c++
// Add all files to the symbol table. This will add almost all
// symbols that we need to the symbol table. This process might
// add files to the link, via autolinking, these files are always
// appended to the Files vector.
{
  llvm::TimeTraceScope timeScope("Parse input files");
  for (size_t i = 0; i < files.size(); ++i) {
    llvm::TimeTraceScope timeScope("Parse input files", files[i]->getName());
    parseFile(files[i]);
  }
}
```

```shell
(gdb) print files.size()
$1 = 1075
```

Too many files! We should use conditional break point to filter a.o and main.o, here we focus on main.o:

```shell
files[i]->getName()[0] == 'm'
```

Now we enter `doParseFile`, check the current file:

```shell
(gdb) print file->getName()
$3 = {static npos = 18446744073709551615, Data = 0x5555555d4bb0 "main.o", Length = 6}
```

call `BitcodeFile::parse`, Handle bc file:

```c++
  // LLVM bitcode file
  if (auto *f = dyn_cast<BitcodeFile>(file)) {
    ctx.bitcodeFiles.push_back(f);
    f->parse();
    return;
  }
```

Parse symbols (convert `irsymtab::Symbol` to ELF `Symbol`)

```c++
if (numSymbols == 0) {
  numSymbols = obj->symbols().size();
  symbols = std::make_unique<Symbol *[]>(numSymbols);
}
// Process defined symbols first. See the comment in
// ObjFile<ELFT>::initializeSymbols.
for (auto [i, irSym] : llvm::enumerate(obj->symbols()))
  if (!irSym.isUndefined())
    createBitcodeSymbol(symbols[i], keptComdats, irSym, *this);
for (auto [i, irSym] : llvm::enumerate(obj->symbols()))
  if (irSym.isUndefined())
    createBitcodeSymbol(symbols[i], keptComdats, irSym, *this);

for (auto l : obj->getDependentLibraries())
  addDependentLibrary(l, this);
```

Check symbols:

```c++
(gdb) print symbols[0]->getName()
$11 = {static npos = 18446744073709551615, Data = 0x5555555c02f2 "foo4", Length = 4}
(gdb) print symbols[1]->getName()
$12 = {static npos = 18446744073709551615, Data = 0x5555555c0301 "printf", Length = 6}
(gdb) print symbols[2]->getName()
$13 = {static npos = 18446744073709551615, Data = 0x55555559a015 "main", Length = 4}
(gdb) print symbols[3]->getName()
$14 = {static npos = 18446744073709551615, Data = 0x5555555c02ed "foo1", Length = 4}
(gdb) print numSymbols
$15 = 4
```

The related ir of main.o:

```
; ModuleID = 'main.o'
source_filename = "main.c"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-linux-gnu"

@.str = private unnamed_addr constant [4 x i8] c"Hi\0A\00", align 1

; Function Attrs: noinline nounwind optnone uwtable
define dso_local void @foo4() #0 {
  %1 = call i32 (ptr, ...) @printf(ptr noundef @.str)
  ret void
}

declare i32 @printf(ptr noundef, ...) #1

; Function Attrs: noinline nounwind optnone uwtable
define dso_local i32 @main() #0 {
  %1 = alloca i32, align 4
  store i32 0, ptr %1, align 4
  %2 = call i32 @foo1()
  ret i32 %2
}

declare i32 @foo1() #1

attributes #0 = { noinline nounwind optnone uwtable "frame-pointer"="all" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #1 = { "frame-pointer"="all" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }

!llvm.module.flags = !{!0, !1, !2, !3, !4, !5, !6}
!llvm.ident = !{!7}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 8, !"PIC Level", i32 2}
!2 = !{i32 7, !"PIE Level", i32 2}
!3 = !{i32 7, !"uwtable", i32 2}
!4 = !{i32 7, !"frame-pointer", i32 2}
!5 = !{i32 1, !"ThinLTO", i32 0}
!6 = !{i32 1, !"EnableSplitLTOUnit", i32 1}
!7 = !{!"Deepin clang version 17.0.6 (5deepin4)"}

^0 = module: (path: "main.o", hash: (0, 0, 0, 0, 0))
^1 = gv: (name: ".str", summaries: (variable: (module: ^0, flags: (linkage: private, visibility: default, notEligibleToImport: 1, live: 0, dsoLocal: 1, canAutoHide: 0), varFlags: (readonly: 1, writeonly: 0, constant: 1)))) ; guid = 4917006661136692784
^2 = gv: (name: "printf") ; guid = 7383291119112528047
^3 = gv: (name: "foo1") ; guid = 7682762345278052905
^4 = gv: (name: "foo4", summaries: (function: (module: ^0, flags: (linkage: external, visibility: default, notEligibleToImport: 1, live: 0, dsoLocal: 1, canAutoHide: 0), insts: 2, funcFlags: (readNone: 0, readOnly: 0, noRecurse: 0, returnDoesNotAlias: 0, noInline: 1, alwaysInline: 0, noUnwind: 1, mayThrow: 0, hasUnknownCall: 0, mustBeUnreachable: 0), calls: ((callee: ^2)), refs: (^1)))) ; guid = 11564431941544006930
^5 = gv: (name: "main", summaries: (function: (module: ^0, flags: (linkage: external, visibility: default, notEligibleToImport: 1, live: 0, dsoLocal: 1, canAutoHide: 0), insts: 4, funcFlags: (readNone: 0, readOnly: 0, noRecurse: 0, returnDoesNotAlias: 0, noInline: 1, alwaysInline: 0, noUnwind: 1, mayThrow: 0, hasUnknownCall: 0, mustBeUnreachable: 0), calls: ((callee: ^3))))) ; guid = 15822663052811949562
^6 = flags: 8
^7 = blockcount: 0
```

Return to `parseFile` ( -> `doParseFile` -> `parseFile`).

Return to `link`.

Do lto:

```c++
  // Do link-time optimization if given files are LLVM bitcode files.
  // This compiles bitcode files into real object files.
  //
  // With this the symbol table should be complete. After this, no new names
  // except a few linker-synthesized ones will be added to the symbol table.
  const size_t numObjsBeforeLTO = ctx.objectFiles.size();
  invokeELFT(compileBitcodeFiles, skipLinkedOutput);
```

The related comment:

```c++
// This function is where all the optimizations of link-time
// optimization takes place. When LTO is in use, some input files are
// not in native object file format but in the LLVM bitcode format.
// This function compiles bitcode files into a few big native files
// using LLVM functions and replaces bitcode symbols with the results.
// Because all bitcode files that the program consists of are passed to
// the compiler at once, it can do a whole-program optimization.
template <class ELFT>
void LinkerDriver::compileBitcodeFiles(bool skipLinkedOutput) {
```

Call `BitcodeCompiler::add` to add bc to lto (`ctx.bitcodeFiles.size()` = 2):

```c++
// Compile bitcode files and replace bitcode symbols.
lto.reset(new BitcodeCompiler);
for (BitcodeFile *file : ctx.bitcodeFiles)
  lto->add(*file);
```

Call `LTO::add`:

```c++
  checkError(ltoObj->add(std::move(f.obj), resols));
}
```

Call `LTO::addModule`:

```c++
for (unsigned I = 0; I != Input->Mods.size(); ++I)
  if (Error Err = addModule(*Input, I, ResI, Res.end()))
    return Err;
```

Call `addModuleToGlobalRes`:

```c++
BitcodeModule BM = Input.Mods[ModI];
auto ModSyms = Input.module_symbols(ModI);
addModuleToGlobalRes(ModSyms, {ResI, ResE},
                     LTOInfo->IsThinLTO ? ThinLTO.ModuleMap.size() + 1 : 0,
                     LTOInfo->HasSummary);
```

Related comment:

```c++
// Add the symbols in the given module to the GlobalResolutions map, and resolve
// their partitions.
void LTO::addModuleToGlobalRes(ArrayRef<InputFile::Symbol> Syms,
                               ArrayRef<SymbolResolution> Res,
                               unsigned Partition, bool InSummary) {
```

Add the symbols in the given module to the `GlobalResolutions` map, and resolve their partitions.

<font color="red">Finish Phase2. Symbol Resolution</font>

Call `addRegularLTO`:

```c++
RegularLTO.EmptyCombinedModule = false;
Expected<RegularLTOState::AddedModule> ModOrErr =
    addRegularLTO(BM, ModSyms, ResI, ResE);
if (!ModOrErr)
  return ModOrErr.takeError();
```

The related comment:

```c++
// Add a regular LTO object to the link.
// The resulting module needs to be linked into the combined LTO module with
// linkRegularLTO.
Expected<LTO::RegularLTOState::AddedModule>
LTO::addRegularLTO(BitcodeModule BM, ArrayRef<InputFile::Symbol> Syms,
                   const SymbolResolution *&ResI,
                   const SymbolResolution *ResE) {
```

Note that the resulting module needs to be linked into the combined LTO module with `linkRegularLTO`.

Create a module `Mod` to be added, set up, return `Mod`:

```c++
RegularLTOState::AddedModule Mod;
Expected<std::unique_ptr<Module>> MOrErr =
    BM.getLazyModule(RegularLTO.Ctx, /*ShouldLazyLoadMetadata*/ true,
                     /*IsImporting*/ false);
if (!MOrErr)
  return MOrErr.takeError();
Module &M = **MOrErr;
Mod.M = std::move(*MOrErr);
... // set up
return std::move(Mod);
}
```

return to `addModule`, add the newly created module to `ModsWithSummaries`:

```c++
  // Regular LTO module summaries are added to a dummy module that represents
  // the combined regular LTO module.
  if (Error Err = BM.readSummary(ThinLTO.CombinedIndex, "", -1ull))
    return Err;
  RegularLTO.ModsWithSummaries.push_back(std::move(*ModOrErr));
  return Error::success();
```

<font color="red">Q: Does LTO also use summary in regular mode?</font>

The related comment:

```c++
// This stores the information about a regular LTO module that we have added
// to the link. It will either be linked immediately (for modules without
// summaries) or after summary-based dead stripping (for modules with
// summaries).
struct AddedModule {
  std::unique_ptr<Module> M;
  std::vector<GlobalValue *> Keep;
};
std::vector<AddedModule> ModsWithSummaries;
bool EmptyCombinedModule = true;
```

lld will perform dead symbol stripping based on summary to reduce (IR) link overhead, which we will see soon.

For modules without summaries, lld will linked it immediately. Although it is not the default behavior of lto, for better understanding, let's debug it:

Call `linkRegularLTO` for modules without summaries:

```c++
  if (!LTOInfo->HasSummary)
    return linkRegularLTO(std::move(*ModOrErr), /*LivenessFromIndex=*/false);
```

Call `IRMover::move`:

```c++
  return RegularLTO.Mover->move(std::move(Mod.M), Keep, nullptr,
                                /* IsPerformingImport */ false);
}
```

Call `IRLinker::run` to merge a large bc:

> llvm/lib/Linker/IRMover.cpp

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

When we use llvm-link, it also call this function:

```shell
llvm/tools/llvm-link/llvm-link.cpp:main
llvm/tools/llvm-link/llvm-link.cpp:linkFiles
llvm/lib/Linker/LinkModules.cpp:linkInModule
llvm/lib/Linker/LinkModules.cpp:ModuleLinker::run
llvm/lib/Linker/IRMover.cpp:move
llvm/lib/Linker/IRMover.cpp:IRLinker::run
```

Now we come back to `compileBitcodeFiles`.

Call `BitcodeCompiler::compile` to compile bc files:

```c++
for (InputFile *file : lto->compile()) {
```

The related comment:

```c++
// Merge all the bitcode files we have seen, codegen the result
// and return the resulting ObjectFile(s).
std::vector<InputFile *> BitcodeCompiler::compile() {
```

`maxTasks` = 1:

```c++
unsigned maxTasks = ltoObj->getMaxTasks();
buf.resize(maxTasks);
files.resize(maxTasks);
```

Handle thinlto's cache (we do not enable thinlto in this example):

```c++
// The --thinlto-cache-dir option specifies the path to a directory in which
// to cache native object files for ThinLTO incremental builds. If a path was
// specified, configure LTO to use it as the cache directory.
FileCache cache;
if (!config->thinLTOCacheDir.empty())
  cache = check(localCache("ThinLTO", "Thin", config->thinLTOCacheDir,
                           [&](size_t task, const Twine &moduleName,
                               std::unique_ptr<MemoryBuffer> mb) {
                             files[task] = std::move(mb);
                           }));
```

Call `LTO::run`:

```
if (!ctx.bitcodeFiles.empty())
  checkError(ltoObj->run(
      [&](size_t task, const Twine &moduleName) {
        return std::make_unique<CachedFileStream>(
            std::make_unique<raw_svector_ostream>(buf[task]));
      },
      cache));
```

As we mentioned above, compute dead symbols here:

```c++
// Compute "dead" symbols, we don't want to import/export these!
DenseSet<GlobalValue::GUID> GUIDPreservedSymbols;
DenseMap<GlobalValue::GUID, PrevailingType> GUIDPrevailingResolutions;
for (auto &Res : GlobalResolutions) {
  // Normally resolution have IR name of symbol. We can do nothing here
  // otherwise. See comments in GlobalResolution struct for more details.
  if (Res.second.IRName.empty())
    continue;

  GlobalValue::GUID GUID = GlobalValue::getGUID(
      GlobalValue::dropLLVMManglingEscape(Res.second.IRName));

  if (Res.second.VisibleOutsideSummary && Res.second.Prevailing)
    GUIDPreservedSymbols.insert(GUID);

  if (Res.second.ExportDynamic)
    DynamicExportSymbols.insert(GUID);

  GUIDPrevailingResolutions[GUID] =
      Res.second.Prevailing ? PrevailingType::Yes : PrevailingType::No;
}

auto isPrevailing = [&](GlobalValue::GUID G) {
  auto It = GUIDPrevailingResolutions.find(G);
  if (It == GUIDPrevailingResolutions.end())
    return PrevailingType::Unknown;
  return It->second;
};
computeDeadSymbolsWithConstProp(ThinLTO.CombinedIndex, GUIDPreservedSymbols,
                                isPrevailing, Conf.OptLevel > 0);
```

Call `runRegularLTO`:

```c++
  Error Result = runRegularLTO(AddStream);
  if (!Result)
    Result = runThinLTO(AddStream, Cache, GUIDPreservedSymbols);

  if (StatsFile)
    PrintStatisticsJSON(StatsFile->os());

  return Result;
}
```

For each previously saved module (`ModsWithSummaries`), call `linkRegularLTO` to link them into a large bc.

```c++
// Finalize linking of regular LTO modules containing summaries now that
// we have computed liveness information.
for (auto &M : RegularLTO.ModsWithSummaries)
  if (Error Err = linkRegularLTO(std::move(M),
                                 /*LivenessFromIndex=*/true))
    return Err;
```

Call `IRMover::move`:

```c++
  return RegularLTO.Mover->move(std::move(Mod.M), Keep, nullptr,
                                /* IsPerformingImport */ false);
}
```

Call `IRLinker::run`

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

We will debug this function in detail in llvm-link.

Return to `runRegularLTO`.

Call `lto::backend` to enable pass manager and code generation.

```c++
if (!RegularLTO.EmptyCombinedModule || Conf.AlwaysEmitRegularLTOObj) {
  if (Error Err =
          backend(Conf, AddStream, RegularLTO.ParallelCodeGenParallelismLevel,
                  *RegularLTO.CombinedModule, ThinLTO.CombinedIndex))
    return Err;
}
```

`opt` and `codegen`:

```c++
Error lto::backend(const Config &C, AddStreamFn AddStream,
                   unsigned ParallelCodeGenParallelismLevel, Module &Mod,
                   ModuleSummaryIndex &CombinedIndex) {
  Expected<const Target *> TOrErr = initAndLookupTarget(C, Mod);
  if (!TOrErr)
    return TOrErr.takeError();

  std::unique_ptr<TargetMachine> TM = createTargetMachine(C, *TOrErr, Mod);

  if (!C.CodeGenOnly) {
    if (!opt(C, TM.get(), 0, Mod, /*IsThinLTO=*/false,
             /*ExportSummary=*/&CombinedIndex, /*ImportSummary=*/nullptr,
             /*CmdArgs*/ std::vector<uint8_t>()))
      return Error::success();
  }

  if (ParallelCodeGenParallelismLevel == 1) {
    codegen(C, TM.get(), AddStream, 0, Mod, CombinedIndex);
  } else {
    splitCodeGen(C, TM.get(), AddStream, ParallelCodeGenParallelismLevel, Mod,
                 CombinedIndex);
  }
  return Error::success();
}
```

<font color="red">Finish Phase 3: Optimize Bitcode Files</font>

We do not care about Phase 4, at this point we have already understood the workflow of lto.

## Summary

```
 linkerMain
 |createFiles
 ||addFile
 |||BitcodeFile::BitcodeFile
 ||||lto::InputFile::create
 |||||Finish Phase1. Read LLVM Bitcode Files.
 |link
 ||parseFile
 |||BitcodeFile::parse
 |compileBitcodeFiles
 ||BitcodeCompiler::add
 |||LTO::add
 ||||LTO::addModule
 |||||Finish Phase2. Symbol Resolution
 ||||addRegularLTO
 ||BitcodeCompiler::compile
 |||LTO::run
 ||||runRegularLTO
 |||||linkRegularLTO
 ||||||IRMover::move
 |||||||IRLinker::run
 |||||lto::backend
 ||||||opt
 ||||||codegen
 ||||||Finish Phase 3: Optimize Bitcode Files
```

