gnu ld的链接脚本可以用`x86_64-linux-gnu-ld --verbose`查看:

```shell
GNU ld (GNU Binutils for Deepin) 2.41
  支持的仿真：
   elf_x86_64
   elf32_x86_64
   elf_i386
   elf_iamcu
   i386pep
   i386pe
使用内部链接脚本：
==================================================
/* Script for -z combreloc -z separate-code */
/* Copyright (C) 2014-2023 Free Software Foundation, Inc.
   Copying and distribution of this script, with or without modification,
   are permitted in any medium without royalty provided the copyright
   notice and this notice are preserved.  */
OUTPUT_FORMAT("elf64-x86-64", "elf64-x86-64",
              "elf64-x86-64")
OUTPUT_ARCH(i386:x86-64)
ENTRY(_start)
SEARCH_DIR("=/usr/local/lib/x86_64-linux-gnu"); SEARCH_DIR("=/lib/x86_64-linux-gnu"); SEARCH_DIR("=/usr/lib/x86_64-linux-gnu"); SEARCH_DIR("=/usr/lib/x86_64-linux-gnu64"); SEARCH_DIR("=/usr/local/lib64"); SEARCH_DIR("=/lib64"); SEARCH_DIR("=/usr/lib64"); SEARCH_DIR("=/usr/local/lib"); SEARCH_DIR("=/lib"); SEARCH_DIR("=/usr/lib"); SEARCH_DIR("=/usr/x86_64-linux-gnu/lib64"); SEARCH_DIR("=/usr/x86_64-linux-gnu/lib");
SECTIONS
{
  PROVIDE (__executable_start = SEGMENT_START("text-segment", 0x400000)); . = SEGMENT_START("text-segment", 0x400000) + SIZEOF_HEADERS;
  .interp         : { *(.interp) }
  .note.gnu.build-id  : { *(.note.gnu.build-id) }
  ...
```

lld没有默认的链接脚本, 而是通过一个函数指定默认布局:

https://groups.google.com/g/llvm-dev/c/3y15MZRgVZ4

关于链接脚本的教程: 

https://www.cnblogs.com/jianhua1992/p/16852784.html

#  Linker Scripts

每个链接都由一个链接脚本控制。该脚本使用链接器命令语言编写。

链接脚本的主要目的是描述如何将输入文件中的各个部分映射到输出文件中，并控制输出文件的内存布局。大多数链接脚本仅此而已。但是，必要时，链接器脚本也可以使用下面描述的命令来指导链接器执行更多的其它操作。

链接器通常使用一个链接脚本。如果没有为其提供，链接器将会使用默认的编译在链接器执行文件内部的脚本。可以使用命令 ’*– **verbose*** ’ 显示默认的链接脚本。某些命令行选项，例如 ’*-r* ’，’*-N* ’ 会影响默认的链接脚本。

你可以通过在命令行使用 ’*-**T*** ’ 命令使用自己的脚本。如果使用此命令，你的链接脚本将会替代默认链接脚本。

