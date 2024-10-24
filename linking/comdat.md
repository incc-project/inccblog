# COMDAT

## What is COMDAT?

If you search this concept in Google, the first result jumping out should be from [StackOverflow](https://stackoverflow.com/questions/1834597/what-is-the-comdat-section-used-for) (at least in my case).

> The purpose of a COMDAT section is to allow “duplicate” sections to be defined in multiple object files. Normally, if the same symbol is defined in multiple object files, the linker will report errors. This can cause problems for some C++ language features, like templates, that may instantiate the same symbols in different cpp files.

COMDAT is actually a section in an object file. It contains symbols that can be potentially with same name in different objects. This could happen when different C++ source files instantiate same template functions. Consider the following example:

```c++
// header.hpp
template <typename T>
T add(T a, T b) { return a + b; }
// foo.cpp
#include "header.hpp"
int foo(int a, int b) { return add(a, b); }
// bar.cpp
#include "header.hpp"
int bar(int a, int b) { return add(a, b); }
```

After we compile `foo.cpp` and `bar.cpp` and get `foo.o` and `bar.o`, both the two objects contain a symbol named `__Z3addIiET_S0_S0_`, which is a mangled name. It stands for `int add<int>(int, int)`, which is exactly the instance function of the template function in `header.hpp`. When the linker links the two objects, if we don’t do something, there will be a linker error because there are two symbols with the same name.

COMDAT is to solve this problem. The symbol `__Z3addIiET_S0_S0_` is put into a special section (COMDAT section). Since symbols in an object must have different names, when multiple objects are linked together, only one of those symbols with different names from different COMDAT sections in different objects can be kept. The linker must determine which one to stay. There are a couple of strategies that will be covered in next section.

But wait, why? Shouldn’t they be the same, like the above case, such that we can choose whatever we want? They should work fine because they’re same. Well, that’s true. However, things are not always like that. Consider the following code:

```c++
// foo.cpp
template <typename T>
T add(T a, T b) { return a + b; }
int foo(int a, int b) { return add(a, b); }
// bar.cpp
template <typename T>
T add(T a, T b) { return a + b + 2; }
int bar(int a, int b) { return add(a, b); }
```

Every file has its own template function `add`, and they work differently. However, they’re all called `__Z3addIiET_S0_S0_` in their own objects, without any difference! During the linkage, the linker knows they’re different, but which one to choose? Here comes the strategy. You might be thinking, does it mean either `foo` or `bar` will not work correctly after the linkage?! Unfortunately, that’s true. That is how C++ works! That’s why we have millions of articles titled “Best Practice in C++” or “Ten things you should never do in C++”, etc. 🙂

## How COMDAT works in LLVM?

Let’s first take a look how `llvm::Comdat` is defined:

```c++
class Comdat {
public:
  enum SelectionKind {
    Any,
    ExactMatch,
    Largest,
    NoDuplicates,
    SameSize,
  };
  Comdat(const Comdat &) = delete;
  Comdat(Comdat &&C);
  SelectionKind getSelectionKind() const;
  void setSelectionKind(SelectionKind Val);
  StringRef getName() const;
private:
  friend class Module;
  Comdat();
  StringMapEntry<Comdat> *Name = nullptr;
  SelectionKind SK = Any;
};
```

Form simplicity, I only kept meaningful parts. The class is very simple. It only contains two data members, a pointer to `StringMapEntry` and a `SelectionKind`. The latter one is pretty straightforward, which defines how to deal with the corresponding symbol. It has five kinds (strategies):

- `Any`: The linker can choose whichever it wants when it has multiple symbols with the same name from different objects.
- `ExactMatch`: The linker needs to check every instance from different objects whether they’re exact matched. If so, it can choose any of them (obviously). Others will be dropped. If any of them is different from others, a linkage error will be emitted. As for what is *exact match*, it just means different instances must have same size, same functionalities, etc.
- `Largest`: The linker should choose the largest one if multiple instances are of different sizes.
- `NoDuplicates`: This symbol should NOT be defined in another object, which means it can only exist in one object. Neither example in the previous section can pass if the COMDAT is this kind.
- `SameSize`: The linker needs to check whether the corresponding symbols from different objects are of same **size**. It is different from `ExactMatch` because it only requires the same size. It is possible that different symbols can have the same size but different functionalities.

From the class definition, `llvm::Comdat` is like a property of a symbol. Therefore, each `llvm::GlobalObject` holds a pointer to a `llvm::Comdat`. The `llvm::Module` contains a mapping from a symbol name to its corresponding `llvm::Comdat`, and `llvm::Comdat`s are actually stored in the map. For efficient look-up, `llvm::Comdat` contains a pointer to its corresponding entry in the map. It has three advantages:

- The owner of the `llvm::Comdat` is the map (part of the `llvm::Module`) not a symbol. In this way, all COMDATs for a single module are in a same place. We can easily traverse all COMDATs if necessary.
- A symbol only needs to hold a pointer to its `llvm::Comdat` without taking care of its lifetime.
- The symbol name can be easily got via the pointer to the entry.

Every time we need to check whether a symbol is in the COMDAT section, we can either use function `llvm::GlobalObject::hasComdat()`or check whether the return value of `llvm::GlobalObject::getComdat()` is `nullptr`.

## ModuleLinker::computeResultingSelectionKind

When linking two comdat with the same name, llvm-link should determine which symbol to use, and compute the selection kind.

The related source code (LLVM 19.1.2)

`llvm/lib/Linker/LinkModules.cpp`

```c++
bool ModuleLinker::run() {
  Module &DstM = Mover.getModule();
  DenseSet<const Comdat *> ReplacedDstComdats;
  DenseSet<const Comdat *> NonPrevailingComdats;

  for (const auto &SMEC : SrcM->getComdatSymbolTable()) {
    const Comdat &C = SMEC.getValue();
    if (ComdatsChosen.count(&C))
      continue;
    Comdat::SelectionKind SK;
    LinkFrom From;
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
  ...
```

Call `getComdatResult` to calculate `SK` and `From`:

```c++
bool ModuleLinker::getComdatResult(const Comdat *SrcC,
                                   Comdat::SelectionKind &Result,
                                   LinkFrom &From) {
  Module &DstM = Mover.getModule();
  Comdat::SelectionKind SSK = SrcC->getSelectionKind();
  StringRef ComdatName = SrcC->getName();
  Module::ComdatSymTabType &ComdatSymTab = DstM.getComdatSymbolTable();
  Module::ComdatSymTabType::iterator DstCI = ComdatSymTab.find(ComdatName);

  if (DstCI == ComdatSymTab.end()) {
    // Use the comdat if it is only available in one of the modules.
    From = LinkFrom::Src;
    Result = SSK;
    return false;
  }

  const Comdat *DstC = &DstCI->second;
  Comdat::SelectionKind DSK = DstC->getSelectionKind();
  return computeResultingSelectionKind(ComdatName, SSK, DSK, Result, From);
}
```

If the source comdat is found in the dest Module, llvm-link will call `computeResultingSelectionKind` to determine which symbol to use, and compute the selection kind:

```c++
bool ModuleLinker::computeResultingSelectionKind(StringRef ComdatName,
                                                 Comdat::SelectionKind Src,
                                                 Comdat::SelectionKind Dst,
                                                 Comdat::SelectionKind &Result,
                                                 LinkFrom &From) {
  Module &DstM = Mover.getModule();
  // The ability to mix Comdat::SelectionKind::Any with
  // Comdat::SelectionKind::Largest is a behavior that comes from COFF.
  bool DstAnyOrLargest = Dst == Comdat::SelectionKind::Any ||
                         Dst == Comdat::SelectionKind::Largest;
  bool SrcAnyOrLargest = Src == Comdat::SelectionKind::Any ||
                         Src == Comdat::SelectionKind::Largest;
  if (DstAnyOrLargest && SrcAnyOrLargest) {
    if (Dst == Comdat::SelectionKind::Largest ||
        Src == Comdat::SelectionKind::Largest)
      Result = Comdat::SelectionKind::Largest;
    else
      Result = Comdat::SelectionKind::Any;
  } else if (Src == Dst) {
    Result = Dst;
  } else {
    return emitError("Linking COMDATs named '" + ComdatName +
                     "': invalid selection kinds!");
  }

  switch (Result) {
  case Comdat::SelectionKind::Any:
    // Go with Dst.
    From = LinkFrom::Dst;
    break;
  case Comdat::SelectionKind::NoDeduplicate:
    From = LinkFrom::Both;
    break;
  case Comdat::SelectionKind::ExactMatch:
  case Comdat::SelectionKind::Largest:
  case Comdat::SelectionKind::SameSize: {
    const GlobalVariable *DstGV;
    const GlobalVariable *SrcGV;
    if (getComdatLeader(DstM, ComdatName, DstGV) ||
        getComdatLeader(*SrcM, ComdatName, SrcGV))
      return true;

    const DataLayout &DstDL = DstM.getDataLayout();
    const DataLayout &SrcDL = SrcM->getDataLayout();
    uint64_t DstSize = DstDL.getTypeAllocSize(DstGV->getValueType());
    uint64_t SrcSize = SrcDL.getTypeAllocSize(SrcGV->getValueType());
    if (Result == Comdat::SelectionKind::ExactMatch) {
      if (SrcGV->getInitializer() != DstGV->getInitializer())
        return emitError("Linking COMDATs named '" + ComdatName +
                         "': ExactMatch violated!");
      From = LinkFrom::Dst;
    } else if (Result == Comdat::SelectionKind::Largest) {
      From = SrcSize > DstSize ? LinkFrom::Src : LinkFrom::Dst;
    } else if (Result == Comdat::SelectionKind::SameSize) {
      if (SrcSize != DstSize)
        return emitError("Linking COMDATs named '" + ComdatName +
                         "': SameSize violated!");
      From = LinkFrom::Dst;
    } else {
      llvm_unreachable("unknown selection kind");
    }
    break;
  }
  }

  return false;
}
```

Note that llvm only support perform `ExactMatch` on `GlobalVariable`, see the related source code:

```c++
bool ModuleLinker::getComdatLeader(Module &M, StringRef ComdatName,
                                   const GlobalVariable *&GVar) {
  const GlobalValue *GVal = M.getNamedValue(ComdatName);
  if (const auto *GA = dyn_cast_or_null<GlobalAlias>(GVal)) {
    GVal = GA->getAliaseeObject();
    if (!GVal)
      // We cannot resolve the size of the aliasee yet.
      return emitError("Linking COMDATs named '" + ComdatName +
                       "': COMDAT key involves incomputable alias size.");
  }

  GVar = dyn_cast_or_null<GlobalVariable>(GVal);
  if (!GVar)
    return emitError(
        "Linking COMDATs named '" + ComdatName +
        "': GlobalVariable required for data dependent selection!");

  return false;
}
```

# COMDAT and weak symbol

In C++, inline functions, template instantiations, and a few other things can be defined in multiple object files but need deduplication at link time. In the dark ages the functionality was implemented by weak definitions: the linker does not report duplicate definition errors and resolves the references to the first definition. The downside is that unneeded copies remained in the linked image.

Some discussion about comdat and weak symbol:

https://stackoverflow.com/questions/57378828/whats-the-difference-between-weak-linkage-vs-using-a-comdat-section

https://gcc.gnu.org/onlinedocs/gcc/Vague-Linkage.html#Vague-Linkage

> On targets that don’t support COMDAT, but do support weak symbols, GCC uses them. This way one copy overrides all the others．

# COMDAT and section groups

Assume that we have two source files, each of which will instantiate function `__Z3subIiET_S0_S0_`, and both `__Z3addIiET_S0_S0_` will instantiate another function `__Z3addIiET_S0_S0_`.

When linking these two source files, we need to deduplicate both `__Z3subIiET_S0_S0_` and `__Z3addIiET_S0_S0_`, here we have three choices:

- Use two COMDAT symbols. There is the drawback that deduplication happens independently for the interconnected sections.
- Make the section `.text.__Z3addIiET_S0_S0_` link to the section `.text.__Z3subIiET_S0_S0_` via `IMAGE_COMDAT_SELECT_ASSOCIATIVE`.
- Use section groups.

You can see that `IMAGE_COMDAT_SELECT_ASSOCIATIVE` and section groups are designed for situations where multiple sections are interrelated. With them,  the interrelated sections will be retained or discarded as a unit.

## IMAGE_COMDAT_SELECT_ASSOCIATIVE

`IMAGE_COMDAT_SELECT_ASSOCIATIVE` makes a section linked to another section. The interrelated sections will be retained or discarded as a unit. Conceptually there is a leader and an arbitrary number of followers. We will see that in ELF section groups the sections are equal.

An example of `IMAGE_COMDAT_SELECT_ASSOCIATIVE` in LLVM IR:

```
$foo = comdat largest
@foo = global i32 2, comdat($foo)

define void @bar() comdat($foo) {
  ret void
}
```

In a COFF object file, this will create a COMDAT section with selection kind `IMAGE_COMDAT_SELECT_LARGEST` containing the contents of the `@foo` symbol and another COMDAT section with selection kind `IMAGE_COMDAT_SELECT_ASSOCIATIVE` which is associated with the first COMDAT section and contains the contents of the `@bar` symbol.

## ELF section groups

In the GNU world, `.gnu.linkonce.` was invented to deduplicate groups with just one member. `.gnu.linkonce.` has been long obsoleted in favor of section groups but the usage has been daunting until 2020. Adhemerval Zanella removed the last live glibc use case for `.gnu.linkonce.` [BZ #20543](http://sourceware.org/PR20543).

Cary Coutant et al implemented section groups and `GRP_COMDAT` on HP-UX before pushing the idea to the generic ABI committee. The design was a generalization of PE COMDAT. You can find the document *IA-64 gABI Issue 72: COMDAT* on the Internet. It lists some design consideration.

> Some sections occur in interrelated groups. For example, an out-of-line definition of an inline function might require, in addition to the section containing its executable instructions, a read-only data section containing literals referenced, one or more debugging information sections and other informational sections. Furthermore, there may be internal references among these sections that would not make sense if one of the sections were removed or replaced by a duplicate from another object. Therefore, such groups must be included or omitted from the linked object as a unit. A section cannot be a member of more than one group.

You may want to read the description before reading the following paragraphs.

According to "such groups must be included or omitted from the linked object as a unit", a linker's garbage collection feature must retain or discard the sections as a unit. This property can be leveraged by interrelated metadata sections.

Say `__llvm_prf_data` and `__llvm_prf_cnts` are used as interrelated metadata sections describing the text section `.text.a`. `__llvm_prf_data` has a relocation to `__llvm_prf_cnts`. `.text.a` and other text sections reference `__llvm_prf_cnts`. Without the "as a unit" semantics, `ld --gc-sections` may retain `__llvm_prf_cnts` while discarding `__llvm_prf_data`, since `__llvm_prf_data` is not referenced by a live section. The "as a unit" semantics allow `__llvm_prf_data` to be retained due to its interrelation with `__llvm_prf_cnts`.

A section group can contain more than 2 members. E.g. if the function in `.text.a` uses value profiling, there will be a third metadata section `__llvm_prf_vnds`, which is not referenced. The "as a unit" semantics allow `__llvm_prf_vnds` to be retained if `__llvm_prf_data` or `__llvm_prf_cnts` is retained.

> The section data of a SHT_GROUP section is an array of Elf32_Word entries. The first entry is a flag word. The remaining entries are a sequence of section header indices.

The most common section group flag is `GRP_COMDAT`, which makes the member sections similar to COMDAT in Microsoft PE file format, but can apply to multiple sections. (The committee borrowed the name "COMDAT" from PE.)

> `GRP_COMDAT` - This is a COMDAT group. It may duplicate another COMDAT group in another object file, where duplication is defined as having the same group signature. In such cases, only one of the duplicate groups may be retained by the linker, and the members of the remaining groups must be discarded.

The signature symbol provides a unique name for deduplication. The binding (local/global status) of the group signature is not part of the equation. This is not clear in the specification and has been clarified by [GRP_COMDAT group with STB_LOCAL signature](https://groups.google.com/g/generic-abi/c/2X6mR-s2zoc).

Example:

```c++
struct Data {
	int x;
	Data() { x = 1; }
};
template <typename T> Data data;

inline int test() {
	return data<int>.x;
}

int main() {
	return test();
}
```

```shell
readelf -grsSW test.o

There are 23 section headers, starting at offset 0x4d0:

节头：
  [Nr] Name              Type            Address          Off    Size   ES Flg Lk Inf Al
  [ 0]                   NULL            0000000000000000 000000 000000 00      0   0  0
  [ 1] .strtab           STRTAB          0000000000000000 0003dc 0000f0 00      0   0  1
  [ 2] .text             PROGBITS        0000000000000000 000040 000000 00  AX  0   0  4
  [ 3] .text.main        PROGBITS        0000000000000000 000040 00001a 00  AX  0   0 16
  [ 4] .rela.text.main   RELA            0000000000000000 0002d0 000018 18   I 22   3  8
  [ 5] .group            GROUP           0000000000000000 000180 00000c 04     22   8  4
  [ 6] .text._Z4testv    PROGBITS        0000000000000000 000060 00000c 00 AXG  0   0 16
  [ 7] .rela.text._Z4testv RELA            0000000000000000 0002e8 000018 18   G 22   6  8
  [ 8] .group            GROUP           0000000000000000 00018c 00001c 04     22   9  4
  [ 9] .text.startup     PROGBITS        0000000000000000 000070 000026 00 AXG  0   0 16
  [10] .rela.text.startup RELA            0000000000000000 000300 000060 18   G 22   9  8
  [11] .group            GROUP           0000000000000000 0001a8 000008 04     22  11  4
  [12] .text._ZN4DataC2Ev PROGBITS        0000000000000000 0000a0 000014 00 AXG  0   0 16
  [13] .bss._Z4dataIiE   NOBITS          0000000000000000 0000b4 000004 00 WAGR  0   0  4
  [14] .bss._ZGV4dataIiE NOBITS          0000000000000000 0000b8 000008 00 WAG  0   0  8
  [15] .init_array       INIT_ARRAY      0000000000000000 0000b8 000008 00 WAG  0   0  8
  [16] .rela.init_array  RELA            0000000000000000 000360 000018 18   G 22  15  8
  [17] .comment          PROGBITS        0000000000000000 0000c0 000028 01  MS  0   0  1
  [18] .note.GNU-stack   PROGBITS        0000000000000000 0000e8 000000 00      0   0  1
  [19] .eh_frame         X86_64_UNWIND   0000000000000000 0000e8 000098 00   A  0   0  8
  [20] .rela.eh_frame    RELA            0000000000000000 000378 000060 18   I 22  19  8
  [21] .llvm_addrsig     LOOS+0xfff4c03  0000000000000000 0003d8 000004 00   E 22   0  1
  [22] .symtab           SYMTAB          0000000000000000 0001b0 000120 18      1   7  8
Key to Flags:
  W (write), A (alloc), X (execute), M (merge), S (strings), I (info),
  L (link order), O (extra OS processing required), G (group), T (TLS),
  C (compressed), x (unknown), o (OS specific), E (exclude),
  R (retain), D (mbind), l (large), p (processor specific)

COMDAT group section [    5] `.group' [_Z4testv] contains 2 sections:
   [Index]    Name
   [    6]   .text._Z4testv
   [    7]   .rela.text._Z4testv

COMDAT group section [    8] `.group' [_Z4dataIiE] contains 6 sections:
   [Index]    Name
   [    9]   .text.startup
   [   10]   .rela.text.startup
   [   13]   .bss._Z4dataIiE
   [   14]   .bss._ZGV4dataIiE
   [   15]   .init_array
   [   16]   .rela.init_array

COMDAT group section [   11] `.group' [_ZN4DataC2Ev] contains 1 sections:
   [Index]    Name
   [   12]   .text._ZN4DataC2Ev

重定位节 '.rela.text.main' at offset 0x2d0 contains 1 entry:
    偏移量             信息             类型               符号值          符号名称 + 加数
0000000000000010  0000000800000004 R_X86_64_PLT32         0000000000000000 _Z4testv - 4

重定位节 '.rela.text._Z4testv' at offset 0x2e8 contains 1 entry:
    偏移量             信息             类型               符号值          符号名称 + 加数
0000000000000006  0000000900000002 R_X86_64_PC32          0000000000000000 _Z4dataIiE - 4

重定位节 '.rela.text.startup' at offset 0x300 contains 4 entries:
    偏移量             信息             类型               符号值          符号名称 + 加数
0000000000000006  0000000a00000002 R_X86_64_PC32          0000000000000000 _ZGV4dataIiE - 5
0000000000000013  0000000a00000002 R_X86_64_PC32          0000000000000000 _ZGV4dataIiE - 5
000000000000001b  0000000900000002 R_X86_64_PC32          0000000000000000 _Z4dataIiE - 4
0000000000000020  0000000b00000004 R_X86_64_PLT32         0000000000000000 _ZN4DataC2Ev - 4

重定位节 '.rela.init_array' at offset 0x360 contains 1 entry:
    偏移量             信息             类型               符号值          符号名称 + 加数
0000000000000000  0000000400000001 R_X86_64_64            0000000000000000 .text.startup + 0

重定位节 '.rela.eh_frame' at offset 0x378 contains 4 entries:
    偏移量             信息             类型               符号值          符号名称 + 加数
0000000000000020  0000000200000002 R_X86_64_PC32          0000000000000000 .text.main + 0
0000000000000040  0000000300000002 R_X86_64_PC32          0000000000000000 .text._Z4testv + 0
0000000000000060  0000000400000002 R_X86_64_PC32          0000000000000000 .text.startup + 0
0000000000000080  0000000600000002 R_X86_64_PC32          0000000000000000 .text._ZN4DataC2Ev + 0

Symbol table '.symtab' contains 12 entries:
   Num:    Value          Size Type    Bind   Vis      Ndx Name
     0: 0000000000000000     0 NOTYPE  LOCAL  DEFAULT  UND 
     1: 0000000000000000     0 FILE    LOCAL  DEFAULT  ABS test.cpp
     2: 0000000000000000     0 SECTION LOCAL  DEFAULT    3 .text.main
     3: 0000000000000000     0 SECTION LOCAL  DEFAULT    6 .text._Z4testv
     4: 0000000000000000     0 SECTION LOCAL  DEFAULT    9 .text.startup
     5: 0000000000000000    38 FUNC    LOCAL  DEFAULT    9 __cxx_global_var_init
     6: 0000000000000000     0 SECTION LOCAL  DEFAULT   12 .text._ZN4DataC2Ev
     7: 0000000000000000    26 FUNC    GLOBAL DEFAULT    3 main
     8: 0000000000000000    12 FUNC    WEAK   DEFAULT    6 _Z4testv
     9: 0000000000000000     4 OBJECT  WEAK   DEFAULT   13 _Z4dataIiE
    10: 0000000000000000     8 OBJECT  WEAK   DEFAULT   14 _ZGV4dataIiE
    11: 0000000000000000    20 FUNC    WEAK   DEFAULT   12 _ZN4DataC2Ev
```

I find that group will not include explicit dependencies, which are already specified in the relocation table, debug table or exception table, such as `test` to `x`. It will only include implicit dependencies, such as `.text`. `.rela`, derived `.bss`, derived `startup`, etc.,  presumably to save space.

# References

[1] https://tianshilei.me/comdat-in-llvm/

[2] https://maskray.me/blog/2021-07-25-comdat-and-section-group

[3] https://llvm.org/docs/LangRef.html#abstract

[4] https://docs.oracle.com/cd/E26502_01/html/E26507/chapter7-26.html#scrolltoc
