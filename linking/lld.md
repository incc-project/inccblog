相关内容还在施工中,想到什么写什么,比较乱.

源码版本: llvm-16.0.0

测试代码:

```c++
// test.cpp
int test() {
  return 1;
}

// main.cpp
int test();
int main() {
  return test();
}
```

# 入口

lld/tools/lld/lld.cpp::lld_main

lld/Common/DriverDispatcher.cpp::lld::unsafeLldMain

lld/ELF/Driver.cpp::lld:elf::link

lld/ELF/Driver.cpp::lld::elf::LinkerDriver::linkerMain

lld/ELF/Driver.cpp::lld::elf::LinkerDriver::LinkerDriver

整个ELF的链接流程就在这个LinkerDriver里.

这个函数以`wirteResult<ELFT>`为结尾, 将链接结果实际写入文件.

# 写入文件

来看一下ELF头部, 节头部表, 各个节, 以及其中的符号表, 重定位表怎么写入文件.

我们来到lld/ELF/Writer.cpp::Writer::run, 其尾部有如下代码:

```cpp
{
    llvm::TimeTraceScope timeScope("Write output file");
    ...
    writeHeader();
    writeSections();
    ...
}
```

这段代码就是输出ELF的主体逻辑.

我们关注其中的`writeHeader`和`writeSection`, 在这里应该能找到ELF的重要结构的打印方法.

`writeHeader`会创建ELF头部和节头部表:

```c++
template <class ELFT> void Writer<ELFT>::writeHeader() {
  writeEhdr<ELFT>(Out::bufferStart, *mainPart);
  writePhdrs<ELFT>(Out::bufferStart + sizeof(Elf_Ehdr), *mainPart);

  auto *eHdr = reinterpret_cast<Elf_Ehdr *>(Out::bufferStart);
  eHdr->e_type = getELFType();
  eHdr->e_entry = getEntryAddr();
  eHdr->e_shoff = sectionHeaderOff;

  // Write the section header table.
  //
  // The ELF header can only store numbers up to SHN_LORESERVE in the e_shnum
  // and e_shstrndx fields. When the value of one of these fields exceeds
  // SHN_LORESERVE ELF requires us to put sentinel values in the ELF header and
  // use fields in the section header at index 0 to store
  // the value. The sentinel values and fields are:
  // e_shnum = 0, SHdrs[0].sh_size = number of sections.
  // e_shstrndx = SHN_XINDEX, SHdrs[0].sh_link = .shstrtab section index.
  auto *sHdrs = reinterpret_cast<Elf_Shdr *>(Out::bufferStart + eHdr->e_shoff);
  size_t num = outputSections.size() + 1;
  if (num >= SHN_LORESERVE)
    sHdrs->sh_size = num;
  else
    eHdr->e_shnum = num;

  uint32_t strTabIndex = in.shStrTab->getParent()->sectionIndex;
  if (strTabIndex >= SHN_LORESERVE) {
    sHdrs->sh_link = strTabIndex;
    eHdr->e_shstrndx = SHN_XINDEX;
  } else {
    eHdr->e_shstrndx = strTabIndex;
  }

  for (OutputSection *sec : outputSections)
    sec->writeHeaderTo<ELFT>(++sHdrs);
}
```

可以看到其中ELF头部用`Elf_Ehdr`表示, 节头部表用`Elf_Shdr` 表示, 二者可以通过强转相应的地址来获取.

尝试在最后添加有关ELF头部和节头部表的打印代码:

```c++
  llvm::errs() << eHdr->e_entry << "\n";
  llvm::errs() << outputSections.size() << "\n";
  for (OutputSection *sec : outputSections) {
    llvm::errs() << sec->size << "\n";
  }

// results
// e_entry: 5712 (0x1650)
// |shdr|: 31
// sec[0]: 28
// sec[1]: 32
// ...
// 结果与readelf一致
```

接下来分析`Elf_Ehdr`和`Elf_Shdr`的结构.

根据调试信息, 这里的`eHdr`的实际类型是llvm/include/llvm/Object/ELFTypes.h::Elf_Ehdr_Impl, 结构如下:

```c++
template <class ELFT>
struct Elf_Ehdr_Impl {
  LLVM_ELF_IMPORT_TYPES_ELFT(ELFT)
  unsigned char e_ident[ELF::EI_NIDENT]; // ELF Identification bytes
  Elf_Half e_type;                       // Type of file (see ET_*)
  Elf_Half e_machine;   // Required architecture for this file (see EM_*)
  Elf_Word e_version;   // Must be equal to 1
  Elf_Addr e_entry;     // Address to jump to in order to start program
  Elf_Off e_phoff;      // Program header table's file offset, in bytes
  Elf_Off e_shoff;      // Section header table's file offset, in bytes
  Elf_Word e_flags;     // Processor-specific flags
  Elf_Half e_ehsize;    // Size of ELF header, in bytes
  Elf_Half e_phentsize; // Size of an entry in the program header table
  Elf_Half e_phnum;     // Number of entries in the program header table
  Elf_Half e_shentsize; // Size of an entry in the section header table
  Elf_Half e_shnum;     // Number of entries in the section header table
  Elf_Half e_shstrndx;  // Section header table index of section name
                        // string table

  bool checkMagic() const {
    return (memcmp(e_ident, ELF::ElfMagic, strlen(ELF::ElfMagic))) == 0;
  }

  unsigned char getFileClass() const { return e_ident[ELF::EI_CLASS]; }
  unsigned char getDataEncoding() const { return e_ident[ELF::EI_DATA]; }
};
```

同理, `sHdrs`的实际类型是llvm/include/llvm/Object/ELFTypes.h::Elf_Shdr_Impl, 结构如下:

```c++
template <class ELFT>
struct Elf_Shdr_Impl : Elf_Shdr_Base<ELFT> {
  using Elf_Shdr_Base<ELFT>::sh_entsize;
  using Elf_Shdr_Base<ELFT>::sh_size;

  /// Get the number of entities this section contains if it has any.
  unsigned getEntityCount() const {
    if (sh_entsize == 0)
      return 0;
    return sh_size / sh_entsize;
  }
};
```

当然, `sHdrs`同样表示一个数组的首地址.

`eHdr`和`sHdrs`的实例化类型是`llvm::object::ELFType<(llvm::endianness1, true)>`, 看上去是指定一些大小端序什么的.

总之以后如果想了解ELF的基础结构可以去llvm/include/llvm/Object/ELFTypes.h这个文件下查找.

接下来来看`writeSection`:

```c++
template <class ELFT> void Writer<ELFT>::writeSections() {
  llvm::TimeTraceScope timeScope("Write sections");

  {
    // In -r or --emit-relocs mode, write the relocation sections first as in
    // ELf_Rel targets we might find out that we need to modify the relocated
    // section while doing it.
    parallel::TaskGroup tg;
    for (OutputSection *sec : outputSections)
      if (isStaticRelSecType(sec->type))
        sec->writeTo<ELFT>(Out::bufferStart + sec->offset, tg);
  }
  {
    parallel::TaskGroup tg;
    for (OutputSection *sec : outputSections)
      if (!isStaticRelSecType(sec->type))
        sec->writeTo<ELFT>(Out::bufferStart + sec->offset, tg);
  }

  // Finally, check that all dynamic relocation addends were written correctly.
  if (config->checkDynamicRelocs && config->writeAddends) {
    for (OutputSection *sec : outputSections)
      if (isStaticRelSecType(sec->type))
        sec->checkDynRelAddends(Out::bufferStart);
  }
}
```

这里会将各个节的内容写入文件.

# 符号解析

`Driver`一开始的`parseFiles`中有关于符号表的处理:

lld/ELF/InputFiles.cpp::lld::elf::ObjFile::initializeSymbols