也可以通过将脚本作为链接器输入文件隐式的使用链接脚本，参考[Implicit Linker Scripts](https://sourceware.org/binutils/docs/ld/Implicit-Linker-Scripts.html#Implicit-Linker-Scripts)。

- **Basic Script Concepts**: 链接器脚本的基本概念
- **Script Format**: 链接器脚本的格式
- **Simple Example**: 简单的链接器脚本例子
- **Simple Commands**: 简单的链接器脚本命令
- **Assignments**: 为符号指定数值
- **SECTIONS**: 段命令
- **MEMORY**: 内存命令
- **PHDRS**: PHDRS命令
- **VERSION**: 版本命令
- **Expressions**: 链接脚本的表达式
- **Implicit Linker Scripts**: 隐式链接脚本

## 3.1 Basic Linker Script Concepts

为了描述链接脚本语言，我们需要定义一些基本概念和词汇。

链接器将输入文件(一个或多个)合并为一个输出文件。输出文件和每个输入文件都采用一种特殊的数据格式，称为目标文件格式。每个文件称为目标文件。输出文件通常称为可执行文件，但出于我们的目的，我们也将其称为目标文件。每个目标文件都有一个段（section）列表。有时把输入文件的段称作输入段，类似的，输出文件的段称作输出段。

目标文件中的每个段都有名称和大小。大多数段还具有关联的数据块，称为段内容。一个段可能被标记为可加载(*loadable*)，这意味着在运行输出文件时，段内容需要先加载到内存中。一个没有内容的段是可分配的，这意味着应该在内存中预留一个区域，但是这里不需要加载任何东西（在某些情况下，该内存必须清零）。既不可装载也不可分配的部分通常包含某种调试信息。

每个可加载或可分配的输出段都有两个地址。第一个是 **VMA** 或称为 **虚拟内存地址** 。这是运行输出文件时该段将具有的地址。第二个是 **LMA** ，即 **加载内存地址** 。这是段将会被加载的地址。在大多数情况下，这两个地址是相同的。当然它们也可能不同，一个示例是将数据段加载到ROM中，然后在程序启动时将其复制到RAM中（此技术通常用于初始化基于ROM的系统中的全局变量）。在这种情况下，ROM地址将是LMA，而RAM地址将是VMA。

您可以将 ***objdump\*程序与 ’ -\*h\* '选项一起使用**，以查看目标文件中的各个部分。

每个目标文件还具有一个符号列表，称为符号表。符号可以是定义的也可以是未定义的。每个符号都有一个名称，每个定义的符号都有一个地址，以及其他信息。如果将C或C ++程序编译到目标文件中，则将会将所有定义过的函数和全局变量以及静态变量作为已定义符号。输入文件中引用的每个未定义函数或全局变量都将成为未定义符号。

您可以使用 ***nm\* 程序或带有 ‘-\*t\*’ 选项的 \*objdump\*** 程序在目标文件中查看符号。

## 3.2 Linker Script Format

链接脚本是文本文件。

一个链接器脚本是一系列的命令。每个命令都是一个关键字，可能后面还跟有一个参数，或者一个符号的赋值。使用分号分割命令，空格通常被忽略。

类似于文件名或者格式名的字串可以直接输入。如果文件名含有一个字符例如逗号（逗号被用来分割文件名），你可以将文件名放在双引号内部。 **但是禁止在文件名内使用双引号字符** 。

你可以像C语言一样在链接脚本内包含注释，由’/*’和’*/’划分。和C一样，注释在句法上被当作空格。

## 3.3 Simple Linker Script Example

大多数的链接脚本非常简单。

最简单的链接脚本只有一个命令：’*SECTIONS* ’ 。 您可以使用 ’*SECTIONS* ’ 命令来描述输出文件的内存布局。

’*SECTIONS* ’ 命令功能非常强大。 在这里，我们将描述它的一个简单用法。 假设您的程序仅包含代码，初始化数据和未初始化数据。 它们分别位于“ *.text* ”，“.*data* ”和“ *.bss* ”段中。 我们进一步假设这些是唯一将会出现在输入文件中的段。

在此示例中，假设代码应在地址 *0x10000* 处加载，数据应从地址 *0x8000000* 开始。下面的链接脚本将会执行如下操作：

```c
SECTIONS
{
  . = 0x10000;
  .text : { *(.text) }
  . = 0x8000000;
  .data : { *(.data) }
  .bss : { *(.bss) }
}
```

您将 ’*SECTIONS* ’ 命令作为关键字 ’*SECTIONS* ’ 编写，然后在花括号中包含一系列符号的赋值和输出段的描述。

上例中 ’*SECTIONS* ’ 命令中的第一行设置特殊符号 “*.* ” 的值，即位置计数器。如果未通过其他方式指定输出段的地址（稍后将介绍其他方式），地址就会被设置为位置计数器的当前值。然后将位置计数器增加输出段的大小。在‘*SECTIONS* ’命令的开头，位置计数器的值为 ‘ *0* ’ 。

第二行定义了一个输出段“ *.text* ”。 **冒号是必需的语法** ，现在可以忽略它。在输出段名称后面的花括号中，列出应放置在此输出段中的输入段的名称。 “ *” 是与任何文件名匹配的通配符。表达式 ‘ **(.text)\* ’ 表示所有输入文件中的所有 ‘*.text*’ 输入段。

由于在定义输出段 ‘*.text*’ 时位置计数器为‘*0x10000* ’，因此链接程序会将输出文件中 ‘*.text*’ 段的地址设置为‘*0x10000* ’。

剩下的行定义了定义输出文件中的‘*.data* ’ 和‘*.bss* ’ 段。链接器会将‘*.data* ’ 输出段放置在地址’*0x8000000* ’处。在链接器放置‘*.data* ’ 段后，位置计数器为’*0x8000000* ’加上‘*.data* ’ 段的大小。因此‘*.bss* ’ 输出段在内存中将会紧紧挨在‘*.data* ’段后面。

链接器将通过增加位置计数器（如有必要）来确保每个输出部分具有所需的对齐方式。在此示例中， ‘*.text*’ 和‘*.data* ’ 段的指定地址可以满足任何对齐方式约束，但链接器可能必须在‘*.data* ’ 和‘*.bss* ’ 段之间创建一个小的间隙。

如上，这就是一个简单完整的链接脚本。

## 3.4 Simple Linker Script Commands

在本节中，我们将介绍一些简单的链接脚本命令。

- **Entry Point** : 设置入口点
- **File Commands** : 处理文件的命令
- **Format Commands** : 处理目标文件格式的命令
- **REGION_ALIAS** : 为内存区域分配别名
- **Miscellaneous Commands** : 其它链接脚本命令

### 3.4.1 Setting the Entry Point

在程序中执行的第一条指令称为入口点。 您可以使用 ***ENTRY*** 链接器脚本命令来设置入口点。 参数是符号名称：

```c
ENTRY(symbol)
```

有几种设置入口点的方法。 链接器将通过依次尝试以下每种方法来设置入口点，并在其中一种成功后停止：

- ‘*-e* ’输入命令行选项；
- 链接描脚本中的 *ENTRY(symbol)* 命令；
- 目标专用符号值（如果已定义）； 对于许多目标来说是 *start* 符号，但是例如基于PE和BeOS的系统检查可能的输入符号列表，并与找到的第一个符号匹配。
- ‘*.text* ’ 部段的第一个字节的地址（如果存在）；
- 地址0。

### 3.4.2 Commands Dealing with Files

以下是链接器脚本处理文件的几个常用命令:
**（1）\*INCLUDE filename\***
在命令处包含链接脚本文件 *filename* ，将在当前目录以及 *-L* 选项指定的任何目录中搜索文件。***INCLUDE*** 调用嵌套最多10个级别。

可以直接把 ***INCLUDE*** 放到顶层、 ***MEMORY*** 或者 ***SECTIONS*** 命令中，或者在输出段的描述中。

**（2）\*INPUT(file, file, …) / INPUT(file file …)\***
*INPUT* 命令指示链接程序在链接中包含指定的文件，就好像它们是在命令行上命名的一样。

例如，如果您始终希望在每次执行链接时都包含 *subr.o*，但又不想将其放在每个链接命令行中，则可以在链接脚本中放置 ‘*INPUT (subr.o)* ’。

实际上，您可以在链接描述文件中列出所有输入文件，然后仅用‘*-T* ’选项调用链接脚本。

如果配置了*sysroot* 前缀，且文件名以‘*/* ’符开头，并且正在处理的脚本位于*sysroot* 前缀内，则将在*sysroot* 前缀中查找文件名。也可以通过指定 *=* 作为文件名路径中的第一个字符，或在文件名路径前加上 *$ SYSROOT* 来强制使用*sysroot* 前缀。另请参阅命令行选项中对‘*-L* ’ 的描述([Command-line Options](https://sourceware.org/binutils/docs/ld/Options.html#Options))。

如果未使用 *sysroot* 前缀，则链接器将尝试打开包含链接器脚本的目录中的文件。如果没有找到，链接器将搜索当前目录。如果仍未找到，链接器将搜索库的搜索路径。

如果您使用 ‘*INPUT (-lfile)* ’ ，则 *ld* 会将名称转换为 *libfile.a*，就像命令行参数‘*-l* ’一样。

当您在隐式链接脚本中使用 *INPUT* 命令时，文件在链接脚本文件被包含的时刻才会被加入。这可能会影响库的搜索。

**（3）\*GROUP(file, file, …) / GROUP(file file …)\***
*GROUP* 命令类似于 *INPUT*，不同之处在于，所有file指出的名字都应该为库，并且所有库将会被重复搜索直到没有新的未定义引用被创建。 请参阅命令行选项中 ‘-(’ 的说明([Command-line Options](https://sourceware.org/binutils/docs/ld/Options.html#Options))。

**（4）\*AS_NEEDED(file, file, …) / AS_NEEDED(file file …)\***
此构造只能出现在 *INPUT* 或 *GROUP* 命令以及其他文件名中。命令中的文件将会以类似于直接出现在 *INPUT* 或 *GROUP* 命令中的文件一样处理，除了ELF共享库，ELF共享库仅在真正需要使用时才被添加。这个构造实质上为其中列出的所有文件启用了 -as-needed 选项，为了恢复以前编译环境，之后需设置 --no-as-needed。

**（5）\*OUTPUT(filename)\***
*OUTPUT* 命令为输出文件命名。 在链接脚本中使用 *OUTPUT（filename）\*与在命令行中使用 ‘\*-o filename*’ 一样（请参阅[Command-line Options](https://sourceware.org/binutils/docs/ld/Options.html#Options))。 如果两者都使用，则命令行选项优先。

您可以使用 *OUTPUT* 命令为输出文件定义默认名称，以此替代默认名称a.out。

**（6）\*SEARCH_DIR(path)\***
*SEARCH_DIR* 命令添加一个 *ld* 搜索库的**路径**。使用 *SEARCH_DIR(path)* 与在命令行上使用 *’ -L path* ’ 完全一样(参见[Command-line Options](https://sourceware.org/binutils/docs/ld/Options.html#Options))。如果同时使用了这两条路径，那么链接器将会搜索所有路径。首先搜索使用命令行选项指定的路径。

**（7）\*STARTUP(filename)\***
*STARTUP* 命令与 *INPUT* 命令类似，除了filename将成为要链接的第一个输入文件，就像它是在命令行中首先指定的一样。在一些把第一个文件当作入口点的系统上这个命令非常有效。

### 3.4.3 Commands Dealing with Object File Formats

有两个链接器脚本命令可以用来处理对象文件格式：

```c
OUTPUT_FORMAT(bfdname)
OUTPUT_FORMAT(default, big, little)
```

*OUTPUT_FORMAT* 命令使用BFD格式的命名方式（请参见[BFD](https://sourceware.org/binutils/docs/ld/BFD.html#BFD)）。使用 *OUTPUT_FORMAT（bfdname）* 与在命令行上使用 ‘*–oformat bfdname* ’ 完全相同（请参见[Command-line Options](https://sourceware.org/binutils/docs/ld/Options.html#Options)）。如果两者都使用，则命令行选项优先。

您可以将OUTPUT_格式与三个参数一起使用，以根据 ’ *-EB* ’ 和 ‘*-EL*’ 命令行选项使用不同的格式。这允许链接器脚本根据所需的endianness设置输出格式。

如果未使用 ’ *-EB* ’ 和 ‘*-EL*’ '，那么输出格式将会使用第一个参数作为默认值。如果使用 ’ *-EB* '，输出格式将是第二个参数 **big**。如果使用 ‘*-EL*’ '，输出格式将是第三个参数，**little**。

例如，MIPS ELF目标的默认链接器脚本使用以下命令：

```c
OUTPUT_FORMAT(elf32-bigmips, elf32-bigmips, elf32-littlemips)
```

这说明输出文件的默认格式是 ‘*elf32-bigmips*’，但如果用户使用’*-EL*’ '命令行选项，则将以‘*elf32-littlemips*’格式创建输出文件。

```c
TARGET(bfdname)
```

*TARGET* 命令设置读取输入文件时的BFD格式。这将影响后面的 *INPUT* 和 *GROUP* 命令。此命令类似使用命令行指令 ‘*-b bfdname*’ （参见[Command-line Options](https://sourceware.org/binutils/docs/ld/Options.html#Options)）。如果使用了*TARGET*命令，但*OUTPUT_FORMAT*命令没使用，则最后的*TARGET*命令还被用来设置输出文件的格式（参见[BFD](https://sourceware.org/binutils/docs/ld/BFD.html#BFD)）。

### 3.4.4 Assign alias names to memory regions

可以为*MEMORY*命令创建的内存区域提供别名。 每个名称最多对应一个存储区域

```c
REGION_ALIAS(alias, region)
```

*REGION_ALIAS* 函数为 内存区域创建别名 。这允许灵活地将输出部分映射到内存指定区域。下面有一个例子。

假设我们有一个用于嵌入式系统的应用程序，它带有各种内存存储设备。它们都有一个通用的，易失性内存RAM，允许代码执行或数据存储。一些可能有一个只读的、非易失性内存ROM，允许代码执行和只读数据访问。最后一个是只读、非易失性存储器ROM2，允许对只读数据段读取，不允许代码执行。现在有四个输出段：

- .text ：程序代码
- .rodata ：只读数据
- .data ：可读写且需要初始化数据
- .bss ：可读写的置零初始化数据
  目标是提供一个链接器脚本文件，该文件包含定义系统无关的输出段的部分，和将输出段映射到系统上可用内存区域的系统相关部分。我们的嵌入式系统有三种不同的内存设置A、B和C：

```c
Section	Variant A	Variant B	Variant C
.text	RAM	         ROM	    ROM
.rodata	RAM	         ROM	    ROM2
.data	RAM	         RAM/ROM	RAM/ROM2
.bss	RAM	         RAM	    RAM
```

RAM/ROM或RAM/ROM2表示将此段分别加载到区域ROM或ROM2中。请注意，三个设置的.data段的起始地址都位于.rodata段的末尾。

接下来是处理输出段的基本链接脚本。 它包含描述内存布局的系统相关链接 *cmds.memory* 文件：

```mipsasm
INCLUDE linkcmds.memory
SECTIONS
 
{
 
.text :
 
{
 
(.text)
 
} > REGION_TEXT
 
.rodata :
 
{
 
(.rodata)
 
rodata_end = .;
 
} > REGION_RODATA
 
.data : AT (rodata_end)
 
{
 
data_start = .;
 
(.data)
 
} > REGION_DATA
 
data_size = SIZEOF(.data);
 
data_load_start = LOADADDR(.data);
 
.bss :
 
{
 
(.bss)

} > REGION_BSS
 
}
 
```



现在我们需要三个不同的 *linkcmds.memory* 来定义内存区域以及别名。下面是A，B，C不同的 *linkcmds.memory* ：

**A ：所有都存入RAM**

```scss
MEMORY
  {
    RAM : ORIGIN = 0, LENGTH = 4M
  }
REGION_ALIAS("REGION_TEXT", RAM);
 
REGION_ALIAS("REGION_RODATA", RAM);
 
REGION_ALIAS("REGION_DATA", RAM);
 
REGION_ALIAS("REGION_BSS", RAM);
 
```



**B ：代码和只读数据存入ROM。可读写数据放入RAM。一个已初始化了的数据的镜像被加载到ROM，并在系统启动的时候读入RAM**

```scss
MEMORY
  {
    ROM : ORIGIN = 0, LENGTH = 3M
    RAM : ORIGIN = 0x10000000, LENGTH = 1M
  }
REGION_ALIAS("REGION_TEXT", ROM);
 
REGION_ALIAS("REGION_RODATA", ROM);
 
REGION_ALIAS("REGION_DATA", RAM);
 
REGION_ALIAS("REGION_BSS", RAM);
 
```



**C ：代码放入ROM，只读数据放入ROM2。可读写数据放入RAM。一个已初始化了的数据的镜像被加载到ROM2，并在系统启动的时候读入RAM**

```scss
MEMORY
  {
    ROM : ORIGIN = 0, LENGTH = 2M
    ROM2 : ORIGIN = 0x10000000, LENGTH = 1M
    RAM : ORIGIN = 0x20000000, LENGTH = 1M
  }
REGION_ALIAS("REGION_TEXT", ROM);
 
REGION_ALIAS("REGION_RODATA", ROM2);
 
REGION_ALIAS("REGION_DATA", RAM);
 
REGION_ALIAS("REGION_BSS", RAM);
 
```



如有必要，可以编写通用的系统初始化程序以将.data段从ROM或ROM2复制到RAM：

```cpp
#include <string.h>
 
 
extern char data_start [];
 
extern char data_size [];
 
extern char data_load_start [];
 
 
void copy_data(void)
 
{
 
if (data_start != data_load_start)
 
{
 
memcpy(data_start, data_load_start, (size_t) data_size);
 
}
 
}
 
```



### 3.4.5 Other Linker Script Commands

还有一些其他链接器脚本命令：

- ***ASSERT(exp, message)***
  注意此断言会在最终链接阶段之前进行检查。这表示，在段内使用PROVIDE的定义如果用户没有为其设置值，此表达式将无法通过检测。唯一的例外是PROVIDE的符号刚刚引用了’.’。因此，一个如下断言：
  确保 *exp* 不为零。 如果为零，则退出链接并显示错误代码，并打印一些相关的信息。

请注意，在链接的最后阶段发生之前会检查断言。 这意味着，如果用户没有为这些符号设置值，则涉及段定义中提供的符号的表达式将失败。 该规则的唯一例外是仅引用点的提供的符号。 因此，这样的断言：

```c
.stack :
  {
    PROVIDE (__stack = .);
    PROVIDE (__stack_size = 0x100);
    ASSERT ((__stack > (_end + __stack_size)), "Error: No room left for the stack");
  }
```

如果没有在其他地方定义stack_size，则会失败。在段外定义的符号会在此前被求值，可以在*ASSERTions* 使用它们，因此:

```c
PROVIDE (__stack_size = 0x100);
  .stack :
  {
    PROVIDE (__stack = .);
    ASSERT ((__stack > (_end + __stack_size)), "Error: No room left for the stack");
  }
```

将会工作。

- ***EXTERN(symbol symbol …)***
  强制将符号作为未定义符号输入到输出文件中。 这样做可能会例如触发标准库中其他模块的链接。 您可以为每个 *EXTERN* 列出几个符号，并且可以多次使用 *EXTERN*。 此命令与 ‘*-u* ’ 命令行选项具有相同的作用。
- ***FORCE_COMMON_ALLOCATION***
  这个命令与’ *-d* ’ 命令行选项具有相同的效果:即便是使用了’*-r*’ 的重定位输出文件，也让 *ld* 为普通符号分配空间。
- ***INHIBIT_COMMON_ALLOCATION***
  这个命令与命令行选项 *‘–no-define-common*’ 具有相同的效果 : 让 *ld* 不为普通符号分配空间，即便是一个非可重定位输出文件。
- ***FORCE_GROUP_ALLOCATION***
  这个命令与命令行选项 ‘*–force-group-allocation*’ 具有相同的效果 : 使*ld* place 段组成员像普通的输入段一样，并且即使指定了可重定位的输出文件(’ -r ')也可以删除段组。
- ***INSERT [ AFTER | BEFORE ] output_section***
  此命令通常在‘*-T* ’ 指定的脚本中使用，用来增强默认的**SECTIONS**。例如，重复占位程序段。它将把所有此前的链接脚本的声明插入*output_section*的后面（或者前面），并且使 *’-T* ’不要覆盖默认链接脚本。实际插入点类似于孤儿段。参见[Location Counter](https://sourceware.org/binutils/docs/ld/Location-Counter.html#Location-Counter)。插入发生在链接器把输入段映射到输出段后。在插入前，因为’*-T* ’的脚本在默认脚本之前被解析，在’*-T*’脚本中的声明会先于默认内部脚本的声明而执行。特别是，将对默认脚本中的’*-T* ’输出段进行输入段分配。下例为’*-T* ’脚本使用*INSERT*可能的情况：

```c
SECTIONS
{
  OVERLAY :
  {
    .ov1 { ov1*(.text) }
    .ov2 { ov2*(.text) }
  }
}
INSERT AFTER .text;
```

- ***NOCROSSREFS(section section …)***
  此命令可能被用来告诉 *ld*，如果引用了section的参数就报错。

在特定的程序类型中，比如使用覆盖技术的嵌入式系统，当一个段被加载到内存中，另一个段不会被加载。任何两个段之间直接的引用都会带来错误。例如，如果一个段中的代码调用另一个段中的函数，将会产生错误。

*NOCROSSREFS* 命令列出了一系列输出段的名字。如果 *ld* 检测到任何段间交叉引用，将会报告错误并返回非零退出码。注意*NOCROSSREFS*使用输出段名称，而不是输入段名称。

- ***NOCROSSREFS_TO(tosection fromsection …)***
  此命令可能被用来告诉 *ld*，从其他段列表中对某个段的任何引用就会引发错误。

当需要确保两个或多个输出段完全独立，但是在某些情况下需要单向依赖时，*NOCROSSREFS* 命令很有用。 例如，在多核应用程序中，可能存在可以从每个核调用的共享代码，但是出于安全考虑，绝不能回调。

*NOCROSSREFS_TO* 命令携带(给出)输出段名称的列表。 其他任何部分都不能引用第一部分。 如果 *ld* 从其他任何部分中检测到对第一部分的任何引用，它将报告错误并返回非零退出状态。 请注意，*NOCROSSREFS_TO* 命令使用输出段名称，而不是输入段名称。

- ***OUTPUT_ARCH(bfdarch)***
  指定一个特定的输出机器架构。该参数是BFD库使用的名称之一(请参阅[BFD](https://sourceware.org/binutils/docs/ld/BFD.html#BFD))。通过使用带有 ’ *-f* ’ 选项的objdump程序，您可以看到目标文件的体系结构。
- ***LD_FEATURE(string)***
  此命令可用于修改 *ld* 行为。如果字符串是“*SANE_EXPR*”，那么脚本中的绝对符号和数字将被在任何地方当作数字对待。请参考[ Expression Section](https://sourceware.org/binutils/docs/ld/Expression-Section.html#Expression-Section)。

## 3.5 Assigning Values to Symbols

可以给链接器脚本中的符号赋值。这会定义符号并将其放入具有全局作用域的符号表中。

- Simple Assignments 简单赋值
- HIDDEN 隐藏
- PROVIDE PROVIDE
- PROVIDE_HIDDEN PROVIDE_HIDDEN
- Source Code Reference 如何在源代码中使用一个链接脚本定义的符号

## 3.5.1 Simple Assignments

您可以使用任何C赋值操作符来赋值符号:

```c
symbol = expression ;
symbol += expression ;
symbol -= expression ;
symbol *= expression ;
symbol /= expression ;
symbol <<= expression ;
symbol >>= expression ;
symbol &= expression ;
symbol |= expression ;
```

第一种情况将表达式的值赋值给符号。 在其他情况下，必须先定义符号，并相应地调整符号的值。

特殊符号名称 ‘. ’ 表示位置计数器。 您只能在 *SECTIONS* 命令中使用它。 请参阅[ Location Counter](https://sourceware.org/binutils/docs/ld/Location-Counter.html#Location-Counter)。

表达式后面的分号不能省略。

表达式定义如下； 请参阅[Expressions](https://sourceware.org/binutils/docs/ld/Expressions.html#Expressions)。

你在写表达式赋值语句时，可以把它们作为单独的部分,也可以作为 ’*SECTIONS*’ 命令中的一个语句,或者作为 ’*SECTIONS*’ 命令中输出段描述的一个部分。

符号的有效作用区域由表达式所在的段决定，[Expression Section](https://sourceware.org/binutils/docs/ld/Expression-Section.html#Expression-Section)。

下面是是三个不同位置为符号赋值的示例：

```c
floating_point = 0;
SECTIONS
{
  .text :
    {
      *(.text)
      _etext = .;
    }
  _bdata = (. + 3) & ~ 3;
  .data : { *(.data) }
}
```

在本例中，符号 ‘*floating_point*’ 将被定义为零。符号 ’ *_etext* ’ 将被设置为紧随 ’.*text*’ 最后一个输入段后面的地址。符号’ _bdata '将被定义为在 ’.*text*’ 输出段后面的一个4字节向上对齐的地址。

### 3.5.2 HIDDEN

语法HIDDEN(symbol = expression)为ELF目标的端口定义一个符号，符号将被隐藏并且不会被导出。
下面是[Simple Assignments](https://sourceware.org/binutils/docs/ld/Simple-Assignments.html#Simple-Assignments)的例子，使用HIDDEN重写：

```c
HIDDEN(floating_point = 0);
SECTIONS
{
  .text :
    {
      *(.text)
      HIDDEN(_etext = .);
    }
  HIDDEN(_bdata = (. + 3) & ~ 3);
  .data : { *(.data) }
}
```

在本例中，这三个符号在此模块之外都不可见

### 3.5.3 PROVIDE

在某些情况下，仅当一个符号被引用了却没有定义在任何链接目标中，才需要为链接脚本定义一个符号。例如，传统链接器定义了符号‘*etext*’。然而，ANSI C要求用户能够使用’ *etext* '作为函数名而不会引发错误。PROVIDE关键字可以用来定义一个符号，比如‘*etext*’ ，只有当它被引用但没有被定义时才使用。语法是 *PROVIDE(symbol = expression)*。

下面是一个使用提供定义‘*etext*’的例子:

```c
SECTIONS
{
  .text :
    {
      *(.text)
      _etext = .;
      PROVIDE(etext = .);
    }
}
```

在本例中，如果程序定义了’ *_etext* ‘(带有前导下划线)，链接器将给出重复定义错误。另一方面，如果程序定义了’ *etext* ‘(没有前导下划线)，链接器会默认使用程序中的定义。如果程序引用了’ *etext* '但没有定义它，链接器将使用链接器脚本中的定义。

注意 -*PROVIDE*指令将考虑定义一个普通符号，即使这样的符号可以与PROVIDE将创建的符号组合在一起。当考虑构造函数和析构函数列表符号时，这一点尤其重要，因为它们通常被定义为普通符号。

### 3.5.4 PROVIDE_HIDDEN

与 *PROVIDE* 类似。对于ELF目标的端口，符号将被隐藏且不会被输出。

### 3.5.5 Source Code Reference

从源代码获取链接器脚本定义的变量并不直观。 特别是，特别是链接脚本中的符号与高级语言定义的变量声明不同的时候，将使用一个没有值的变量替代它。

在进一步讨论之前，必须注意，当编译器将源代码中的名称存储在符号表中时，它们通常会将它们转换为不同的名称。 例如，Fortran编译器通常在前面或后面加上下划线，而C ++则执行大量的 ‘*name mangling* ’。 因此，在源代码中使用的变量名称与在链接脚本中定义的相同变量的名称之间可能会有差异。 例如，在C语言中，链接脚本变量可能称为：

```c
extern int foo;
```

但是在链接器脚本中，它可能被定义为:

```c
  _foo = 1000;
```

然而，在其余的例子中，假设没有发生名称转换。

当一个符号用高级语言，比如C语言，声明了一个符号，会发生两件事。首先，编译器在程序内存中保留足够的空间来保存符号的值。第二种方法是编译器在程序的符号表中创建一个条目，用来保存符号的地址。例如下面的C声明：

```c
 int foo = 1000;
```

在符号表中创建一个名为’ *foo* '的条目。这个入口保存了一个‘*int*’ 大小的内存块的地址，数字1000最初存储在这里。

当程序引用一个符号时，编译器生成的代码首先访问符号表以查找该符号的内存块地址，然后代码从该内存块读取值。所以:

```c
 foo = 1;
```

在符号表中查找符号’ *foo* '，获取与该符号关联的地址，然后将值1写入该地址。而:

```c
int * a = & foo;
```

在符号表中查找符号’ *foo* '，获取它的地址，然后将这个地址复制到与变量 ’ *a* ’ 相关联的内存块中。

相比之下，链接器脚本符号声明在符号表中创建一个条目，但不给它们分配任何内存。因此，它们是一个没有值的地址。例如链接器脚本定义:

```c
  foo = 1000;
```

在符号表中创建一个名为’ *foo* '的条目，该条目保存内存位置1000的地址，但地址1000上没有存储任何特殊内容。这意味着您无法访问链接程序脚本定义的符号的值-它没有值。您所能做的就是访问链接器脚本定义符号的地址。

因此，当您在源代码中使用链接器脚本定义的符号时，**您应该始终获取该符号的地址，并且永远不要尝试使用它的值**。例如，假设你想把内存的 .ROM 拷贝到 .FLASH 中，链接器脚本包含了这些声明:

```c
  start_of_ROM   = .ROM;
  end_of_ROM     = .ROM + sizeof (.ROM);
  start_of_FLASH = .FLASH;
```

那么执行复制的C源代码为:

```cpp
  extern char start_of_ROM, end_of_ROM, start_of_FLASH;
memcpy (& start_of_FLASH, & start_of_ROM, & end_of_ROM - & start_of_ROM);
 
```



注意 ‘*&*’ 运算符的使用。上面是正确的代码。一种替换是，把符号被当作一个数组变量的名称，因此代码变成了：

```scss
  extern char start_of_ROM[], end_of_ROM[], start_of_FLASH[];
memcpy (start_of_FLASH, start_of_ROM, end_of_ROM - start_of_ROM);
 
```



注意此时不需要操作符 ’*&*’ 了。

## 3.6 SECTIONS Command

*SECTIONS* 命令告诉链接器如何将输入段映射到输出段，以及如何将输出段放在内存中。
*SECTIONS* 命令的格式为:

```c
SECTIONS
{
  sections-command
  sections-command
  …
}
```

每个 *sections-command* 命令可能是下面之一：

- *ENTRY* 命令(请参阅[Entry command](https://sourceware.org/binutils/docs/ld/Entry-Point.html#Entry-Point))
- 符号赋值(请参阅[Assignments](https://sourceware.org/binutils/docs/ld/Assignments.html#Assignments))
- 输出段的描述
- overlay描述

为了方便在这些命令中使用位置计数器，在*SECTIONS* 命令中允许使用 *ENTRY* 命令和符号赋值。 这也可以使链接描述文件更容易理解，因为你可以在更有意义的地方使用这些命令来控制输出文件的布局。

输出段描述和覆盖在下面将会分析。

如果在链接脚本中未使用 *SECTIONS* 命令，则链接器将会照输入文本的顺序，将每个输入部段放置到名称相同的输出段中。例如，如果所有输入段出现在第一个文件中，输出文件的段的顺序将会与第一个输入文件保持一致。第一个段被放在地址0。

- Output Section Description 输出段描述
- Output Section Name 输出段名称
- Output Section Address 输出段地址
- Input Section 输入段描述
- Output Section Data 输出段数据
- Output Section Keywords 输出段关键字
- Output Section Discarding 输出段忽略的内容
- Output Section Attributes 输出段属性
- Overlay Description Overlay description

### 3.6.1 Output Section Description

输出段的完整描述如下所示：

```c
section [address] [(type)] :
  [AT(lma)]
  [ALIGN(section_align) | ALIGN_WITH_INPUT]
  [SUBALIGN(subsection_align)]
  [constraint]
  {
    output-section-command
    output-section-command
    …
  } [>region] [AT>lma_region] [:phdr :phdr …] [=fillexp] [,]
```

大部分的可选段属性在多数输出段不需要使用。

*SECTION* 边上的**空格是必须的**，这样段名就没有歧义了。**冒号和花括号也是必需的**。如果使用了fillexp，并且下一个section -命令看起来像是表达式的延续，则可能需要在末尾使用逗号。换行符和其他空格是可选的。

当 fillexp 使用且接下来的 *sections-command* 看起来像是表达式的延续的时候，可能需要在后面加上逗号。

每个 *output-section-command* 可以是下列命令之一:
符号赋的值(参见[Assignments](https://sourceware.org/binutils/docs/ld/Assignments.html#Assignments))
输入段描述(参见[Input Section](https://sourceware.org/binutils/docs/ld/Input-Section.html#Input-Section))
直接包引用的数据值(参见[Output Section Data](https://sourceware.org/binutils/docs/ld/Output-Section-Data.html#Output-Section-Data))
特殊的输出段关键字(参见[Output Section Keywords)](https://sourceware.org/binutils/docs/ld/Output-Section-Keywords.html#Output-Section-Keywords))

### 3.6.2 Output Section Name

输出段的名字是 *section* 。*section*必须满足输出格式的规定。在只支持有限段数目的格式中，例如 *a.out* ，名称必须是该格式所支持的名称之一(例如*a.out* ，只允许’*.text*’，’.*data*’，’.*bss*’)。如果输出格式支持任意数量的段，但是只有数字而不是名称(*Oasys* 就是这种情况)，则名称应该以带引号的数字字符串的形式提供。一个段的名字可以由任意字符序列组成，但一个含有许多特殊字符(如逗号)的名称必须用引号括起来。

名称为 ‘/*DISCARD*/’ 的输出段 ，有特殊含义; 参考[Output Section Discarding](https://sourceware.org/binutils/docs/ld/Output-Section-Discarding.html#Output-Section-Discarding).

### 3.6.3 Output Section Address

*address* 是输出段VMA（虚拟内存地址）的表达式。此地址是可选参数，但如果提供了该地址，则输出地址就会被精确的设置为指定的值。

如果没有指定输出地址，那么则依照下面的几种方式尝试选择一个地址。此地址将被调整以适应输出段的对齐要求。输出段的对齐要求是所有输入节中含有的对齐要求中最严格的一个。

输出段地址探索如下：

- 如果为该段设置了一个输出内存区域，那么它将被添加到该区域中，其地址将是该区域中的下一个空闲地址。
- 如果使用 *MEMORY* 命令创建内存区域列表，那么将选择具有与该段兼容属性的第一个区域来包含该区域。该部分的输出地址将是该区域中的下一个空闲地址；*MEMORY* 。
- 如果没有指定内存区域，或者没有与段匹配的内存区域，则输出地址将基于位置计数器的当前值。

例如:

```c
.text . : { *(.text) }
```

和

```c
.text : { *(.text) }
```

有着细微的不同。 第一个将‘.*text*’ 输出段的地址设置为位置计数器的当前值。 第二个参数会将其设置为位置计数器的当前值，但是该值与所有‘.*text*’ 输入段中最严格的对齐方式对齐。

*address* 可以是任意表达式； 例如，如果要在0x10字节(16字节)边界上对齐段，以使节地址的最低四位为零，则可以执行以下操作：

```c
.text ALIGN(0x10) : { *(.text) }
```

之所以这样做，是因为 *ALIGN* 返回的当前位置计数器向上对齐到指定的值。
为段指定地址将会改变位置计数器的值，前提是该段是非空的(空的段被忽略)。

### 3.6.4 Input Section Description

最常见的输出段命令（*output-section-command*）是输入段描述。

输入段描述是链接脚本最基本的操作。 您可以使用输出段来告诉链接器如何在内存中布置程序。 您可以使用输入段描述来告诉链接器如何将输入文件映射到您的内存布局中。

- Input Section Basics 基本的输入段
- Input Section Wildcards 输入段通配符模板
- Input Section Common 普通符号的输入段
- Input Section Keep 输入段与垃圾回收
- Input Section Example 输入段例子

#### 3.6.4.1 Input Section Basics

输入段说明由一个文件名和一个括号中的段名列表（可选）组成。

文件名和段名可以是通配符，我们将在下面进一步描述（请参阅[Input Section Wildcards](https://sourceware.org/binutils/docs/ld/Input-Section-Wildcards.html#Input-Section-Wildcards)）。

最常见的输入段描述是在输出段中包括所有具有特定名称的输入段。 例如，把所有输入段放入’.*text*’段，可以这么写：

```c
*(.text)
```

**这里的 ‘\*’ 是一个通配符**，它可以用来匹配任何文件名。要排除与文件名通配符匹配的文件列表，可以使用 *EXCLUDE_FILE* 来匹配除 *EXCLUDE_FILE*列表中指定的文件以外的所有文件。例如：

```c
EXCLUDE_FILE (*crtend.o *otherfile.o) *(.ctors)
```

将导致包括除 *crtend.o* 和 *otherfile.o* 以外的所有文件的所有 *.ctors* 段。EXCLUDE_FILE 也可以放在段的列表中，例如：

```c
*(EXCLUDE_FILE (*crtend.o *otherfile.o) .ctors)
```

其结果与前面的示例相同。如果段列表包含多个段，则支持 EXCLUDE_FILE 的两个语法非常有用，如下所述。

有两种方法可以包含多个段：

```c
*(.text .rdata)
*(.text) *(.rdata)
```

两种方法的区别是输入段的 ’.*text*’ 和 ’.*rata*’ 段出现在输出段中的顺序。第一个例子里，他们将被混合在一起，按照链接器找到它们的顺序存放。另一个例子中，所有 ’.*text*’ 输入段将会先出现，后面是 ’.*rdata*’ 输入段。

将EXCLUDE_FILE与多个段一起使用时，这个排除命令仅仅对紧随其后的段有效，例如：

```c
*(EXCLUDE_FILE (*somefile.o) .text .rdata)
```

将导致包含除 *somefile.o* 以外的所有文件的所有‘.*text*’段，而包括somefile.o在内的所有文件的所有‘.*rdata*’ 段都将被包含。要从somefile.o中排除‘.*rdata*’ 段部分，可以将示例修改为：

```c
*(EXCLUDE_FILE (*somefile.o) .text EXCLUDE_FILE (*somefile.o) .rdata)
```

或者，将EXCLUDE_FILE放在段列表之外(在选择输入文件之前)，将导致排除操作对所有段有效。因此，前一示例可以重写为：

```c
EXCLUDE_FILE (*somefile.o) *(.text .rdata)
```

你可以指定一个文件名来包含特定文件的段。如果一个或者多个你的文件需要被放在内存中的特定位置，你可能需要这么做。例如：

```c
data.o(.data)
```

如果想使用段标志来选择输入文件的段，可以使用INPUT_SECTION_FLAGS。

下面是一个为ELF段使用段头标志的简单示例:

```c
SECTIONS {
  .text : { INPUT_SECTION_FLAGS (SHF_MERGE & SHF_STRINGS) *(.text) }
  .text2 :  { INPUT_SECTION_FLAGS (!SHF_WRITE) *(.text) }
}
```

在本例中，输出段 ‘.*text*’ 将被由那些与 *(.text) 能匹配的段（名字）且段头部标志设置了SHF_MERGE和SHF_STRINGS的段构成。输出段 ‘.*text2*’ 由那些与 *(.text) 能匹配的段（名字）且段头部标志未设置SHF_WRITE的段构成。

你也可以指出特别的关联库名称的文件，命令是[ 库匹配模板:与文件匹配的模式 ]，冒号两边不能有空格。

- ‘*archive:file*’ 在库中寻找能够匹配的文件
- ‘*archive:*’ 匹配整个库
- ‘*:file*’ 匹配文件但不匹配库
- ‘*archive*’ 和 ‘*file*’ 中的一个或两个都可以包含shell通配符。在基于DOS的文件系统上，链接器会假定一个单字跟着一个冒号是一个特殊的驱动符，因此 ‘*c:myfile.o*’ 是一个文件的特殊使用，而不是关联库’c’的 ’*myfile.o*’ 文件。‘*archive:file*’：可以使用在EXCLUDE_FILE列表中，但不能出现在其他链接脚本内部。例如，你不能使用 ‘*archive:file*’从 INPUT命令中取出一个库相关的文件。

如果你使用一个文件名而不指出段列表，则所有的输入文件的段将被放入输出段。通常不会这么做，但有些场合比较有用，例如：

```c
data.o
```

当你使用一个文件名且不是 ‘*archive:file*’特殊命令，并且不含任何通配符，链接器将先查看你是否在命令行上或者在INPUT命令里指定了该文件。如果没有这么做，链接器尝试将文件当作输入文件打开，就像文件出现在了命令行一样。注意与INPUT命令有区别，因为链接器不会在库文件路径搜索文件。

#### 3.6.4.2 Input Section Wildcard Patterns

在输入段描述中，文件名和段名都可以使用通配符模式。
在许多示例中看到的文件名’ * '是一个简单的文件名通配符模式。
通配符模式类似于Unix shell使用的那些模式。

- ‘*’ 匹配任意数量字符
- ‘?’ 匹配任意单字
- ‘[chars]’ 匹配任何字符的单个实例；‘-’ 字符可被用来指出一个字符的范围，例如 ‘[a-z]’ 可以用来匹配所有小写字母
- ‘\’ 引用后面的字符

当文件名与通配符匹配时，通配符将不匹配 ‘/’ 字符（在Unix上用于分隔目录名）。由单个 ‘*’ 字符组成的模式是除外；它将始终匹配任何文件名，无论它是否包含 ‘/’ 。在段名称中，通配符将匹配 ‘/’ 字符。

文件名通配符模式只匹配在命令行或输入命令中显式指定的文件。链接器不会搜索目录以扩展通配符。

如果一个文件名匹配多个通配符，或者一个文件名被显示指定了，且又被通配符匹配了，则链接器将使用链接器脚本中的第一个匹配项。例如，例如，下面的输入段描述可能有错误，因为 *data.o* 的规则不会被应用：

```c
.data : { *(.data) }
.data1 : { data.o(.data) }
```

通常情况下，链接器将按照链接过程中出现通配符的顺序放置文件和段。您可以通过使用*SORT_BY_NAME* 关键字来更改此行为，该关键字出现在括号中的通配符模式之前（例如，SORT_BY_NAME（.*text**））。当使用 *SORT_BY_NAME* 关键字时，链接器将按名称按升序对文件或段进行排序，然后将它们放入输出文件中。

*SORT_BY_ALIGNMENT* 对齐方式类似于 *SORT_BY_NAME*. *SORT_BY_ALIGNMENT* 将在将段放入输出文件之前，按对齐方式的降序对段进行排序。大的对齐被放在小的对齐前面可以减少所需的填充量。

*SORT_BY_INIT_PRIORITY* 与 *SORT_BY_NAME* 相似，区别是 *SORT_BY_INIT_PRIORITY*把段按照GCC的嵌入在段名称的 *init_priority* 数字属性值升序排列后放入输出文件。*.init_array.NNNNN* 和 *.fini_array.NNNNN, NNNNN* 是init_priority 。 *.ctors.NNNNN* 和 *.dtors.NNNNN, NNNNN* 是65535减去 init_priority 。

*SORT* 是 *SORT_BY_NAME* 的别名。

当链接器脚本中有嵌套的段排序命令时，段排序命令最多可以有1个嵌套级别。
（1）*SORT_BY_NAME (SORT_BY_ALIGNMENT (wildcard section pattern))* 。它将首先按名称对输入部分进行排序，如果两个部分同名，则按对齐方式排序。
（2）*SORT_BY_ALIGNMENT (SORT_BY_NAME (wildcard section pattern))*。它将首先按对齐方式对输入段进行排序，如果两个段具有相同的对齐方式，则按名称排序。
（3）*SORT_BY_NAME (SORT_BY_NAME (wildcard section pattern))*与 *SORT_BY_NAME (wildcard section pattern)* 相同。
（4）*SORT_BY_ALIGNMENT (SORT_BY_ALIGNMENT (wildcard section pattern))* 与 *SORT_BY_ALIGNMENT (wildcard section pattern)* 相同。
（5）除此之外，其它所有嵌套段排序命令都是无效的。

当同时使用命令行段排序选项和链接器脚本段排序命令时，段排序命令总是优先于命令行选项。

如果链接器脚本中的段排序命令不是嵌套的，那么命令行选项将使段排序命令被视为嵌套的排序命令。
（1）*SORT_BY_NAME (wildcard section pattern )* 与 *–sort-sections alignment* 连用等价于SORT_BY_NAME (SORT_BY_ALIGNMENT (wildcard section pattern)) 。
（2）*SORT_BY_ALIGNMENT (wildcard section pattern)* 与 *–sort-section name* 连用等价于
*SORT_BY_ALIGNMENT (SORT_BY_NAME (wildcard section pattern))*。

如果链接器脚本中的段排序命令是嵌套的，那么命令行选项将被忽略。

*SORT_NONE* 通过忽略命令行部段排序选项来禁用段排序。

如果您对输入段的去向感到困惑， **可以使用 ’ -\*M\* ’ 链接器选项来生成映射文件** 。映射文件精确地显示了如何将输入段映射到输出段。

下面这个示例展示了通配符如何被用来分隔文件。这个链接脚本指引链接器把所有 ‘.*text*’ 段放在’ ‘.*text*’ 里，以及所有 ’.*bss*’ 段放到 ’.*bss*’ 中。链接器将会把所有以大写字母开头的文件的 ’.*data*’ 段放入 ’.*DATA*’ ，其他文件的 ’.*data*’ 段放入 ’.*data*’ 。

```c
SECTIONS {
  .text : { *(.text) }
  .DATA : { [A-Z]*(.data) }
  .data : { *(.data) }
  .bss : { *(.bss) }
}
```

#### 3.6.4.3 Input Section for Common Symbols

普通符号需要一个特别的标记，因为很多目标文件格式中没有特定的普通符号输入段。链接器把普通符号当作位于一个名为 ’*COMMON*’ 的输入段中。

像使用其它文件名与段一样，你也可以使用文件名与 ’*COMMON*’ 段的组合。通过这种方法把一个特定文件的普通符号放入一个段内，同时把其它输入文件的普通符号放入另一个段内。

大多数情况下，输入文件的普通符号会被放到输出文件的 ’.*bss*’ 段里面。例如：

```c
.bss { *(.bss) *(COMMON) }
```

有些目标文件格式含有多种普通符号的类型。例如，MIPS ELF目标文件把标准普通符号和小型普通符号区分开来。在这种情况下，链接器会为另一个类型的普通符号使用其它的特殊段名称。在MIPS ELF中，链接器为普通符号使用 ’*COMMON*’ 以及为小型普通符号使用 ’.*scommon*’ 。这样就可以把不同类型的普通符号映射到内存中的不同位置。

有时在老的链接脚本中能看见 ’[*COMMON*]’ 。这个标记现在已废弃。它等价于’*(*COMMON*)’ 。

#### 3.6.4.4 Input Section and Garbage Collection

使用了链接时垃圾收集(‘*–gc-sections*’)的功能，在把段标记为不应被消除非常常用。此功能通过把一个输入段的通配符入口使用 *KEEP()* 实现，类似于 *KEEP((.init))* 或*KEEP(SORT_BY_NAME()(.ctors))*。

#### 3.6.4.5 Input Section Example

下面是一个完整的链接脚本的例子。它告诉链接器从 *all.o* 读取所有段，把它们放到输出段 *’outputa’* 的开头位置，*’outputa’* 的起始地址为 ’*0x10000*’ 。所有文件 *foo.o* 中的 ’.*input1*’ 段紧跟其后。所有文件 *foo.o* 中的 ’*input2*’ 段放入输出文件的 ’*outputb*’ 中，跟着是 *foo1.o* 中的 ’*input1*’ 段。所有其它的 ’.*input1*” 和 .*input2*’ 段被放入输出段 ’*outputc*’ 。

```c
SECTIONS {
  outputa 0x10000 :
    {
    all.o
    foo.o (.input1)
    }
  outputb :
    {
    foo.o (.input2)
    foo1.o (.input1)
    }
  outputc :
    {
    *(.input1)
    *(.input2)
    }
}
```

如果输出段的名称与输入段的名称相同，并且可以表示为C标识符，那么链接器将自动看到[ PROVIDE](https://sourceware.org/binutils/docs/ld/PROVIDE.html#PROVIDE)两个符号:余下的*__start_SECNAME* 和 *_stop_SECNAME*，其中SECNAME是段的名称。它们分别指示输出段的开始地址和结束地址。注意:大多数段名不能表示为C标识符，因为它们包含 ‘.’ 字符。

### 3.6.5 Output Section Data

你可以通过使用输出段命令*BYTE*, *SHORT*, *LONG*, *QUAD*, 或者 *SQUAD*在输出段显式的包含几个字节的数据。每个关键字后面跟着一个括号包裹的表达式指出需要存储的数值（参照[Expressions](https://sourceware.org/binutils/docs/ld/Expressions.html#Expressions)）。表达式的值被存储在当前位置计数器值的地方。

*BYTE*, *SHORT*, *LONG*, *QUAD*命令分别存储1，2，4，8字节。在存储字节后，位置计数器会按照存储的字节数增加。

例如，下面将会存储一个单字节数据1，然后存储一个符号为 ’*addr*’ 四字节数据的值：

```c
BYTE(1)
LONG(addr)
```

当使用64位主机或目标时，*QUAD* 和*SQUAD*是相同的;它们都存储一个8字节或64位的值。主机和目标都是32位时，表达式被当作32位计算。在这种情况下QUAD存储一个32位的值，并使用0扩展到64位，SQUAD保存32位值并使用符号位扩展到64位。

如果输出文件的目标文件格式显式的指定 *endiannes*，在正常的情况下，值将按照大小端存储。当对象文件格式没有显式的指定 *endianness*，例如，S-records，值将被按照第一个输入目标文件的大小端存储。
注意 - 这些命令仅在段描述内部工作，因此下面的例子会使链接器产生错误：

```c
SECTIONS { .text : { *(.text) } LONG(1) .data : { *(.data) } } 
```

而下面这是可行的：

```c
SECTIONS { .text : { *(.text) ; LONG(1) } .data : { *(.data) } } 
```

您可以使用 *FILL* 命令设置当前段的填充模式。该命令后面跟着一个括号包裹的表达式。所有其它没有被特别指定段的内存区域（例如因为对齐需要而留出来的缝隙）按照表达式的值填充，如果有必要可以重复填充。一个*FILL*语句仅会覆盖它本身在段定义中出现的位置后面的所有内存区域；通过使用不同的*FILL*声明，你可以在一个输出段中使用不同的填充模板。
这个例子显示了如何使用 ’*0x90*’ 填充未定义内存区域：

```c
FILL(0x90909090)
```

FILL命令类似 ’*=fillexp*’ 输出段属性，但其仅影响*FILL*命令后面的段，而不是整个段。如果同时使用，FILL命令为高优先级。参考[ Output Section Fill](https://sourceware.org/binutils/docs/ld/Output-Section-Fill.html#Output-Section-Fill)获取更多填充细节。

### 3.6.6 Output Section Keywords

这里有两个关键字可以作为输出段的命令：

*CREATE_OBJECT_SYMBOLS*

此命令告诉链接器为每个输入文件创建一个符号。每个符号的名字为对应输入文件的名字。每个符号出现的位置位于包含*CREATE_OBJECT_SYMBOLS*命令的输出段中。

这个命令常常是 *a.out* 目标文件格式特有的。 它一般不为其它的目标文件格式所使用。

*CONSTRUCTORS*

当链接时使用 *a.out* 目标文件的格式，链接器使用一个特殊构造集来支持C++ 全局构造函数和析构函数。在链接不支持任意段的文件格式时，例如 *ECOFF* 和 *XCOFF* ，链接器将会通过名字自动识别C++全局构造函数和析构函数。对于这些格式的目标文件，*CONSTRUCTORS*命令告诉链接器把构造函数信息放到出现 *CONSTRUCTORS* 命令的输出段中。其它文件格式中*CONSTRUCTORS*命令被忽略。

符号__CTOR_LIST__ 标记全局构造函数的开始，符号__CTOR_END__标记结束。同样的，__DTOR_LIST__和__DTOR_END__分别标记全局析构函数的开始和结束。第一个列表中的字是入口的数量，后面是每个构造函数或者析构函数的地址，最后是一个全零的字。编译器必须安排实际运行代码。对于这些目标文件格式，GNU C++通常从一个 __main 子程序中调用构造函数，而对 __main 的调用自动被插入到 *main* 的启动代码中。GNU C++通常使用 *atexit* 运行析构函数，或者直接从函数 *exit* 中退出。

对于*COFF*或者*ELF*等支持任意段名字的目标文件格式，GNU C++通常把全局构造函数和析构函数放入 .*ctors* 和 .*dtors* 段。把下面的代码放入你的链接脚本，将会创建GUN C++运行时期望的表。

```c
      __CTOR_LIST__ = .;
      LONG((__CTOR_END__ - __CTOR_LIST__) / 4 - 2)
      *(.ctors)
      LONG(0)
      __CTOR_END__ = .;
      __DTOR_LIST__ = .;
      LONG((__DTOR_END__ - __DTOR_LIST__) / 4 - 2)
      *(.dtors)
      LONG(0)
      __DTOR_END__ = .;
```

如果你正在使用GUN C++支持的初始化优先级，初始化优先级提供了一些对全局构造函数运行顺序的控制，则你必须在链接时对构造函数排序以保证它们以正确的顺序执行。当你使用*CONSTRUCTORS* 命令，使用 ‘*SORT_BY_NAME(CONSTRUCTORS)*’ 替换它。当使用 .*ctors* 和 .*dtors* 段,使用 ‘*(SORT_BY_NAME(.ctors))’ 和’ \*(SORT_BY_NAME(.dtors))’ 取代 ‘\*(.ctors)’ 和’ ‘*(.dtors)’ 。

通常编译器和链接器会自动处理这些问题，您不需要关心它们。但是，在你自己写链接脚本且正在使用C++的时候，你可能需要考虑这些。

### 3.6.7 Output Section Discarding

链接器通常不会创建没有内容的输出段。这是为了方便引用那些有可能出现或者不出现任何输入文件中的段。例如：

```c
.foo : { *(.foo) }
```

只有在至少有一个输入文件含有 ’.*foo*’ 段且 ’.*foo*’ 段不为空的时候才会在输出文件创建一个 ’.*foo*’ 段。其它链接脚本指出在一个段中间分配空间也会创建输出段。赋值也一样即使赋值没有创建空间，除了‘. = 0’, ‘. = . + 0’, ‘. = sym’, ‘. = . + sym’ 和‘. = ALIGN (. != 0, expr, 1)’ 其中 ’*sym*’ 是一个值为0的已定义绝对符号。因此你可以强制一个空的输出段使用 ‘. = .’。

链接器将忽略为丢弃的输出段进行地址赋值（请参见[Output Section Address](https://sourceware.org/binutils/docs/ld/Output-Section-Address.html#Output-Section-Address)），除非链接器脚本在输出段中定义符号。在这种情况下，链接器将遵守地址赋值，有可能更新 ’.’ 的值，即便段被抛弃了。

特殊输出段名称 ’/*DISCARD*/’ 可被用来抛弃输入段。一个被分派到名为 ’/*DISCARD*/’ 的输出段的输入段将不会被包含在输出文件中。

### 3.6.8 Output Section Discarding

我们在前面展示了输出部分的完整描述如下:

```c
section [address] [(type)] :
  [AT(lma)]
  [ALIGN(section_align) | ALIGN_WITH_INPUT]
  [SUBALIGN(subsection_align)]
  [constraint]
  {
    output-section-command
    output-section-command
    …
  } [>region] [AT>lma_region] [:phdr :phdr …] [=fillexp]
```

我们已经描述了*section*, *address*, and *output-section-command*命令。在本节中，我们将描述其余的段属性。

- Output Section Type: 输出段类型
- Output Section LMA: 输出段LMA —加载地址
- Forced Output Alignment: 强制输出对齐
- Forced Input Alignment: 强制输入对齐
- Output Section Constraint: 输出段限制
- Output Section Region: 输出段区域
- Output Section Phdr: 输出段phdr
- Output Section Fill: 输出段填充

#### 3.6.8.1 Output Section Type

每个输出段可以有一个类型。类型是圆括号中的关键字。定义了以下类型:

```c
NOLOAD
```

此段应标记为不可加载，以便在程序运行时不会将其加载到内存中。

```c
DSECT
COPY
INFO
OVERLAY
```

支持这些类型名称是为了向后兼容，而且很少使用。它们都具有相同的效果:该段应该标记为不可分配，以便在程序运行时不会为该段分配内存。

链接器通常根据映射到输出段的输入段设置输出段的属性。您可以使用 *section* 类型来覆盖它。例如，在下面的脚本示例中，’ *ROM* ’ 部分位于内存位置 ’ 0 '，在程序运行时不需要加载它。

```c
SECTIONS {
  ROM 0 (NOLOAD) : { … }
  …
}
```

#### 3.6.8.2 Output Section LMA

每个段有一个虚拟地址(VMA)和一个加载地址(LMA);参见[ Basic Script Concepts](https://sourceware.org/binutils/docs/ld/Basic-Script-Concepts.html#Basic-Script-Concepts)。虚拟地址由前面描述的[ Output Section Address](https://sourceware.org/binutils/docs/ld/Output-Section-Address.html#Output-Section-Address)指定。加载地址由 *AT* 或 *AT>* 关键字指定。指定加载地址是可选的。

***AT\* 关键字把一个表达式当作自己的参数。这将指定段的实际加载地址** 。关键字 AT> 使用内存区域的名字作为参数。参考[MEMORY](https://sourceware.org/binutils/docs/ld/MEMORY.html#MEMORY)。段的加载地址被设置为该区域的当前空闲位置，并且按照段对齐要求对齐。

如果没有为可分配段使用 *AT* 和 *AT>*，链接器会使用下面的方式尝试来决定加载地址：

- 如果段有一个特定的VMA地址，则LMA也使用该地址。
- 如果段为不可分配的则LMA被设置为它的VMA。
  否则如果可以找到符合当前段的一个内存区域，且此区域至少包含了一个段，则设置LMA在那里。如此VMA和LMA的区别类似于VMA和LMA在该区域的上一个段的区别。
- 如果没有声明内存区域且默认区域覆盖了整个地址空间，则采用前面的步骤。
- 如果找不到合适的区域或者没有前面存在的段，则LMA被设置为等于VMA。

这些功能旨在使构建ROM映像变得容易。例如，以下链接器脚本创建三个输出段：一个名为“.*text*”，从0x1000开始；一个名为“.*mdata*”，即使其VMA为0x2000，也加载在“.*text*”节的末尾；另一个名为“.*bss*”，用于在地址0x3000保存未初始化的数据。符号’*_data*’被定义为值0x2000，这表明位置计数器保存VMA值，而不是LMA值。

```c
SECTIONS
  {
  .text 0x1000 : { *(.text) _etext = . ; }
  .mdata 0x2000 :
    AT ( ADDR (.text) + SIZEOF (.text) )
    { _data = . ; *(.data); _edata = . ;  }
  .bss 0x3000 :
    { _bstart = . ;  *(.bss) *(COMMON) ; _bend = . ;}
}
```

此链接脚本的运行时初始化代码应该类似于下面的形式，把初始化数据从ROM镜像复制到运行时地址。注意这些代码是如何利用链接器脚本定义的符号的。

```vbnet
extern char _etext, _data, _edata, _bstart, _bend;
char *src = &_etext;
char *dst = &_data;
 
 
/* ROM has data at end of text; copy it.  /
 
while (dst < &_edata)
 
dst++ = *src++;
 
 
/* Zero bss.  /
 
for (dst = &_bstart; dst< &_bend; dst++)
 
dst = 0;
 
```



#### 3.6.8.3 Forced Output Alignment

你可以使用ALIGN增加输出段的对齐。作为替换，你可以通过ALIGN_WITH_INPUT属性强制VMA与LMA自始至终保持它们之间的区别。
您可以使用*ALIGN*来增加输出段的对齐方式。作为一种替代方法，您可以使用*ALIGN_WITH_INPUT*属性在整个输出段保持VMA和LMA之间的差异。

#### 3.6.8.4 Forced Input Alignment

您可以使用*SUBALIGN*来强制输出段中的输入段对齐。指定的值将覆盖输入段提供的任何对齐方式，无论比原来大还是小。

#### 3.6.8.5 Output Section Constraint

通过分别使用关键字 *ONLY_IF_RO* 和*ONLY_IF_RW*，可以指定只有在所有输入段都是只读或所有输入段都是读写的情况下才创建输出段。

#### 3.6.8.6 Output Section Region

可以使用 ’>*region*’ 把一个段**指定到此前设置的内存区域内**。参见[MEMORY](https://sourceware.org/binutils/docs/ld/MEMORY.html#MEMORY)。

下面是一个例子：

```c
MEMORY { rom : ORIGIN = 0x1000, LENGTH = 0x1000 }
SECTIONS { ROM : { *(.text) } >rom }
```

#### 3.6.8.7 Output Section Phdr

您可以使用 ':*phdr* ’ 将一个段分配给先前定义的程序段。参见[ PHDRS](https://sourceware.org/binutils/docs/ld/PHDRS.html#PHDRS)。如果一个段被分配给一个或多个段，那么所有后续分配的段也将被分配给这些段，除非它们显式地使用 :*phdr* 修饰符。您可以使用:*NONE*来告诉链接器根本不要将该段放在任何段中。

这里有一个简单的例子:

```c
PHDRS { text PT_LOAD ; }
SECTIONS { .text : { *(.text) } :text }
```

#### 3.6.8.8 Output Section Fill

你可以使用’=*fillexp*’为整个段设置填充模板。*fillexp*是一个表达式（参考[Expressions](https://sourceware.org/binutils/docs/ld/Expressions.html#Expressions)）。任何其它的未被特殊指定的输出段的内存区域（例如，因为对其输入段产生的缝隙）将会被用*fillexp*的值填充，如果有需要可以重复填充。如果表达式是一个简单的hex数字，例如一个十六进制数字由’0x’开头且结尾没有 ’*k*’ 或 ’*M*’，则一个任意长的十六进制数字可以被用来给填充模板赋值，前面的0同样成为模板的一部分。在其它情况中，包含额外的括号或者一个一元+，填充模板为表达式值的最低4个有意义的字节。在所有情况中，数字总是大端的。

你也可以使用*FILL*命令设置填充值（参考[Output Section Data](https://sourceware.org/binutils/docs/ld/Output-Section-Data.html#Output-Section-Data)）。

这里有一个简单的例子:

```c
SECTIONS { .text : { *(.text) } =0x90909090 }
```

### 3.6.9 Overlay Description

覆盖描述提供了一种简单的方法来描述将作为单个内存映像的一部分加载但将在相同内存地址上运行的段。在运行时，某种类型的覆盖管理器将根据需要从运行时内存地址复制覆盖的段，可能通过简单地操作寻址位来实现。这种方法可能很有用，例如，当某个内存区域比另一个区域更快时。

覆盖描述使用*OVERLAY*命令。*OVERLAY*命令和*SECTIONS*命令一起使用，就像一个输出段描述符。完整的*OVERLAY*命令的语义如下：

```c
OVERLAY [start] : [NOCROSSREFS] [AT ( ldaddr )]
  {
    secname1
      {
        output-section-command
        output-section-command
        …
      } [:phdr…] [=fill]
    secname2
      {
        output-section-command
        output-section-command
        …
      } [:phdr…] [=fill]
    …
  } [>region] [:phdr…] [=fill] [,]
```

除了*OVERLAY*（关键字），以及每个段都必须有一个名字（上面的*secname1*和*secname2*），所有的部分都是可选的。除了*OVERLAY*中不能为段定义地址和内存区域，使用*OVERLAY*结构定义的段类似于那些普通的*SECTIONS*中的结构（参考[SECTIONS](https://sourceware.org/binutils/docs/ld/SECTIONS.html#SECTIONS)）。

结尾的逗号可能会被使用，如果使用了 *fill* 且下一个 *sections-command* 看起来像是表达式的延续。

所有的段都使用同样的开始地址定义。所有段的载入地址都被排布，使它们在内存中从整个’*OVERLAY*’的载入地址开始都是连续的（就像普通的段定义，载入地址是可选的，缺省的就是开始地址；开始地址也是可选的，缺省是当前的位置计数器的值）。

如果使用了关键字*NOCROSSREFS*，并且在任何段间有互相引用，链接器将会产生一个错误报告。因为所有的段运行在同样的地址，直接引用其它的段通常没有任何意义。参考[NOCROSSREFS](https://sourceware.org/binutils/docs/ld/Miscellaneous-Commands.html#Miscellaneous-Commands)。

每个伴随*OVERLAY*的段，链接器自动提供两个符号。符号*__load_start_secname*被定义为段的起始地址。符号*__load_stop_secname*被定义为段结束地址。任何不符合C定义的伴随*secname*的字符都将被移除。C（或者汇编）代码可以使用这些符号在需要时搬移复盖代码。

覆盖之后，位置计数器的值设置为覆盖的起始值加上最大段的长度。

下面是例子，请记住这应该放在*SECTIONS*结构内。

```c
  OVERLAY 0x1000 : AT (0x4000)
   {
     .text0 { o1/*.o(.text) }
     .text1 { o2/*.o(.text) }
   }
 
```

这将把 ’.*text0*’ 和 ’.*text1*’ 的起始地址设置为地址 *0x1000*。’.*text0*’ 的加载地址为 *0x4000*，’.*text1*’ 会加载到 ’.*text0*’ 后面。下面的符号如果被引用则会被定义： *__load_start_text0*, *__load_stop_text0*, *__load_start_text1*, *__load_stop_text1*。

C代码拷贝覆盖.*text1*到覆盖区域可能像下面的形式。

```c
  extern char __load_start_text1, __load_stop_text1;
  memcpy ((char *) 0x1000, &__load_start_text1,
          &__load_stop_text1 - &__load_start_text1);
```

注意’*OVERLAY*’命令只是为了语法上的便利，因为它所做的所有事情都可以用更加基本的命令加以代替。上面的例子可以用下面的写法：

```c
  .text0 0x1000 : AT (0x4000) { o1/*.o(.text) }
  PROVIDE (__load_start_text0 = LOADADDR (.text0));
  PROVIDE (__load_stop_text0 = LOADADDR (.text0) + SIZEOF (.text0));
  .text1 0x1000 : AT (0x4000 + SIZEOF (.text0)) { o2/*.o(.text) }
  PROVIDE (__load_start_text1 = LOADADDR (.text1));
  PROVIDE (__load_stop_text1 = LOADADDR (.text1) + SIZEOF (.text1));
  . = 0x1000 + MAX (SIZEOF (.text0), SIZEOF (.text1));
 
```

## 3.7 MEMORY Command

链接器的默认配置允许分配所有可用内存。您可以使用 *MEMORY* 命令来重载它。

***MEMORY\* 命令描述目标中内存块的位置和大小**。您可以使用它来描述链接器可以使用哪些内存区域，以及链接器必须避免使用哪些内存区域。你可以把段放到特定的内存区域里。链接器将会基于内存区域设置段地址，如果区域趋于饱和将会产生警告信息。链接器不会为了把段更好的放入内存区域而打乱段的顺序。

链接器脚本可能包含 *MEMORY* 命令的许多用法，但是，定义的所有内存块都被视为在单个 *MEMORY* 命令中指定的。内存的语法是:

```c
MEMORY
  {
    name [(attr)] : ORIGIN = origin, LENGTH = len
    …
  }
```

*name* 是链接器脚本中用于引用内存区域的**名称**。区域名称在链接器脚本之外没有任何意义。区域名称存储在单独的名称空间中，不会与符号名、文件名或段冲突。每个内存区域在 *MEMORY* 命令中必须有一个不同的名称。但是你此后可以使用[REGION_ALIAS](https://sourceware.org/binutils/docs/ld/REGION_005fALIAS.html#REGION_005fALIAS)命令为已存在的内存区域添加别名。

*attr* 字符是一个**可选的属性**列表，用于指定是否对链接器脚本中未显式映射的输入段使用特定的内存区域。如[ SECTIONS](https://sourceware.org/binutils/docs/ld/SECTIONS.html#SECTIONS)中所述，如果不为某些输入段指定输出段，则链接器将创建一个与输入段同名的输出段。如果定义区域属性，链接器将使用它们为它创建的输出段选择内存区域。
*attr* 字符串只能使用下面的字符组成：

- ‘*R*’ 只读段
- ‘*W*’ 读写段
- ‘*X*’ 可执行段
- ‘*A*’ 可分配段
- ‘*I*’ 已初始化段
- ‘*L*’ 类似于’I’
- ‘!’ 反转其后面的所有属性
  如果一个未映射段匹配了上面除 ’!’ 之外的一个属性，它就会被放入该内存区域。’!’ 属性对该测试取反，所以只有当它不匹配上面列出的行何属性时，一个未映射段才会被放入到内存区域。

*origin* 是**内存区域起始地址**的数值表达式。表达式的计算结果必须为常量，并且不能包含任何符号。关键字*ORIGIN*可以缩写为*org* 或 *o*（但不能是*ORG*）。

*len* 是**内存区域的字节大小**的表达式。与原始表达式一样，表达式必须仅为数值，并且必须计算为常量。关键字长度可以缩写为 *len* 或 *l*。

在下面的示例中，我们指定有两个内存区域可供分配：一个从“0”开始空间大小为256k字节，另一个从“0x40000000”开始空间大小为4M字节。链接器将把未显式映射到内存区域的每个部分放入“*rom*” 内存区域，这些部分要么是只读的，要么是可执行的。链接器会将未显式映射到内存区域的其他部分放入 “*ram*” 内存区域。

```c
MEMORY
  {
    rom (rx)  : ORIGIN = 0, LENGTH = 256K
    ram (!rx) : org = 0x40000000, l = 4M
  }
```

定义内存区域后，可以使用 ‘*>region*’ 输出段属性指引链接器把特殊输出段放到该内存区域。例如，如果您有一个名为 ‘*mem*’ 的内存区域，你可以在输出段定义中使用 ’>*mem*’。请参见[Output Section Region](https://sourceware.org/binutils/docs/ld/Output-Section-Region.html#Output-Section-Region)。如果没有为输出段指定地址，链接器将把地址设置为内存区域内的下一个可用地址。如果指向某个内存区域的组合输出段对于该区域来说太大，则链接器将发出错误消息。

可以通过 *ORIGIN(memory)* 和 *LENGTH(memory)* 函数获得内存区域的起始地址以及长度：

```c
 _fstack = ORIGIN(ram) + LENGTH(ram) - 4;
```

## 3.8 PHDRS Command

ELF对象文件格式使用程序头，类似于段。程序头描述了如何将程序加载到内存中。您可以使用带有 ’ -p ’ 选项的 *objdump* 程序将它们打印出来。

当您在本地运行ELF程序时，系统加载程序将读取程序头以确定如何加载程序。只有当程序头设置正确时，这才会工作。本手册没有详细描述系统加载程序如何解释程序头;有关更多信息，请参见ELF ABI。

默认的链接器将会创建合适的程序头部。但是，有些情况下，你可能需要更加精确地指定程程序头。可以使用 *PHDRS* 命令达到此目的。当链接器在链接器脚本中看到*PHDRS*命令时，它将只创建指定的程序头。

链接器仅在创建ELF输出文件时才会关注*PHDRS*命令。其他情况下链接器将会忽视*PHDRS*。

下面是*PHDRS*的语法。*PHDRS*, *FILEHDR*, *AT*, *FLAGS*都是关键字：

```c
PHDRS
{
  name type [ FILEHDR ] [ PHDRS ] [ AT ( address ) ]
        [ FLAGS ( flags ) ] ;
}
```

*name* 仅用于链接器脚本的 *SECTIONS* 命令中的引用。它不会被放到输出文件中。程序头名称存储在单独的名称空间中，不会与符号名称、或者段名产生冲突。每个程序头必须有一个不同的名称。头按照顺序执行，且通常将它们以上升的加载顺序映射到段。

具体的程序头类型描述系统加载程序将从文件加载的头部段。在链接器脚本中，可以通过放置可再分配输出段在头部段内来指定头部段的内容。您可以使用’:phdr '输出段属性将段放在特定的段中。请参阅[Output Section Phdr](https://sourceware.org/binutils/docs/ld/Output-Section-Phdr.html#Output-Section-Phdr)。

将某一个段放在多于一个的段中是很正常的。这仅仅意味着一个内存段包含另一个内存段。可以为每个应当包含段的头部段重复使用 ’:*phdr*’ 命令。

如果使用 ‘：*phdr*’ 将段放在一个或多个段中，则链接器会将所有未指定 ‘：*phdr*’ 的后续可分配段放在同一段中。 这是为了方便起见，因为通常将一整套连续段放在单个段中。 您可以使用：NONE覆盖默认段，并告诉链接器不要将该段放在任何段中。

您可以在程序头类型之后使用 *FILEHDR* 和 *PHDRS* 关键字来进一步描述段的内容。 *FILEHDR*关键字意味着该段应包含ELF文件头。 *PHDRS*关键字意味着该段应包括ELF程序头本身。 如果应用于可加载段（*PT_LOAD*），则所有先前的可加载段都必须具有以下关键字之一。

类型可以是以下之一。 数字表示关键字的值。

- PT_NULL (0)表示未使用的程序头。
- PT_LOAD (1)表示此程序头描述了要从文件中加载的段。
- PT_DYNAMIC (2)表示可以找到动态链接信息的段。
- PT_INTERP (3)表示可以在其中找到程序解释器名称的段。
- PT_NOTE (4)表示包含注释信息的段。
- PT_SHLIB (5)保留的程序头类型，由ELF ABI定义但未指定。
- PT_PHDR (6)表示可以在其中找到程序头的段。
- PT_TLS（7）指示包含线程本地存储的段。
- expression 该表达式给出程序头的数字类型。 这可以用于上面未定义的类型。

您可以使用 *AT* 表达式指定将段加载到内存中的特定地址。 这与 *AT* 作为输出段使用属性时的方法一样（参考[Output Section LMA](https://sourceware.org/binutils/docs/ld/Output-Section-LMA.html#Output-Section-LMA)）。程序头的*AT*命令会覆盖输出段属性。

链接器通常会根据组成段的段来设置段标志。 您可以使用 *FLAGS* 关键字来显式指定段标志。 标志的值必须是整数。 它用于设置程序头的 *p_flags* 字段。

下面是一个PHDRS例子， 显示了在本机ELF系统上使用的一组典型的程序头。

```kotlin
PHDRS
{
  headers PT_PHDR PHDRS ;
  interp PT_INTERP ;
  text PT_LOAD FILEHDR PHDRS ;
  data PT_LOAD ;
  dynamic PT_DYNAMIC ;
}
SECTIONS
 
{
 
. = SIZEOF_HEADERS;
 
.interp : { (.interp) } :text :interp
 
.text : { (.text) } :text
 
.rodata : { (.rodata) } / defaults to :text /
 
…
 
. = . + 0x1000; / move to a new page in memory /
 
.data : { (.data) } :data
 
.dynamic : { *(.dynamic) } :data :dynamic
 
…
 
}
 
```



## 3.9 VERSION Command

使用ELF时，链接器支持符号版本。符号版本仅在使用共享库时有用。当动态链接器运行的程序可能已链接到共享库的早期版本时，动态链接器可以使用符号版本来选择函数的特定版本。

可以在主链接器脚本中直接包含版本脚本，也可以将版本脚本作为隐式链接器脚本提供。您也可以使用’–version script’链接器选项。

VERSION命令的语法是：

```c
VERSION { version-script-commands }
```

版本脚本命令的格式与solaris2.5中 Sun链接器使用的格式相同。版本脚本定义了一个版本节点树。您可以在版本脚本中指定节点名称和相互依赖关系。可以指定将哪些符号绑定到哪个版本节点，还可以把一组指定的符号限定到本地范围，这样在共享库的外面它们就不是全局可见的了。

演示版本脚本语言的最简单方法是使用几个示例：

```mipsasm
VERS_1.1 {
	 global:
		 foo1;
	 local:
		 old*;
		 original*;
		 new*;
};
 
 
VERS_1.2 {
 
foo2;
 
} VERS_1.1;
 
 
VERS_2.0 {
 
bar1; bar2;

extern "C++" {

ns::*;
 
"f(int, double)";
 
};
 
} VERS_1.2;
 
```



这个示例版本脚本定义了三个版本节点。定义的第一个版本节点是 *VERS_1.1*’；它没有其他依赖项。脚本将符号 ‘*foo1*’ 绑定到 ‘*VERS_1.1*’ 。脚本把一些符号缩减到局部可见，因此在共享库外部它们将是不可见的；这是使用通配符模式完成的，因此以’*old*’,’*original*’,’*new*’开头的符号将被匹配上。通配符模板与shell匹配文件名时使用的方法一致。但是，如果把特指的符号名放在双引号中，则名字被按照字面意思处理，而不是正则表达式模板。

接下来，版本脚本定义节点 ‘*VERS_1.2*’ 。此节点依赖于 ‘*VERS_1.1*’。脚本将符号 ‘foo2’绑定到版本节点 ‘*VERS_1.2*’。

最后，版本脚本定义节点 ‘*VERS_2.0*’ 。此节点依赖于 ‘*VERS_1.2*’ 。脚本将符号 ‘*bar1*’ 和 ‘*bar2*’ 绑定到版本节点 ‘*VERS_2.0*’。

当链接器发现在库中定义的符号没有特别绑定到版本节点时，它将有效地将其绑定到库的未指定的基本版本。通过在版本脚本的某个地方使用 ‘*global: *;*’，可以将所有其他未指定的符号绑定到给定的版本节点。注意，在全局规范中使用通配符有点疯狂，除了在最后一个版本节点上。其他地方的全局通配符可能会意外地将符号添加到为旧版本导出的集合中。这是错误的，因为旧版本应该有一套固定的符号。

版本节点的名字没有什么特殊含义，但会给人阅读带来便利。‘*2.0*’版本也可以出现在 ‘*1.1*’ 和 ‘*1.2*’ 之间。然而，这将是一种令人困惑的编写版本脚本的方法。

节点名可以省略，前提是它是版本脚本中唯一的版本节点。这样的版本脚本不为符号指定任何版本，只选择哪些符号将全局可见，哪些符号不可见。

```c
{ global: foo; bar; local: *; };
```

当您将应用程序链接到具有版本化符号的共享库时，应用程序本身知道它需要每个符号的哪个版本，还知道它需要链接到的每个共享库中的哪个版本节点。因此，在运行时，动态加载程序可以快速检查以确保链接到的库实际上提供了应用程序解析所有动态符号所需的所有版本节点。以这种方式，动态链接器可以确定地知道它所需要的所有外部符号将是可解析的，而不必搜索每个符号引用。

符号版本控制实际上是一种更为复杂的方法，可以进行*SunOS*所做的次要版本检查。这里要解决的基本问题是，对外部函数的引用通常根据需要进行绑定，而不是在应用程序启动时全部绑定。如果共享库过期，则可能缺少所需的接口；当应用程序尝试使用该接口时，它可能会突然意外地失败。使用符号版本控制，如果应用程序使用的库太旧，用户在启动程序时会收到警告。

*Sun*的版本控制方法有几个GNU扩展。其中第一项功能是将符号绑定到源文件中定义符号的版本节点，而不是在版本控制脚本中。这主要是为了减轻库维护的工作量。你可以这样做：

```c
__asm__(".symver original_foo,foo@VERS_1.1");
```

在C源文件中。这会将函数‘*original_foo*’ 重命名为绑定到版本节点 ‘*VERS’1.1*’ 的 ‘*foo*’ 的别名。‘*local*:’ 指令可用于阻止导出符号 ‘*original_foo*’ 。‘.*symver*’ 指令优先于版本脚本。

第二个GNU扩展允许同一个函数的多个版本出现在给定的共享库中。通过这种方式，您可以在不增加共享库的主要版本号的情况下对接口进行不兼容的更改，同时仍然允许与旧接口链接的应用程序继续运行。

为此，必须在源文件中使用多个‘.*symver*’ 指令。下面是一个例子：

```c
__asm__(".symver original_foo,foo@");
__asm__(".symver old_foo,foo@VERS_1.1");
__asm__(".symver old_foo1,foo@VERS_1.2");
__asm__(".symver new_foo,foo@@VERS_2.0");
```

在本例中，’*foo@*’ 表示符号 ’*foo*’ 绑定到没有指定基础版本的符号版本。源文件包含此例子将会定义四个C函数：’*original_foo*’, ‘*old_foo*’, ‘*old_foo1*’, ‘*new_foo*’。

当给定符号有多个定义时，需要使用某种方法指定对该符号的外部引用将绑定到的默认版本。您可以使用‘.*symver*’指令 的’ *foo@@VERS_2.0* '类型的’来完成此操作。以这种方式，只能将符号的一个版本声明为默认值;否则，您将实际上拥有同一符号的多个定义。

如果你希望绑定共享库中的一个符号到特定版本，只需很方便的使用别名（例如，’*old_foo*’），或者可以用 ’.*symver*’ 指令指定一个绑定到外部函数的特定版本。

也可以指定版本脚本使用的语言：

```c
VERSION extern "lang" { version-script-commands }
```

支持的 ‘*lang* ’是‘*C*’、‘*C++*’ 和 ‘*Java*’。链接器会在链接时遍历符号列表，并根据‘*lang* ’将它们与‘*version-script-commands*’中指定的模式进行匹配。默认的 ‘*lang* ’是‘*C*’。

被分解的名字可能含有空格以及其他特殊字符。按照上面说的，可以使用正则表达式模板匹配分解的名字，或者可以使用双引号包裹的字符串来精确匹配字符串。在后一种情况中，注意位于版本脚本和分解输出间一个小的不同（比如空格）将会引起不匹配。分解器创建的字符串在未来可能会改变，即便将被重新组合的名字本身没变，在升级版本时你需要检查所有的版本指令是否都按照你期待的那样工作。

## 3.10 Expressions in Linker Scripts

链接器脚本语言中的表达式的语法与C表达式的语法相同。所有表达式都被计算为整数。所有表达式都以相同的大小计算，如果主机和目标都是32位，则为32位，否则为64位。

可以在表达式中使用和设置符号值。

链接器定义了几个用于表达式的特殊用途内建函数。

- Constants: 常数
- Symbolic Constants: 符号常量
- Symbols: 符号名称
- Orphan Sections: 孤儿段
- ocation Counter: 位置计数器
- Operators: 运算符号
- Evaluation: 求值
- Expression Section: 表达式的段
- Builtin Functions: 内建函数

### 3.10.1 Constants

所有的常量都是整数。

与C中一样，链接器认为以 ‘*0*’ 开头的整数是八进制数，以 ‘*0x*’ 或 ‘*0X*’开头的整数是十六进制数。另外，链接器接受后缀 ‘*h*’ 或 ‘*H*’ 表示十六进制，‘*o*’ 或 ‘*O*’ 表示八进制，‘*b*’ 或 ‘*B*’ 表示二进制，‘*d*’ 或 ‘*D*’ 表示十进制。任何没有前缀或后缀的整数值都被认为是小数。

此外，您可以使用后缀 *K* 和 *M* 分别将一个常数缩放为1024或1024*1024。例如，以下均为同一数量:

```c
_fourk_1 = 4K;
_fourk_2 = 4096;
_fourk_3 = 0x1000;
_fourk_4 = 10000o;
```

注意，*K* 和 *M* 后缀不能与前面的其他系数同时使用。

### 3.10.2 Symbolic Constants

可以通过使用 *CONSTANT(name)* 操作符来引用特定于目标的常量，其中 *name*为:
*MAXPAGESIZE*：目标的最大页面大小。
*COMMONPAGESIZE*：目标的默认页大小。

例如：

```c
  .text ALIGN (CONSTANT (MAXPAGESIZE)) : { *(.text) }
```

将会创建一个对齐到目标支持的最大页边界的代码段。

### 3.10.3 Symbol Names

除引号外，符号名称以字母、下划线或句点开始，可以包括字母、数字、下划线、句点和连字符。非引用符号名称不能与任何关键字冲突。你可以指定一个包含奇数字符的符号或与关键字同名的符号，用双引号包围符号名称:

```c
"SECTION" = 9;
"with a space" = "also with a space" + 10;
```

由于符号可以包含许多非字母字符，用空格分隔符号是最安全的。例如，‘*A-B*’ 是一个符号，而 ‘*A - B*’ 是一个包含减法的表达式。

### 3.10.4 Orphan Sections

输出文件中没有显式放置在链接器文件中的段。链接器仍将通过查找或创建适当的输出段将这些段复制到输出文件中，以便在其中放置孤立的输入段。

如果孤立输入段的名称与现有输出段的名称完全匹配，则孤立输入段将放置在该输出段的末尾。

如果没有具有匹配名称的输出段，则将创建新的输出段。每个新的输出段都将具有与其中放置的孤立段相同的名称。如果有多个具有相同名称的孤立段，这些将被合并到一个新的输出段中。

如果创建新的输出节段来保存孤立的输入段，则链接器必须决定将这些新输出段相对于现有输出节的位置。在大多数现代目标上，链接器试图将孤立段放在同一属性的段之后，例如代码与数据、可加载与不可加载等。如果找不到具有匹配属性的段，或者目标缺少此支持，则孤儿段将放在文件的末尾。

命令行选项 ‘–orphan-handling’ 和 ‘–unique’ (请参[Command-line Options](https://sourceware.org/binutils/docs/ld/Options.html#Options))可以用于控制孤儿放在哪个输出段。

### 3.10.5 The Location Counter

特殊的链接器变量 ‘.’ 始终包含当前输出位置计数器。因为 ’.’ 经常当作一个输出段的地址使用，因此它只能位于*SECTIONS*命令中以一个表达式形式出现。任何普通符号可以出现在表达式中的位置都可以使用 ’.’。

为 ’.’ 赋值将会使得位置计数器移动。这可以用来在输出段中创建空的区域。位置计数器不能在一个输出段内向回移动，也不能在段外回退，如果这么做了将会创建重叠的LMA。

```c
SECTIONS
{
  output :
    {
      file1(.text)
      . = . + 1000;
      file2(.text)
      . += 1000;
      file3(.text)
    } = 0x12345678;
}
```

在上面的例子里，文件 *file1* 的 ’*text*’ 段位于输出段 *output* 的起始位置。其后有个*1000*字节的缝隙。此后 *file2* 的 ’.*text*’ 段出现在输出段内，其后也有*1000*字节的缝隙，最后是 *file3* 的 ’.*text*’ 段。标记 ’=*0x12345678*’ 指定了应当向缝隙中填充的内容（参考[Output Section Fill](https://sourceware.org/binutils/docs/ld/Output-Section-Fill.html#Output-Section-Fill)）。

注：’.’ 实际上是指从当前包含对象开始的字节偏移量。通常为 *SECTIONS* 声明，起始地址为0，因此 ’.’ 可以被当作一个绝对地址使用。但是如果 ’.’ 被在段描述符内使用，它表示从该段开始的偏移地址，不是一个绝对地址。因此在下面脚本中：

```c
SECTIONS
{
    . = 0x100
    .text: {
      *(.text)
      . = 0x200
    }
    . = 0x500
    .data: {
      *(.data)
      . += 0x600
    }
}
```

‘.*text*’ 段将会被安排到起始地址*0x100*，实际大小为*0x200*字节，即便 ’.*text*’ 输入段没有足够的数据填充该区域（反之如果数据过多，将会产生一个错误，因为将会尝试向前回退 ’.’ ）。段 ’.*data*’ 将会从*0x500*开始，并且输出段会有额外的*0x600*字节空余空间在输入段’.*text*’。

如果链接器需要放置孤儿段，则将符号设置为输出段语句外部的位置计数器的值可能会导致意外的值。例如，给定如下:

```xml
SECTIONS
{
    start_of_text = . ;
    .text: { *(.text) }
    end_of_text = . ;
 
start_of_data <span class="token operator">=</span> <span class="token punctuation">.</span> <span class="token punctuation">;</span>
<span class="token punctuation">.</span>data<span class="token punctuation">:</span> <span class="token punctuation">{<!-- --></span> <span class="token operator">*</span><span class="token punctuation">(</span><span class="token punctuation">.</span>data<span class="token punctuation">)</span> <span class="token punctuation">}</span>
end_of_data <span class="token operator">=</span> <span class="token punctuation">.</span> <span class="token punctuation">;</span>
 
 
start_of_data <span class="token operator">=</span> <span class="token punctuation">.</span> <span class="token punctuation">;</span>
<span class="token punctuation">.</span>data<span class="token punctuation">:</span> <span class="token punctuation">{<!-- --></span> <span class="token operator">*</span><span class="token punctuation">(</span><span class="token punctuation">.</span>data<span class="token punctuation">)</span> <span class="token punctuation">}</span>
end_of_data <span class="token operator">=</span> <span class="token punctuation">.</span> <span class="token punctuation">;</span>
}
 
```



如果链接器需要放置一些输入段，例如 ’.*rodata*’ 没有在脚本中提及，可能会被选择放到 ’.text’ 和 ’.*data*’ 段中间。你可能会觉得链接器应该把 ’.*rodata*’ 放在上面脚本的空行处，但空行对于链接器来说没有任何实际意义。同样的，链接器也不会把符号名与段联系起来。实际上，它假设所有定义或者其他声明属于前面的输出段，除了特殊情况设定 ’.’。例如，链接器将会类似于下面的脚本放置孤儿段：

```xml
SECTIONS
{
    start_of_text = . ;
    .text: { *(.text) }
    end_of_text = . ;
 
start_of_data <span class="token operator">=</span> <span class="token punctuation">.</span> <span class="token punctuation">;</span>
<span class="token punctuation">.</span>rodata<span class="token punctuation">:</span> <span class="token punctuation">{<!-- --></span> <span class="token operator">*</span><span class="token punctuation">(</span><span class="token punctuation">.</span>rodata<span class="token punctuation">)</span> <span class="token punctuation">}</span>
<span class="token punctuation">.</span>data<span class="token punctuation">:</span> <span class="token punctuation">{<!-- --></span> <span class="token operator">*</span><span class="token punctuation">(</span><span class="token punctuation">.</span>data<span class="token punctuation">)</span> <span class="token punctuation">}</span>
end_of_data <span class="token operator">=</span> <span class="token punctuation">.</span> <span class="token punctuation">;</span>
 
 
start_of_data <span class="token operator">=</span> <span class="token punctuation">.</span> <span class="token punctuation">;</span>
<span class="token punctuation">.</span>rodata<span class="token punctuation">:</span> <span class="token punctuation">{<!-- --></span> <span class="token operator">*</span><span class="token punctuation">(</span><span class="token punctuation">.</span>rodata<span class="token punctuation">)</span> <span class="token punctuation">}</span>
<span class="token punctuation">.</span>data<span class="token punctuation">:</span> <span class="token punctuation">{<!-- --></span> <span class="token operator">*</span><span class="token punctuation">(</span><span class="token punctuation">.</span>data<span class="token punctuation">)</span> <span class="token punctuation">}</span>
end_of_data <span class="token operator">=</span> <span class="token punctuation">.</span> <span class="token punctuation">;</span>
}
 
```



这能符合或者不符合脚本作者对于 *start_of_data* 的设置意图。一种影响孤儿段放置的办法是为位置计数器指定自身的值，链接器会认为一个 ’.’ 的设置是设定一个后面段的起始地址，因此该段应为一个组。因此可以这么写：

```xml
SECTIONS
{
    start_of_text = . ;
    .text: { *(.text) }
    end_of_text = . ;
 
<span class="token punctuation">.</span> <span class="token operator">=</span> <span class="token punctuation">.</span> <span class="token punctuation">;</span>
start_of_data <span class="token operator">=</span> <span class="token punctuation">.</span> <span class="token punctuation">;</span>
<span class="token punctuation">.</span>data<span class="token punctuation">:</span> <span class="token punctuation">{<!-- --></span> <span class="token operator">*</span><span class="token punctuation">(</span><span class="token punctuation">.</span>data<span class="token punctuation">)</span> <span class="token punctuation">}</span>
end_of_data <span class="token operator">=</span> <span class="token punctuation">.</span> <span class="token punctuation">;</span>
 
 
<span class="token punctuation">.</span> <span class="token operator">=</span> <span class="token punctuation">.</span> <span class="token punctuation">;</span>
start_of_data <span class="token operator">=</span> <span class="token punctuation">.</span> <span class="token punctuation">;</span>
<span class="token punctuation">.</span>data<span class="token punctuation">:</span> <span class="token punctuation">{<!-- --></span> <span class="token operator">*</span><span class="token punctuation">(</span><span class="token punctuation">.</span>data<span class="token punctuation">)</span> <span class="token punctuation">}</span>
end_of_data <span class="token operator">=</span> <span class="token punctuation">.</span> <span class="token punctuation">;</span>
}
 
```



这样以来，孤儿段 ’.rodata’ 将会被放置在 *end_of_text* 和 *start_of_data*之间。

### 3.10.6 Operators

链接器识别标准的C算术运算符集，具有标准绑定和优先级级别:

```c
precedence      associativity   Operators                Notes
(highest)
1               left            !  -  ~                  (1)
2               left            *  /  %
3               left            +  -
4               left            >>  <<
5               left            ==  !=  >  <  <=  >=
6               left            &
7               left            |
8               left            &&
9               left            ||
10              right           ? :
11              right           &=  +=  -=  *=  /=       (2)
(lowest)
```

注意:(1)前缀操作符(2)参见[ Assignments](https://sourceware.org/binutils/docs/ld/Assignments.html#Assignments)。

### 3.10.7 Evaluation

链接器惰性地计算表达式。它只在绝对必要时才会去计算表达式的值。

链接器需要一些信息，例如第一部分的起始地址的值，以及内存区域的来源和长度，才能够完成所有的链接工作。这些值会在链接器读链接脚本的时候立即计算。

但是其他的值（例如符号值）在存储分配之后才能知道或者需要。这种值将会推迟计算，直到符号赋值表达式的其他信息（例如输出段的大小）都可获得后。

分区的大小在分配之后才能知道，因此依赖它的赋值都将在分配后才会执行。

某些表达式（例如依赖于位置计数器 ‘.’ 的表达式）必须在段分配期间求值。

如果表达式的结果是必需的，但该值不可用，则会产生错误。例如，下面这样的脚本：

```c
SECTIONS
  {
    .text 9+this_isnt_constant :
      { *(.text) }
  }
```

会导致错误消息：‘*non constant expression for initial address*’

### 3.10.8 The Section of an Expression

地址和符号可以是段相对的，也可以是绝对的。段的相对符号是可重定位的。如果使用 ‘-*r*’ 选项请求可重定位输出，则进一步的链接操作可能会更改段相对符号的值。另一方面，绝对符号将在任何进一步的链路操作中保持相同的值。

链接器表达式中的某些术语是地址。对于段相关符号和返回地址的内置函数（如*ADDR*、*LOADADDR*、*ORIGIN* 和 *SEGMENT_START*），都是如此。其他术语只是数字，或者是返回非地址值（如长度）的内置函数。一个复杂的问题是，除非您设置*LD_FEATURE*（“*SANE_EXPR*”）（请参见[Miscellaneous Commands](https://sourceware.org/binutils/docs/ld/Miscellaneous-Commands.html#Miscellaneous-Commands)），否则数字和绝对符号将根据其位置进行不同的处理，以与旧版本的LD兼容。出现在输出段定义之外的表达式将所有数字视为绝对地址。出现在输出段定义中的表达式将绝对符号视为数字。如果给定了*LD_FEATURE*（“SANE_EXPR”），则任何位置的绝对符号和数字都被简单的当作数字。

在下面这个简单的例子中：

```c
SECTIONS
  {
    . = 0x100;
    __executable_start = 0x100;
    .data :
    {
      . = 0x10;
      __data_start = 0x10;
      *(.data)
    }
    …
  }
```

在上述两个赋值例子中：’.’ 和 ’*__executable_start*’ 都被设置为绝对地址*0x100*，在后两个赋值中，’.’ 和 ’*__data_start*’ 被设置为相对于 ’.data’ 的0x10。

对于涉及数字、相对地址和绝对地址的表达式，ld采用以下规则求值:

- 对绝对地址或数字进行一元运算，对两个绝对地址或两个数字进行二进制运算，或在一个绝对地址和一个数字之间进行二元运算，在数值上应用运算符。
- 对一个相对地址的一元运算，以及对同一部分中的两个相对地址或一个相对地址与一个数字之间的两个相对地址的二进制运算，将运算符应用于地址的偏移部分。
- 其他二进制操作，即不在同一段中的两个相对地址之间，或相对地址和绝对地址之间，在应用运算符之前，首先将任何非绝对项转换为绝对地址。

每个子表达式的结果部分如下:

- 只有数字参与的运算符结果为数字。
- 比较运算’&&’和’||’的结果也是数字。
- 对同一部分中的两个相对地址或两个绝对地址（在上述转换之后）进行的其他二进制算术和逻辑操作的结果，当*LD_FEATURE*（“*SANE_EXPR*”）或在输出部分定义内时，也是一个数字，但在其他情况下是绝对地址。
- 对相对地址或一个相对地址和一个数字进行其他操作的结果是，在相对操作数的同一部分中有一个相对地址。
- 对绝对地址的其他操作（在上述转换之后）的结果是一个绝对地址。
- 

可以使用内建函数*ABSOLUTE*来强制一个本来是相对地址的表达式变为绝对地址。例如，要创建一个设置为输出段‘.*data*’结尾地址的绝对符号：

```c
SECTIONS
  {
    .data : { *(.data) _edata = ABSOLUTE(.); }
  }
```

如果不使用’*ABSOLUTE*’，’*_edata*’将会为’.*data*’段的相对地址。

使用*LOADADDR*也会强制一个表达式变为绝对地址，因为此特殊内建函数返回一个绝对地址。

### 3.10.9 Builtin Functions

链接器脚本语言包括许多用于链接器脚本表达式的内建函数。

- *ABSOLUTE(exp)*
  返回表达式exp的绝对值(不可重定位，非负)。主要用于在段定义中为符号赋绝对值，其中符号值通常是段相对的。参见[ABSOLUTE(exp)](https://sourceware.org/binutils/docs/ld/Expression-Section.html#Expression-Section)。
- ***ADDR(section)***
  **返回名为 ’\*section\*’ 的段的地址（VMA）**。你的脚本必须事先为该段定义了位置。在下面的例子里，*start_of_output_1*, *symbol_1*, *symbol_2*分配了同样的值，除了*symbol_1*将是相对于.output1段的，而其他两个值是绝对的:

```c
SECTIONS { …
  .output1 :
    {
    start_of_output_1 = ABSOLUTE(.);
    …
    }
  .output :
    {
    symbol_1 = ADDR(.output1);
    symbol_2 = start_of_output_1;
    }
… }
```

- ***ALIGN(align)***
- ***ALIGN(exp,align)***
  返回位置计数器(.)或任意表达式对齐到下一个align指定边界的值。单操作数*ALIGN*并不会改变位置计数器的值，它只是对其进行算术运算。两个操作数*ALIGN*允许任意表达式向上对齐(*ALIGN(ALIGN*)等价于*ALIGN*(绝对(.)，*ALIGN*))。

下面是一个示例，它将输出 .*data*段对齐到上一段之后的下一个*0x2000*字节边界，并将该段中的一个变量设置为输入段之后的下一个*0x8000*字节边界:

```c
SECTIONS { …
  .data ALIGN(0x2000): {
    *(.data)
    variable = ALIGN(0x8000);
  }
… }
```

在本例中，*ALIGN*的第一次使用指定了段的位置，因为它被用作段定义的可选地址属性(参见[Output Section Address](https://sourceware.org/binutils/docs/ld/Output-Section-Address.html#Output-Section-Address))。*ALIGN*的第二种用法用于定义符号的值。

内建函数NEXT与ALIGN密切相关。

- ***ALIGNOF(section)***
  如果section已分配，返回名为*section*的对齐字节。如果段还没被分配，链接器会报错。下面的例子里，.*output*段的对齐存储在该段的第一个值。

```c
SECTIONS{ …
  .output {
    LONG (ALIGNOF (.output))
    …
    }
… }
```

- ***BLOCK(exp)***
  这是*ALIGN*的同义词，用于与旧的链接器脚本兼容。在设置输出段的地址时最常见。
- ***DATA_SEGMENT_ALIGN(maxpagesize, commonpagesize)***
  它等于任何一个

```c
(ALIGN(maxpagesize) + (. & (maxpagesize - 1)))
```

或者

```c
(ALIGN(maxpagesize)
 + ((. + commonpagesize - 1) & (maxpagesize - commonpagesize)))
```

这取决于后者对数据段(表达式结果和*DATA_SEGMENT_END*之间的区域)使用的*commonpagesize*大小的页面是否比前者更少。如果使用后一种形式
如果后面的形式被使用了，表示着保存*commonpagesize*字节的运行时内存，花费的代价最多浪费*commonpagesize*大小的磁盘空间。

此表达式只能直接在*SECTIONS*命令中使用，不能在任何输出段描述中使用，并且只能在链接器脚本中使用一次。*commonpagesize*应该小于或等于*maxpagesize*，并且应该是对象希望优化的系统页面大小，同时仍在系统页面大小达到*maxpagesize*时运行。但是请注意，如果系统页面大小大于*commonpagesize*，则‘*-z relro*’保护将无效。
例如：

```c
 . = DATA_SEGMENT_ALIGN(0x10000, 0x2000);
```

- ***DATA_SEGMENT_END(exp)***
  此命令为DATA_SEGMENT_ALIGN运算定义了数据段的结尾。

```c
  . = DATA_SEGMENT_END(.);
```

- ***DATA_SEGMENT_RELRO_END(offset, exp)***
  此命令为使用 ’*-z relro*’ 命令的情况定义了*PT_GNU_RELRO*段的结尾。当 ’*-z relro*’ 选项不存在时，*DATA_SEGMENT_RELRO_END*不做任何事情，否则将填充*DATA_SEGMENT_ALIGN*，以便*exp* + 偏移量与*DATA_SEGMENT_ALIGN*给定的*commonpagesize*参数对齐。如果它出现在链接器脚本中，那么它必须放在*DATA_SEGMENT_ALIGN*和*DATA_SEGMENT_END*之间。计算为第二个参数加上*PT_GNU_RELRO*段末尾由于节对齐而需要的任何填充。

```c
  . = DATA_SEGMENT_RELRO_END(24, .);
```

- ***DEFINED(symbol)***
  如果符号在链接器全局符号表中，并且在脚本中定义的语句之前定义，则返回1，否则返回0。可以使用此函数为符号提供默认值。例如，以下脚本片段演示如何将全局符号 ‘*begin*’ 设置为 ‘.*text*’ 段中的第一个位置，但如果名为 ‘*begin*’ 的符号已经存在，则其值将被保留。

```c
SECTIONS { …
  .text : {
    begin = DEFINED(begin) ? begin : . ;
    …
  }
  …
}
```

- ***LENGTH(memory)***
  返回名为 *memory* 内存区域的长度。
- ***LOADADDR(section)***
  返回名为 *section* 的段的LMA绝对地址（参见[Output Section LMA](https://sourceware.org/binutils/docs/ld/Output-Section-LMA.html#Output-Section-LMA)）。
- ***LOG2CEIL(exp)***
  返回 *exp* 的二进制对数，取整为无穷大。LOG2CEIL（0）返回0。
- ***MAX(exp1, exp2)***
  返回 *exp1* 和 *exp2* 的最大值。
- ***MIN(exp1, exp2)***
  返回 *exp1* 和 *exp2* 的最小值。
- ***NEXT(exp)***
  返回下一个未分配的地址，它是 *exp* 的倍数。此函数与 A*LIGN(exp)*密切相关；除非使用 *MEMORY* 命令为输出文件定义不连续内存，否则这两个函数是等效的。
- ***ORIGIN(memory)***
  返回名为 *memory* 的内存区域的起始地址。
- ***SEGMENT_START(segment, default)***
  返回命名段的基址。如果已经为此段指定了显式值（使用命令行 ‘*-T*’ 选项），则将返回该值，否则该值将为默认值。目前，’*-T*’命令行选项只能用于设置 “*text*” 、“*data*” 和 “*bss*” 段的基址，但你可以使用SEGMENT_START搭配任何段名字。
- ***SIZEOF(section)***
  返回名为 *section* 段的字节数。如果段还没被分配就是用函数求值，将会产生错误。下面是一个例子，*symbol_1* 和 *symbol_2* 的值相同：

```c
SECTIONS{ …
  .output {
    .start = . ;
    …
    .end = . ;
    }
  symbol_1 = .end - .start ;
  symbol_2 = SIZEOF(.output);
… }
```

- ***SIZEOF_HEADERS***
- ***sizeof_headers***
  返回输出文件头的大小（以字节为单位）。这是一个会出现在输出文件的起始位置的信息。如果您愿意，您可以在设置第一段的起始地址时使用此数字，以方便分页。

生成ELF输出文件时，如果链接器脚本使用 *SIZEOF_HEADERS* 内建函数，则链接器必须在确定所有节地址和大小之前计算程序头的数量。如果链接器后来发现它需要额外的程序头，它将报告一个错误“没有足够的空间来容纳程序头”。要避免此错误，必须避免使用 *SIZEOF_HEADERS* 函数，或者必须重新编写链接器脚本以避免强制链接器使用其他程序头，或者必须使用 *PHDRS* 命令自己定义程序头（请参见[ PHDRS](https://sourceware.org/binutils/docs/ld/PHDRS.html#PHDRS)）。

## 3.11 Implicit Linker Scripts

如果你指定了一个链接输入文件，而链接器无法将其识别为一个目标文件或者库文件，它将尝试将该文件作为链接器脚本读取。如果无法将文件解析为链接器脚本，则链接器将报告错误。

隐式链接器脚本不会替换默认链接器脚本。

通常，隐式链接器脚本只包含符号分配，或 *INPUT*、*GROUP* 或 *VERSION* 命令。

读取任何输入文件时，由于隐式链接器脚本将在命令行中读取隐式链接器脚本的位置读取，这会影响库的搜索。