lld/ELF/InputFiles.cpp::lld::elf::ObjFile::parse

lld/ELF/InputFiles.cpp::doParseFile

lld/ELF/InputFiles.cpp::doParseFile

lld/ELF/Driver.cpp::lld::elf::LinkerDriver::link

我们先来分析`initializeSymbols`所属的`ObjFile`类, 这个类是用于描述.o文件的, 继承自`ELFFileBase`. `ELFFileBase`具备以下属性, 看起来和符号表有关:

```c++
class ELFFileBase : public InputFile {
protected:
  StringRef stringTable;
  const void *elfShdrs = nullptr;
  const void *elfSyms = nullptr;
  uint32_t numELFShdrs = 0;
  uint32_t numELFSyms = 0;
  uint32_t firstGlobal = 0;
};
```

`ELFFileBase`继承自`InputFile`. `InputFile`具备以下属性:

```c++
// The root class of input files.
class InputFile {
protected:
  std::unique_ptr<Symbol *[]> symbols;
  uint32_t numSymbols = 0;
  SmallVector<InputSectionBase *, 0> sections;
  ...
}
```

当然还有一些和架构什么的有关的属性, 不过这里我们关注`symbols`, 看上去也和符号表有关.

猜测`symbols`是解析后的符号, `elfSyms`代表存储符号表那个节. 

我们继续来看`initializeSymbols`.

```c++
oid ObjFile<ELFT>::initializeSymbols(const object::ELFFile<ELFT> &obj) {
  ArrayRef<Elf_Sym> eSyms = this->getELFSyms<ELFT>();
  ...
  SmallVector<unsigned, 32> undefineds;
  for (size_t i = firstGlobal, end = eSyms.size(); i != end; ++i) {
    const Elf_Sym &eSym = eSyms[i];
    uint32_t secIdx = eSym.st_shndx;
    if (secIdx == SHN_UNDEF) {
      undefineds.push_back(i);
      continue;
    }

    uint8_t binding = eSym.getBinding();
    uint8_t stOther = eSym.st_other;
    uint8_t type = eSym.getType();
    uint64_t value = eSym.st_value;
    uint64_t size = eSym.st_size;

    Symbol *sym = symbols[i];
    ...
    // Handle global defined symbols. Defined::section will be set in postParse.
    sym->resolve(Defined{this, StringRef(), binding, stOther, type, value, size,
                         nullptr});
  }
  ...
  for (unsigned i : undefineds) {
    const Elf_Sym &eSym = eSyms[i];
    Symbol *sym = symbols[i];
    sym->resolve(Undefined{this, StringRef(), eSym.getBinding(), eSym.st_other,
                           eSym.getType()});
    sym->isUsedInRegularObj = true;
    sym->referenced = true;
  }
}
```

首先, `initializeSymbols`会通过`this->getELFSyms<ELFT>();`获取`Elf_Sym`数组格式的符号表. `this->getELFSyms<ELFT>();`实际上会强转`elfSyms`, 这和我们之前猜测的一样 (`symbols`是解析后的符号, `elfSyms`代表存储符号表那个节).

接下来, `initializeSymbols`会分别解析定义符号和未定义符号(各种位运算), 并存储在`symbols`中.

`Elf_Sym`的结构如下:

```c++
template <class ELFT>
struct Elf_Sym_Impl : Elf_Sym_Base<ELFT> {
  using Elf_Sym_Base<ELFT>::st_info;
  using Elf_Sym_Base<ELFT>::st_shndx;
  using Elf_Sym_Base<ELFT>::st_other;
  using Elf_Sym_Base<ELFT>::st_value;
  ...
}
```

另外`Symbol`的结构如下:

```c++
class Symbol {
  ...
protected:
  const char *nameData;
  // 32-bit size saves space.
  uint32_t nameSize;

public:
  // The next three fields have the same meaning as the ELF symbol attributes.
  // type and binding are placed in this order to optimize generating st_info,
  // which is defined as (binding << 4) + (type & 0xf), on a little-endian
  // system.
  uint8_t type : 4; // symbol type

  // Symbol binding. This is not overwritten by replace() to track
  // changes during resolution. In particular:
  //  - An undefined weak is still weak when it resolves to a shared library.
  //  - An undefined weak will not extract archive members, but we have to
  //    remember it is weak.
  uint8_t binding : 4;

  uint8_t stOther; // st_other field value

  uint8_t symbolKind;
  ...
}
```

标准的字符串表结构.

接下来尝试在这个地方打印符号表, 并尝试添加一个新的符号.

在`initializeSymbols`底部添加如下打印语句:

```c++
  if (config->relocatable) {
      for (size_t i = firstGlobal; i < numSymbols; i++) {
        Symbol *sym = symbols[i];
        llvm::errs() << sym->getName() << " " << (int)sym->binding << "\n";
      }
  }
  
// results
// main 1
// _Z4testv 1
// ...
```

我们发现其实在parse时, 相关符号已经读入到ObjFIle中了, 这段逻辑最初是在哪里实现的呢?

`InputFile`是通过`createObjFile`创建的, 调用栈如下:

lld/ELF/InputFiles.cpp::lld::elf::createObjFile

lld/ELF/Driver.cpp::lld::elf::LinkerDriver::addFile

lld/ELF/Driver.cpp::lld::elf::LinkerDriver::createFiles

lld/ELF/Driver.cpp::lld::elf::LinkerDriver::linkerMain

创建文件过程中一个非常重要的操作是初始化, 代码如下:

```c++
template <class ELFT> void ELFFileBase::init(InputFile::Kind k) {
  using Elf_Shdr = typename ELFT::Shdr;
  using Elf_Sym = typename ELFT::Sym;

  // Initialize trivial attributes.
  const ELFFile<ELFT> &obj = getObj<ELFT>();
  emachine = obj.getHeader().e_machine;
  osabi = obj.getHeader().e_ident[llvm::ELF::EI_OSABI];
  abiVersion = obj.getHeader().e_ident[llvm::ELF::EI_ABIVERSION];

  ArrayRef<Elf_Shdr> sections = CHECK(obj.sections(), this);
  elfShdrs = sections.data();
  numELFShdrs = sections.size();

  // Find a symbol table.
  const Elf_Shdr *symtabSec =
      findSection(sections, k == SharedKind ? SHT_DYNSYM : SHT_SYMTAB);

  if (!symtabSec)
    return;

  // Initialize members corresponding to a symbol table.
  firstGlobal = symtabSec->sh_info;

  ArrayRef<Elf_Sym> eSyms = CHECK(obj.symbols(symtabSec), this);
  if (firstGlobal == 0 || firstGlobal > eSyms.size())
    fatal(toString(this) + ": invalid sh_info in symbol table");

  elfSyms = reinterpret_cast<const void *>(eSyms.data());
  numELFSyms = uint32_t(eSyms.size());
  stringTable = CHECK(obj.getStringTableForSymtab(*symtabSec, sections), this);
}
```

<font color="red">可以看到这里解析了ELF头, 节头部表, 符号表, 字符串表等. 每个解析函数里其实都是一些位运算操作. </font>

# 重定位

重定位过程在最后的写入逻辑中 (`lld/ELF/Writer.cpp::Writer<ELFT>::finalizeSections`:

```cpp
  if (!config->relocatable) {
    llvm::TimeTraceScope timeScope("Scan relocations");
    // Scan relocations. This must be done after every symbol is declared so
    // that we can correctly decide if a dynamic relocation is needed. This is
    // called after processSymbolAssignments() because it needs to know whether
    // a linker-script-defined symbol is absolute.
    ppc64noTocRelax.clear();
    scanRelocations<ELFT>();
    reportUndefinedSymbols();
    postScanRelocations();

    if (in.plt && in.plt->isNeeded())
      in.plt->addSymbols();
    if (in.iplt && in.iplt->isNeeded())
      in.iplt->addSymbols();

    if (config->unresolvedSymbolsInShlib != UnresolvedPolicy::Ignore) {
      auto diagnose =
          config->unresolvedSymbolsInShlib == UnresolvedPolicy::ReportError
              ? errorOrWarn
              : warn;
      // Error on undefined symbols in a shared object, if all of its DT_NEEDED
      // entries are seen. These cases would otherwise lead to runtime errors
      // reported by the dynamic linker.
      //
      // ld.bfd traces all DT_NEEDED to emulate the logic of the dynamic linker
      // to catch more cases. That is too much for us. Our approach resembles
      // the one used in ld.gold, achieves a good balance to be useful but not
      // too smart.
      for (SharedFile *file : ctx.sharedFiles) {
        bool allNeededIsKnown =
            llvm::all_of(file->dtNeeded, [&](StringRef needed) {
              return symtab.soNames.count(CachedHashStringRef(needed));
            });
        if (!allNeededIsKnown)
          continue;
        for (Symbol *sym : file->requiredSymbols)
          if (sym->isUndefined() && !sym->isWeak())
            diagnose("undefined reference due to --no-allow-shlib-undefined: " +
                     toString(*sym) + "\n>>> referenced by " + toString(file));
      }
    }
  }
```

真正的地址写入在这里 (`lld/ELF/Arch/X86_64.cpp::X86_64::relocate`):

```c++
void X86_64::relocate(uint8_t *loc, const Relocation &rel, uint64_t val) const {
  switch (rel.type) {
  case R_X86_64_8:
    checkIntUInt(loc, val, 8, rel);
    *loc = val;
    break;
  case R_X86_64_PC8:
    checkInt(loc, val, 8, rel);
    *loc = val;
    break;
  case R_X86_64_16:
  ...
}
```

具体过程以后再来调试.

另外补充一下-r模式下的重定位合并:

-r模式下，重定位表的解析不在一开始的节解析中，而是在最后的写入阶段。lld会首先并行的调用OutputSection的writeTo函数，进而通过SectionCommand拿到其对应的InputSection（注意OutputSectionDesc和InputSectionDesc都继承自SectionCommand，所以可以比较方便地转化），接着调用InputSection的copyRelocations函数进行重定位表的解析，调整与写入：

```c++
// This is used for -r and --emit-relocs. We can't use memcpy to copy
// relocations because we need to update symbol table offset and section index
// for each relocation. So we copy relocations one by one.
template <class ELFT, class RelTy>
void InputSection::copyRelocations(uint8_t *buf, ArrayRef<RelTy> rels) {
  const TargetInfo &target = *elf::target;
  InputSectionBase *sec = getRelocatedSection();
  (void)sec->contentMaybeDecompress(); // uncompress if needed

  for (const RelTy &rel : rels) {
    RelType type = rel.getType(config->isMips64EL);
    const ObjFile<ELFT> *file = getFile<ELFT>();
    Symbol &sym = file->getRelocTargetSym(rel);

    auto *p = reinterpret_cast<typename ELFT::Rela *>(buf);
    buf += sizeof(RelTy);

    if (RelTy::IsRela)
      p->r_addend = getAddend<ELFT>(rel);

    // Output section VA is zero for -r, so r_offset is an offset within the
    // section, but for --emit-relocs it is a virtual address.
    p->r_offset = sec->getVA(rel.r_offset);
    p->setSymbolAndType(in.symTab->getSymbolIndex(&sym), type,
                        config->isMips64EL);

    if (sym.type == STT_SECTION) {
      // We combine multiple section symbols into only one per
      // section. This means we have to update the addend. That is
      // trivial for Elf_Rela, but for Elf_Rel we have to write to the
      // section data. We do that by adding to the Relocation vector.

      // .eh_frame is horribly special and can reference discarded sections. To
      // avoid having to parse and recreate .eh_frame, we just replace any
      // relocation in it pointing to discarded sections with R_*_NONE, which
      // hopefully creates a frame that is ignored at runtime. Also, don't warn
      // on .gcc_except_table and debug sections.
      //
      // See the comment in maybeReportUndefined for PPC32 .got2 and PPC64 .toc
      auto *d = dyn_cast<Defined>(&sym);
      if (!d) {
        if (!isDebugSection(*sec) && sec->name != ".eh_frame" &&
            sec->name != ".gcc_except_table" && sec->name != ".got2" &&
            sec->name != ".toc") {
          uint32_t secIdx = cast<Undefined>(sym).discardedSecIdx;
          Elf_Shdr_Impl<ELFT> sec = file->template getELFShdrs<ELFT>()[secIdx];
          warn("relocation refers to a discarded section: " +
               CHECK(file->getObj().getSectionName(sec), file) +
               "\n>>> referenced by " + getObjMsg(p->r_offset));
        }
        p->setSymbolAndType(0, 0, false);
        continue;
      }
      SectionBase *section = d->section;
      if (!section->isLive()) {
        p->setSymbolAndType(0, 0, false);
        continue;
      }

      int64_t addend = getAddend<ELFT>(rel);
      const uint8_t *bufLoc = sec->content().begin() + rel.r_offset;
      if (!RelTy::IsRela)
        addend = target.getImplicitAddend(bufLoc, type);

      if (config->emachine == EM_MIPS &&
          target.getRelExpr(type, sym, bufLoc) == R_MIPS_GOTREL) {
        // Some MIPS relocations depend on "gp" value. By default,
        // this value has 0x7ff0 offset from a .got section. But
        // relocatable files produced by a compiler or a linker
        // might redefine this default value and we must use it
        // for a calculation of the relocation result. When we
        // generate EXE or DSO it's trivial. Generating a relocatable
        // output is more difficult case because the linker does
        // not calculate relocations in this mode and loses
        // individual "gp" values used by each input object file.
        // As a workaround we add the "gp" value to the relocation
        // addend and save it back to the file.
        addend += sec->getFile<ELFT>()->mipsGp0;
      }

      if (RelTy::IsRela)
        p->r_addend = sym.getVA(addend) - section->getOutputSection()->addr;
      else if (config->relocatable && type != target.noneRel)
        sec->addReloc({R_ABS, type, rel.r_offset, addend, &sym});
    } else if (config->emachine == EM_PPC && type == R_PPC_PLTREL24 &&
               p->r_addend >= 0x8000 && sec->file->ppc32Got2) {
      // Similar to R_MIPS_GPREL{16,32}. If the addend of R_PPC_PLTREL24
      // indicates that r30 is relative to the input section .got2
      // (r_addend>=0x8000), after linking, r30 should be relative to the output
      // section .got2 . To compensate for the shift, adjust r_addend by
      // ppc32Got->outSecOff.
      p->r_addend += sec->file->ppc32Got2->outSecOff;
    }
  }
}
```

这里加数的计算看上去比较复杂, 但实际在-r模式下都是走的Synthetic类型的节的计算, 只会用到符号自身的Value以及InputSection在OutputSection中的偏移, 然后加上原有加数, 不会有什么特殊的计算.

# 输入到输出

接下来分析lld如何根据`InputFile`生成并写入目标文件.

回想`writeHeader`时, 我们的写入数据是从这里来的:

```c++
writeEhdr<ELFT>(Out::bufferStart, *mainPart);
```

这里我们不关注lld中`mainPart`是怎么来的, 可以用我们自己的文件指针代替.

关于ELF头部中一些索引的计算, 之前已经介绍过了.

接下来我们看看节头部表的的索引和便宜怎么计算.

注意`writeSections`是用来写各个节的内容的, 节头部表的写入也是在`writeHeader`中完成的, 具体是这个位置:

```c++
  for (OutputSection *sec : outputSections)
    sec->writeHeaderTo<ELFT>(++sHdrs);
```

`OutputSection::writeHeaderTo`的定义如下:

```c++
template <class ELFT>
void OutputSection::writeHeaderTo(typename ELFT::Shdr *shdr) {
  shdr->sh_entsize = entsize;
  shdr->sh_addralign = addralign;
  shdr->sh_type = type;
  shdr->sh_offset = offset;
  shdr->sh_flags = flags;
  shdr->sh_info = info;
  shdr->sh_link = link;
  shdr->sh_addr = addr;
  shdr->sh_size = size;
  shdr->sh_name = shName;
}
```

看起来这些值已经提前在`OutputSection`中计算好了, 我们去看看它们是在哪里计算的以及怎么计算的.

以`addralign`为例, 其更新逻辑位于:

```c++
// Update fields (type, flags, alignment, etc) according to the InputSection
// isec. Also check whether the InputSection flags and type are consistent with
// other InputSections.
void OutputSection::commitSection(InputSection *isec) {
  if (LLVM_UNLIKELY(type != isec->type)) {
    if (hasInputSections || typeIsSet) {
      if (typeIsSet || !canMergeToProgbits(type) ||
          !canMergeToProgbits(isec->type)) {
        // The (NOLOAD) changes the section type to SHT_NOBITS, the intention is
        // that the contents at that address is provided by some other means.
        // Some projects (e.g.
        // https://github.com/ClangBuiltLinux/linux/issues/1597) rely on the
        // behavior. Other types get an error.
        if (type != SHT_NOBITS) {
          errorOrWarn("section type mismatch for " + isec->name + "\n>>> " +
                      toString(isec) + ": " +
                      getELFSectionTypeName(config->emachine, isec->type) +
                      "\n>>> output section " + name + ": " +
                      getELFSectionTypeName(config->emachine, type));
        }
      }
      if (!typeIsSet)
        type = SHT_PROGBITS;
    } else {
      type = isec->type;
    }
  }
  if (!hasInputSections) {
    // If IS is the first section to be added to this section,
    // initialize type, entsize and flags from isec.
    hasInputSections = true;
    entsize = isec->entsize;
    flags = isec->flags;
  } else {
    // Otherwise, check if new type or flags are compatible with existing ones.
    if ((flags ^ isec->flags) & SHF_TLS)
      error("incompatible section flags for " + name + "\n>>> " +
            toString(isec) + ": 0x" + utohexstr(isec->flags) +
            "\n>>> output section " + name + ": 0x" + utohexstr(flags));
  }

  isec->parent = this;
  uint64_t andMask =
      config->emachine == EM_ARM ? (uint64_t)SHF_ARM_PURECODE : 0;
  uint64_t orMask = ~andMask;
  uint64_t andFlags = (flags & isec->flags) & andMask;
  uint64_t orFlags = (flags | isec->flags) & orMask;
  flags = andFlags | orFlags;
  if (nonAlloc)
    flags &= ~(uint64_t)SHF_ALLOC;

  addralign = std::max(addralign, isec->addralign);

  // If this section contains a table of fixed-size entries, sh_entsize
  // holds the element size. If it contains elements of different size we
  // set sh_entsize to 0.
  if (entsize != isec->entsize)
    entsize = 0;
}
```

调用栈为:

lld/ELF/OutputSections.cpp::lld::elf::OutputSection::commitSection

lld/ELF/OutputSections.cpp::lld::elf::OutputSection::finalizeInputSections

lld/ELF/Driver.cpp::lld:elf::link

```c++
  {
    llvm::TimeTraceScope timeScope("Merge/finalize input sections");

    // Migrate InputSectionDescription::sectionBases to sections. This includes
    // merging MergeInputSections into a single MergeSyntheticSection. From this
    // point onwards InputSectionDescription::sections should be used instead of
    // sectionBases.
    for (SectionCommand *cmd : script->sectionCommands)
      if (auto *osd = dyn_cast<OutputDesc>(cmd))
        osd->osec.finalizeInputSections();
  }
```

finalizeInputSections的代码如下:

```cpp
// Update fields (type, flags, alignment, etc) according to the InputSection
// isec. Also check whether the InputSection flags and type are consistent with
// other InputSections.
void OutputSection::commitSection(InputSection *isec) {
  if (LLVM_UNLIKELY(type != isec->type)) {
    if (hasInputSections || typeIsSet) {
      if (typeIsSet || !canMergeToProgbits(type) ||
          !canMergeToProgbits(isec->type)) {
        // The (NOLOAD) changes the section type to SHT_NOBITS, the intention is
        // that the contents at that address is provided by some other means.
        // Some projects (e.g.
        // https://github.com/ClangBuiltLinux/linux/issues/1597) rely on the
        // behavior. Other types get an error.
        if (type != SHT_NOBITS) {
          errorOrWarn("section type mismatch for " + isec->name + "\n>>> " +
                      toString(isec) + ": " +
                      getELFSectionTypeName(config->emachine, isec->type) +
                      "\n>>> output section " + name + ": " +
                      getELFSectionTypeName(config->emachine, type));
        }
      }
      if (!typeIsSet)
        type = SHT_PROGBITS;
    } else {
      type = isec->type;
    }
  }
  if (!hasInputSections) {
    // If IS is the first section to be added to this section,
    // initialize type, entsize and flags from isec.
    hasInputSections = true;
    entsize = isec->entsize;
    flags = isec->flags;
  } else {
    // Otherwise, check if new type or flags are compatible with existing ones.
    if ((flags ^ isec->flags) & SHF_TLS)
      error("incompatible section flags for " + name + "\n>>> " +
            toString(isec) + ": 0x" + utohexstr(isec->flags) +
            "\n>>> output section " + name + ": 0x" + utohexstr(flags));
  }

  isec->parent = this;
  uint64_t andMask =
      config->emachine == EM_ARM ? (uint64_t)SHF_ARM_PURECODE : 0;
  uint64_t orMask = ~andMask;
  uint64_t andFlags = (flags & isec->flags) & andMask;
  uint64_t orFlags = (flags | isec->flags) & orMask;
  flags = andFlags | orFlags;
  if (nonAlloc)
    flags &= ~(uint64_t)SHF_ALLOC;

  addralign = std::max(addralign, isec->addralign);

  // If this section contains a table of fixed-size entries, sh_entsize
  // holds the element size. If it contains elements of different size we
  // set sh_entsize to 0.
  if (entsize != isec->entsize)
    entsize = 0;
}
```

基本就是把`InputSection`的东西复制过来.

不过里面并没有涉及到offset的计算, offset的计算应该是受`addralign`影响的, 我们找一下在哪里.

`finalizeInputSections`最后还会调用`finalizeContents`, 用来计算offset, 其定义如下:

```c++
// lld/ELF/SyntheticSections.cpp

// This function is very hot (i.e. it can take several seconds to finish)
// because sometimes the number of inputs is in an order of magnitude of
// millions. So, we use multi-threading.
//
// For any strings S and T, we know S is not mergeable with T if S's hash
// value is different from T's. If that's the case, we can safely put S and
// T into different string builders without worrying about merge misses.
// We do it in parallel.
void MergeNoTailSection::finalizeContents() {
  // Initializes string table builders.
  for (size_t i = 0; i < numShards; ++i)
    shards.emplace_back(StringTableBuilder::RAW, llvm::Align(addralign));

  // Concurrency level. Must be a power of 2 to avoid expensive modulo
  // operations in the following tight loop.
  const size_t concurrency =
      llvm::bit_floor(std::min<size_t>(config->threadCount, numShards));

  // Add section pieces to the builders.
  parallelFor(0, concurrency, [&](size_t threadId) {
    for (MergeInputSection *sec : sections) {
      for (size_t i = 0, e = sec->pieces.size(); i != e; ++i) {
        if (!sec->pieces[i].live)
          continue;
        size_t shardId = getShardId(sec->pieces[i].hash);
        if ((shardId & (concurrency - 1)) == threadId)
          sec->pieces[i].outputOff = shards[shardId].add(sec->getData(i));
      }
    }
  });

  // Compute an in-section offset for each shard.
  size_t off = 0;
  for (size_t i = 0; i < numShards; ++i) {
    shards[i].finalizeInOrder();
    if (shards[i].getSize() > 0)
      off = alignToPowerOf2(off, addralign);
    shardOffsets[i] = off;
    off += shards[i].getSize();
  }
  size = off;

  // So far, section pieces have offsets from beginning of shards, but
  // we want offsets from beginning of the whole section. Fix them.
  parallelForEach(sections, [&](MergeInputSection *sec) {
    for (size_t i = 0, e = sec->pieces.size(); i != e; ++i)
      if (sec->pieces[i].live)
        sec->pieces[i].outputOff +=
            shardOffsets[getShardId(sec->pieces[i].hash)];
  });
}
```

可以看到offset在计算过程中会不断对齐addralign模2.

上述分析还漏了一点, 那就是InputSection和OutputSeciton的转换, 相关逻辑在这里, 通过Linker Script的Section Command中转了一下:

```c++
// lld/ELF/Driver.cpp::LinkerDriver::link
// lld/ELF/LinkerScript.cpp::LinkerScript::addOrphanSections
void LinkerScript::addOrphanSections() {
  StringMap<TinyPtrVector<OutputSection *>> map;
  SmallVector<OutputDesc *, 0> v;

  auto add = [&](InputSectionBase *s) {
    if (s->isLive() && !s->parent) {
      orphanSections.push_back(s);

      StringRef name = getOutputSectionName(s);
      if (config->unique) {
        v.push_back(createSection(s, name));
      } else if (OutputSection *sec = findByName(sectionCommands, name)) {
        sec->recordSection(s);
      } else {
        if (OutputDesc *osd = addInputSec(map, s, name))
          v.push_back(osd);
        assert(isa<MergeInputSection>(s) ||
               s->getOutputSection()->sectionIndex == UINT32_MAX);
      }
    }
  };

  // For further --emit-reloc handling code we need target output section
  // to be created before we create relocation output section, so we want
  // to create target sections first. We do not want priority handling
  // for synthetic sections because them are special.
  size_t n = 0;
  for (InputSectionBase *isec : ctx.inputSections) {
    // Process InputSection and MergeInputSection.
    if (LLVM_LIKELY(isa<InputSection>(isec)))
      ctx.inputSections[n++] = isec;

    // In -r links, SHF_LINK_ORDER sections are added while adding their parent
    // sections because we need to know the parent's output section before we
    // can select an output section for the SHF_LINK_ORDER section.
    if (config->relocatable && (isec->flags & SHF_LINK_ORDER))
      continue;

    if (auto *sec = dyn_cast<InputSection>(isec))
      if (InputSectionBase *rel = sec->getRelocatedSection())
        if (auto *relIS = dyn_cast_or_null<InputSectionBase>(rel->parent))
          add(relIS);
    add(isec);
    if (config->relocatable)
      for (InputSectionBase *depSec : isec->dependentSections)
        if (depSec->flags & SHF_LINK_ORDER)
          add(depSec);
  }
  // Keep just InputSection.
  ctx.inputSections.resize(n);

  // If no SECTIONS command was given, we should insert sections commands
  // before others, so that we can handle scripts which refers them,
  // for example: "foo = ABSOLUTE(ADDR(.text)));".
  // When SECTIONS command is present we just add all orphans to the end.
  if (hasSectionsCommand)
    sectionCommands.insert(sectionCommands.end(), v.begin(), v.end());
  else
    sectionCommands.insert(sectionCommands.begin(), v.begin(), v.end());
}
```

然后通过`lld/ELF/Writer.cpp::Writer<ELFT>::finalizeSections`将SectionCommand转换成了OutputSection:

```c++
// Create a list of OutputSections, assign sectionIndex, and populate
  // in.shStrTab.
  for (SectionCommand *cmd : script->sectionCommands)
    if (auto *osd = dyn_cast<OutputDesc>(cmd)) {
      OutputSection *osec = &osd->osec;
      outputSections.push_back(osec);
      osec->sectionIndex = outputSections.size();
      osec->shName = in.shStrTab->addString(osec->name);
    }

  // Prefer command line supplied address over other constraints.
  for (OutputSection *sec : outputSections) {
    auto i = config->sectionStartMap.find(sec->name);
    if (i != config->sectionStartMap.end())
      sec->addrExpr = [=] { return i->second; };
  }
```

# 并行写入

并行写入逻辑(`lld/ELF/Writer.cpp`):

```c++
// Write section contents to a mmap'ed file.
template <class ELFT> void Writer<ELFT>::writeSections() {
  llvm::TimeTraceScope timeScope("Write sections");

  {
    // In -r or --emit-relocs mode, write the relocation sections first as in
    // ELf_Rel targets we might find out that we need to modify the relocated
    // section while doing it.
    parallel::TaskGroup tg;
    for (OutputSection *sec : outputSections)
      if (sec->type == SHT_REL || sec->type == SHT_RELA)
        sec->writeTo<ELFT>(Out::bufferStart + sec->offset, tg);
  }
  {
    parallel::TaskGroup tg;
    for (OutputSection *sec : outputSections)
      if (sec->type != SHT_REL && sec->type != SHT_RELA)
        sec->writeTo<ELFT>(Out::bufferStart + sec->offset, tg);
  }

  // Finally, check that all dynamic relocation addends were written correctly.
  if (config->checkDynamicRelocs && config->writeAddends) {
    for (OutputSection *sec : outputSections)
      if (sec->type == SHT_REL || sec->type == SHT_RELA)
        sec->checkDynRelAddends(Out::bufferStart);
  }
}
```

这里会创建TaskGroup, 多线程执行writeTo.

# 兼容性模板

来看下lld是如何通过模板来实现ELF32/64, 以及大小端序的兼容的:

```c++
template <class ELFT>
struct Elf_Ehdr_Impl {
  LLVM_ELF_IMPORT_TYPES_ELFT(ELFT)
  unsigned char e_ident[ELF::EI_NIDENT]; // ELF Identification bytes
  Elf_Half e_type;                       // Type of file (see ET_*)
  ...
```

首先每个ELF结构都会注入一行`LLVM_ELF_IMPORT_TYPES_ELFT(ELFT)`, 定义如下:

```c++
// I really don't like doing this, but the alternative is copypasta.
#define LLVM_ELF_IMPORT_TYPES_ELFT(ELFT)                                       \
  using Elf_Addr = typename ELFT::Addr;                                        \
  using Elf_Off = typename ELFT::Off;                                          \
  using Elf_Half = typename ELFT::Half;                                        \
  using Elf_Word = typename ELFT::Word;                                        \
  using Elf_Sword = typename ELFT::Sword;                                      \
  using Elf_Xword = typename ELFT::Xword;                                      \
  using Elf_Sxword = typename ELFT::Sxword;                                    \
  using uintX_t = typename ELFT::uint;                                         \
  using Elf_Ehdr = typename ELFT::Ehdr;                                        \
  using Elf_Shdr = typename ELFT::Shdr;                                        \
  using Elf_Sym = typename ELFT::Sym;                                          \
  using Elf_Dyn = typename ELFT::Dyn;                                          \
  using Elf_Phdr = typename ELFT::Phdr;                                        \
  using Elf_Rel = typename ELFT::Rel;                                          \
  using Elf_Rela = typename ELFT::Rela;                                        \
  using Elf_Relr = typename ELFT::Relr;                                        \
  using Elf_Verdef = typename ELFT::Verdef;                                    \
  using Elf_Verdaux = typename ELFT::Verdaux;                                  \
  using Elf_Verneed = typename ELFT::Verneed;                                  \
  using Elf_Vernaux = typename ELFT::Vernaux;                                  \
  using Elf_Versym = typename ELFT::Versym;                                    \
  using Elf_Hash = typename ELFT::Hash;                                        \
  using Elf_GnuHash = typename ELFT::GnuHash;                                  \
  using Elf_Chdr = typename ELFT::Chdr;                                        \
  using Elf_Nhdr = typename ELFT::Nhdr;                                        \
  using Elf_Note = typename ELFT::Note;                                        \
  using Elf_Note_Iterator = typename ELFT::NoteIterator;                       \
  using Elf_CGProfile = typename ELFT::CGProfile;                              \
  using Elf_Dyn_Range = typename ELFT::DynRange;                               \
  using Elf_Shdr_Range = typename ELFT::ShdrRange;                             \
  using Elf_Sym_Range = typename ELFT::SymRange;                               \
  using Elf_Rel_Range = typename ELFT::RelRange;                               \
  using Elf_Rela_Range = typename ELFT::RelaRange;                             \
  using Elf_Relr_Range = typename ELFT::RelrRange;                             \
  using Elf_Phdr_Range = typename ELFT::PhdrRange;
```

`ELFT`在实际使用过程中会被实例化成`ELF64LE`, `ELF64LE`等, 它们都会继承`ELFType`, 定义如下:

```c++
template <endianness E, bool Is64> struct ELFType {
private:
  template <typename Ty>
  using packed = support::detail::packed_endian_specific_integral<Ty, E, 1>;

public:
  static const endianness TargetEndianness = E;
  static const bool Is64Bits = Is64;

  using uint = std::conditional_t<Is64, uint64_t, uint32_t>;
  using Ehdr = Elf_Ehdr_Impl<ELFType<E, Is64>>;
  using Shdr = Elf_Shdr_Impl<ELFType<E, Is64>>;
  using Sym = Elf_Sym_Impl<ELFType<E, Is64>>;
  using Dyn = Elf_Dyn_Impl<ELFType<E, Is64>>;
  using Phdr = Elf_Phdr_Impl<ELFType<E, Is64>>;
  using Rel = Elf_Rel_Impl<ELFType<E, Is64>, false>;
  using Rela = Elf_Rel_Impl<ELFType<E, Is64>, true>;
  using Relr = packed<uint>;
  using Verdef = Elf_Verdef_Impl<ELFType<E, Is64>>;
  using Verdaux = Elf_Verdaux_Impl<ELFType<E, Is64>>;
  using Verneed = Elf_Verneed_Impl<ELFType<E, Is64>>;
  using Vernaux = Elf_Vernaux_Impl<ELFType<E, Is64>>;
  using Versym = Elf_Versym_Impl<ELFType<E, Is64>>;
  using Hash = Elf_Hash_Impl<ELFType<E, Is64>>;
  using GnuHash = Elf_GnuHash_Impl<ELFType<E, Is64>>;
  using Chdr = Elf_Chdr_Impl<ELFType<E, Is64>>;
  using Nhdr = Elf_Nhdr_Impl<ELFType<E, Is64>>;
  using Note = Elf_Note_Impl<ELFType<E, Is64>>;
  using NoteIterator = Elf_Note_Iterator_Impl<ELFType<E, Is64>>;
  using CGProfile = Elf_CGProfile_Impl<ELFType<E, Is64>>;
  using DynRange = ArrayRef<Dyn>;
  using ShdrRange = ArrayRef<Shdr>;
  using SymRange = ArrayRef<Sym>;
  using RelRange = ArrayRef<Rel>;
  using RelaRange = ArrayRef<Rela>;
  using RelrRange = ArrayRef<Relr>;
  using PhdrRange = ArrayRef<Phdr>;

  using Half = packed<uint16_t>;
  using Word = packed<uint32_t>;
  using Sword = packed<int32_t>;
  using Xword = packed<uint64_t>;
  using Sxword = packed<int64_t>;
  using Addr = packed<uint>;
  using Off = packed<uint>;
};

using ELF32LE = ELFType<support::little, false>;
using ELF32BE = ELFType<support::big, false>;
using ELF64LE = ELFType<support::little, true>;
using ELF64BE = ELFType<support::big, true>;
```

注意`Addr`和`Off`的`uint`可能是32位,也可能是64位,取决于`Is64`.

另外`packed`(`packed_endian_specific_integral`)的定义如下:

```c++
template <typename ValueType, endianness Endian, std::size_t Alignment,
          std::size_t ALIGN = PickAlignment<ValueType, Alignment>::value>
struct packed_endian_specific_integral {
  using value_type = ValueType;
  static constexpr endianness endian = Endian;
  static constexpr std::size_t alignment = Alignment;

  packed_endian_specific_integral() = default;

  explicit packed_endian_specific_integral(value_type val) { *this = val; }

  operator value_type() const {
    return endian::read<value_type, endian, alignment>(
      (const void*)Value.buffer);
  }

  void operator=(value_type newValue) {
    endian::write<value_type, endian, alignment>(
      (void*)Value.buffer, newValue);
  }

  packed_endian_specific_integral &operator+=(value_type newValue) {
    *this = *this + newValue;
    return *this;
  }

  packed_endian_specific_integral &operator-=(value_type newValue) {
    *this = *this - newValue;
    return *this;
  }

  packed_endian_specific_integral &operator|=(value_type newValue) {
    *this = *this | newValue;
    return *this;
  }

  packed_endian_specific_integral &operator&=(value_type newValue) {
    *this = *this & newValue;
    return *this;
  }

private:
  struct {
    alignas(ALIGN) char buffer[sizeof(value_type)];
  } Value;

public:
  struct ref {
    explicit ref(void *Ptr) : Ptr(Ptr) {}

    operator value_type() const {
      return endian::read<value_type, endian, alignment>(Ptr);
    }

    void operator=(value_type NewValue) {
      endian::write<value_type, endian, alignment>(Ptr, NewValue);
    }

  private:
    void *Ptr;
  };
};
```

大体上是封装了一个特定类型的数值, 以及其相应的运算操作. 特别地, 数据的底层存储用到了`alignas`进行对齐(这里`ALIGN`默认为1, 不会特殊对齐). 另外, 数据的`read`和`write`操作都考虑到了大端序和小端序, 我们只要按照这个结构进行数据操作就能自动兼容32/64以及大小端序了.

接下来我们来分析如何获取初始的`ELFT`.

再来看`createObjFile`这个函数:

```c++
ELFFileBase *elf::createObjFile(MemoryBufferRef mb, StringRef archiveName,
                                bool lazy) {
  ELFFileBase *f;
  switch (getELFKind(mb, archiveName)) {
  case ELF32LEKind:
    f = make<ObjFile<ELF32LE>>(ELF32LEKind, mb, archiveName);
    break;
  case ELF32BEKind:
    f = make<ObjFile<ELF32BE>>(ELF32BEKind, mb, archiveName);
    break;
  case ELF64LEKind:
    f = make<ObjFile<ELF64LE>>(ELF64LEKind, mb, archiveName);
    break;
  case ELF64BEKind:
    f = make<ObjFile<ELF64BE>>(ELF64BEKind, mb, archiveName);
    break;
  default:
    llvm_unreachable("getELFKind");
  }
  f->init();
  f->lazy = lazy;
  return f;
}
```

可以看到判断的核心是`getELFKind`这个函数.

参数在这里:

```c++
// Opens a file and create a file object. Path has to be resolved already.
void LinkerDriver::addFile(StringRef path, bool withLOption) {
  using namespace sys::fs;

  std::optional<MemoryBufferRef> buffer = readFile(path);
  if (!buffer)
    return;
  MemoryBufferRef mbref = *buffer;
  ...
  case file_magic::elf_relocatable:
    files.push_back(createObjFile(mbref, "", inLib));
    break;
  default:
    error(path + ": unknown file type");
  }
}
```

第一个参数就是文件指针, 第二个参数为空.

接下来我们分析`getELFKind`内部:

```c++
static ELFKind getELFKind(MemoryBufferRef mb, StringRef archiveName) {
  unsigned char size;
  unsigned char endian;
  std::tie(size, endian) = getElfArchType(mb.getBuffer());

  auto report = [&](StringRef msg) {
    StringRef filename = mb.getBufferIdentifier();
    if (archiveName.empty())
      fatal(filename + ": " + msg);
    else
      fatal(archiveName + "(" + filename + "): " + msg);
  };

  if (!mb.getBuffer().startswith(ElfMagic))
    report("not an ELF file");
  if (endian != ELFDATA2LSB && endian != ELFDATA2MSB)
    report("corrupted ELF file: invalid data encoding");
  if (size != ELFCLASS32 && size != ELFCLASS64)
    report("corrupted ELF file: invalid file class");

  size_t bufSize = mb.getBuffer().size();
  if ((size == ELFCLASS32 && bufSize < sizeof(Elf32_Ehdr)) ||
      (size == ELFCLASS64 && bufSize < sizeof(Elf64_Ehdr)))
    report("corrupted ELF file: file is too short");

  if (size == ELFCLASS32)
    return (endian == ELFDATA2LSB) ? ELF32LEKind : ELF32BEKind;
  return (endian == ELFDATA2LSB) ? ELF64LEKind : ELF64BEKind;
}
```

这里主要是用`getElfArchType`读取ELF头部的magic获取位数,大小端序等信息,然后做点检查. 我们可以直接用这个函数进行判断.

# 参考lld实现简易readelf

```c++
class FuncV {
private:
  enum ELFKind : uint8_t {
    ELFNoneKind,
    ELF32LEKind,
    ELF32BEKind,
    ELF64LEKind,
    ELF64BEKind
  };

  static bool startsWith(const char* str, const char* prefix) {
    size_t prefixLen = std::strlen(prefix);
    return std::strncmp(str, prefix, prefixLen) == 0;
  }

  static inline std::pair<unsigned char, unsigned char>
  getElfArchType(const utils::MMapFile &mMapFile) {
    if (mMapFile.getFileSize() < llvm::ELF::EI_NIDENT) {
      return std::make_pair((uint8_t)llvm::ELF::ELFCLASSNONE,
                            (uint8_t)llvm::ELF::ELFDATANONE);
    }
    const char *object = mMapFile.readBytes(0);
    return std::make_pair((uint8_t)object[llvm::ELF::EI_CLASS],
                          (uint8_t)object[llvm::ELF::EI_DATA]);
  }

  static ELFKind getELFKind(const utils::MMapFile &mMapFile) {
    unsigned char size;
    unsigned char endian;
    std::tie(size, endian) = getElfArchType(mMapFile);

    const char *object = mMapFile.readBytes(0);
    if (!startsWith(object, llvm::ELF::ElfMagic)) {
      utils::Logger::fatal(mMapFile.getFilePath() + " is not an ELF file");
    }
    if (endian != llvm::ELF::ELFDATA2LSB && endian != llvm::ELF::ELFDATA2MSB) {
      utils::Logger::fatal(mMapFile.getFilePath() + " is a corrupted ELF file: "
                                                    "invalid data encoding");
    }
    if (size != llvm::ELF::ELFCLASS32 && size != llvm::ELF::ELFCLASS64) {
      utils::Logger::fatal(mMapFile.getFilePath() + " is a corrupted ELF file: "
                                                    "invalid file class");
    }

    size_t fileSize = mMapFile.getFileSize();
    if ((size == llvm::ELF::ELFCLASS32 &&
         fileSize < sizeof(llvm::ELF::Elf32_Ehdr)) ||
        (size == llvm::ELF::ELFCLASS64 &&
         fileSize < sizeof(llvm::ELF::Elf64_Ehdr))) {
      utils::Logger::fatal(mMapFile.getFilePath() + " is a corrupted ELF file: "
                                                    "file is too short");
    }

    if (size == llvm::ELF::ELFCLASS32)
      return (endian == llvm::ELF::ELFDATA2LSB) ? ELF32LEKind : ELF32BEKind;
    return (endian == llvm::ELF::ELFDATA2LSB) ? ELF64LEKind : ELF64BEKind;
  }

  template <class ELFT>
  static void runT(utils::MMapFile &oldMMapFile,
                   utils::MMapFile &newMMapFile,
                   const std::string &outputPath) {
    ObjFile<ELFT> oldObjFile(oldMMapFile);
    ObjFile<ELFT> newObjFile(newMMapFile);

    oldObjFile.parse();
    newObjFile.parse();

    std::stringstream oss;
    newObjFile.dump(oss);
    llvm::errs() << oss.str() << "\n";
  }

public:

  static void run(const std::string &oldObjPath, const std::string &newObjPath,
           ￼
// Opens a file and create a file object. Path has to be resolved already.
void LinkerDriver::addFile(StringRef path, bool withLOption) {
  using namespace sys::fs;
​
  std::optional<MemoryBufferRef> buffer = readFile(path);
  if (!buffer)
    return;
  MemoryBufferRef mbref = *buffer;
  ...
这里我们直接使用readFile即可,里面有高效的mmap机制,可以自动根据页大小进行分块.       const std::string &outputPath) {
    if (oldObjPath == newObjPath) {
      utils::Logger::fatal("the path of old ELF file is the same as "
                           "the path of new ELF file");
    }
    if (oldObjPath == outputPath) {
      utils::Logger::fatal("the output path is the same as "
                           "the path of old ELF file");
    }
    if (newObjPath == outputPath) {
      utils::Logger::fatal("the output path is the same as "
                           "the path of new ELF file");
    }

    utils::MMapFile oldMMapFile(oldObjPath);
    utils::MMapFile newMMapFile(newObjPath);

    ELFKind oldELFKind = getELFKind(oldMMapFile);
    ELFKind newELFKind = getELFKind(newMMapFile);
    if (oldELFKind != newELFKind) {
      utils::Logger::fatal("the format of the old ELF file does not match "
                           "the format of the new ELF file.");
    }

    switch (newELFKind) {
    case ELF32LEKind:
      runT<llvm::object::ELF32LE>(oldMMapFile, newMMapFile, outputPath);
      break;
    case ELF32BEKind:
      runT<llvm::object::ELF32BE>(oldMMapFile, newMMapFile, outputPath);
      break;
    case ELF64LEKind:
      runT<llvm::object::ELF64LE>(oldMMapFile, newMMapFile, outputPath);
      break;
    case ELF64BEKind:
      runT<llvm::object::ELF64BE>(oldMMapFile, newMMapFile, outputPath);
      break;
    default:
      utils::Logger::fatal("invalid ELF kind.");
    }
  }
};
```

`MMapFile`中使用了mmap读取文件, 配置为`mMap = mmap(nullptr, fileSize, PROT_READ, MAP_PRIVATE, fd, 0);`, 可以确保内存中的更改不会影响文件.

接下来我们来解析ELF文件中的内容.

ELF头的解析很简单, 直接头指针强转即可

```c++
ehdr = reinterpret_cast<Elf_Ehdr *>(mMapFile.readBytes(0));
```

Sections的解析可以参考`ELFFile::sections()`:

```c++
template <class ELFT>
Expected<typename ELFT::ShdrRange> ELFFile<ELFT>::sections() const {
  const uintX_t SectionTableOffset = getHeader().e_shoff;
  if (SectionTableOffset == 0) {
    if (!FakeSections.empty())
      return ArrayRef(FakeSections.data(), FakeSections.size());
    return ArrayRef<Elf_Shdr>();
  }

  if (getHeader().e_shentsize != sizeof(Elf_Shdr))
    return createError("invalid e_shentsize in ELF header: " +
                       Twine(getHeader().e_shentsize));

  const uint64_t FileSize = Buf.size();
  if (SectionTableOffset + sizeof(Elf_Shdr) > FileSize ||
      SectionTableOffset + (uintX_t)sizeof(Elf_Shdr) < SectionTableOffset)
    return createError(
        "section header table goes past the end of the file: e_shoff = 0x" +
        Twine::utohexstr(SectionTableOffset));

  // Invalid address alignment of section headers
  if (SectionTableOffset & (alignof(Elf_Shdr) - 1))
    // TODO: this error is untested.
    return createError("invalid alignment of section headers");

  const Elf_Shdr *First =
      reinterpret_cast<const Elf_Shdr *>(base() + SectionTableOffset);

  uintX_t NumSections = getHeader().e_shnum;
  if (NumSections == 0)
    NumSections = First->sh_size;

  if (NumSections > UINT64_MAX / sizeof(Elf_Shdr))
    return createError("invalid number of sections specified in the NULL "
                       "section's sh_size field (" +
                       Twine(NumSections) + ")");

  const uint64_t SectionTableSize = NumSections * sizeof(Elf_Shdr);
  if (SectionTableOffset + SectionTableSize < SectionTableOffset)
    return createError(
        "invalid section header table offset (e_shoff = 0x" +
        Twine::utohexstr(SectionTableOffset) +
        ") or invalid number of sections specified in the first section "
        "header's sh_size field (0x" +
        Twine::utohexstr(NumSections) + ")");

  // Section table goes past end of file!
  if (SectionTableOffset + SectionTableSize > FileSize)
    return createError("section table goes past the end of file");
  return ArrayRef(First, NumSections);
}
```

可以看到无非就是算了一些偏移, 然后有很多检查. 这里我们假设用户提供的ELF是正确的, 就不进行检查了:

```c++
    uint64_t sectionTableOffset = ehdr->e_shoff;
    char *object = mMapFile.readBytes(0);
    sections.emplace_back("", false, object, sectionTableOffset);
    // Handling case where the number of sections exceeds 65536.
    uint64_t numSections = ehdr->e_shnum;
    if (numSections == 0) {
      numSections = sections[0].getShSize();
    }
    for (uint64_t i = 1; i < numSections; i++) {
      sections.emplace_back("", false, object, i * sizeof(Elf_Shdr) +
                                                   sectionTableOffset);
    }
```

完整代码如下:

```c++
// object.hpp

template <class ELFT>
class ObjFile {
private:
  using Elf_Ehdr = typename ELFT::Ehdr;
  using Elf_Shdr = typename ELFT::Shdr;
  using Elf_Sym = typename ELFT::Sym;

  utils::MMapFile &mMapFile;
  Elf_Ehdr *ehdr;
  std::vector<ISection<ELFT>> sections;

public:
  ObjFile(utils::MMapFile &mMapFile) : mMapFile(mMapFile) {}

  void parse() {
    // Read ELF header
    ehdr = reinterpret_cast<Elf_Ehdr *>(mMapFile.readBytes(0));
    // Read sections
    uint64_t sectionTableOffset = ehdr->e_shoff;
    char *object = mMapFile.readBytes(0);
    sections.emplace_back("", false, object, sectionTableOffset);
    // Handling case where the number of sections exceeds 65536.
    uint64_t numSections = ehdr->e_shnum;
    if (numSections == 0) {
      numSections = sections[0].getShSize();
    }
    for (uint64_t i = 1; i < numSections; i++) {
      sections.emplace_back("", false, object, i * sizeof(Elf_Shdr) +
                                                   sectionTableOffset);
    }
  }

  void dump(std::ostream &oss) {
    oss << "==================== Ehdr ====================" << std::endl;
    oss << "[Magic]";
    oss << std::hex;
    for (int i = 0; i < llvm::ELF::EI_NIDENT; i++) {
      oss << " " << (int)ehdr->e_ident[i];
    }
    oss << std::endl;
    oss << std::dec;
    oss << "[e_shoff] " << ehdr->e_shoff << " (bytes into file)" << std::endl;
    oss << "[e_ehsize] " << ehdr->e_ehsize << " (bytes)" << std::endl;
    oss << "[e_shentsize] " << ehdr->e_shentsize << " (bytes)" << std::endl;
    oss << "[e_shnum] " << ehdr->e_shnum << std::endl;
    oss << "[e_shstrndx] " << ehdr->e_shstrndx << std::endl;

    oss << "==================== Shdr ====================" << std::endl;

    for (size_t i = 0; i < sections.size(); i++) {
      oss << "[" << i << "] ";
      sections[i].dump(oss);
      oss << "\n";
    }
  }
};

// section.hpp

template <class ELFT>
class ISection : public Reusable {
private:
  using Elf_Shdr = typename ELFT::Shdr;

  Elf_Shdr *shdr;
  char *data;

public:
  ISection(const std::string &id, bool named, char *object, uint64_t offset)
      : Reusable(id, ReusableType::Section, named) {
    shdr = reinterpret_cast<Elf_Shdr *>(object + offset);
    data = object + shdr->sh_offset;
  }

  uint64_t getShSize() const {
    return shdr->sh_size;
  }

  void dump(std::ostream &oss) {
    oss << shdr->sh_name << " | "
        << shdr->sh_type << " | "
        << shdr->sh_flags << " | " << std::hex
        << shdr->sh_addr << " | "
        << shdr->sh_offset << " | "
        << shdr->sh_size << " | " <<  std::dec
        << shdr->sh_link << " | "
        << shdr->sh_info << " | "
        << shdr->sh_addralign << " | "
        << shdr->sh_entsize;
  }
};
```

输出结果:

```shell
==================== Ehdr ====================
[Magic] 7f 45 4c 46 2 1 1 0 0 0 0 0 0 0 0 0
[e_shoff] 472 (bytes into file)
[e_ehsize] 64 (bytes)
[e_shentsize] 64 (bytes)
[e_shnum] 10
[e_shstrndx] 1
==================== Shdr ====================
[0] 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0
[1] 89 | 3 | 0 | 0 | 169 | 69 | 0 | 0 | 1 | 0
[2] 15 | 1 | 6 | 0 | 40 | 1a | 0 | 0 | 16 | 0
[3] 10 | 4 | 64 | 0 | 138 | 18 | 9 | 2 | 8 | 24
[4] 21 | 1 | 48 | 0 | 5a | 28 | 0 | 0 | 1 | 1
[5] 44 | 1 | 0 | 0 | 82 | 0 | 0 | 0 | 1 | 0
[6] 79 | 1879048193 | 2 | 0 | 88 | 38 | 0 | 0 | 8 | 0
[7] 74 | 4 | 64 | 0 | 150 | 18 | 9 | 6 | 8 | 24
[8] 60 | 1879002115 | 2147483648 | 0 | 168 | 1 | 9 | 0 | 1 | 0
[9] 97 | 2 | 0 | 0 | c0 | 78 | 1 | 3 | 8 | 24
```

需要注意的是, `ELFT`下面的类型和`Elf_Shdr_Base`是不一样的, 直接转换会编译报错, 我们可以用`uint64_t`中转一下. 另外, 上述代码中涉及到了`Reusable`这个类, 现在已经不用这个设计了, 忽略即可.

目前, 我们只打印出了`Shdr`的原始数据, 并没有做细粒度的解析. 不过目前我们已经了解了怎么阅读lld源码, 有需要参考源码扩展即可.

