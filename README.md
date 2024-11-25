# INCC Blog

This is the blog page of INCC group.

# C++

## C++ Crash Debugging

How to debug c++ crash. [link](./cpp/debug-cpp-crash.md)

## Overload, Override, Overwrite

Understand c++ overload, override, overwrite. [link](./cpp/overload-ride-write)

## C++20 Module

Introduce c++20 module. [link](./cpp/cpp-module.md)

## Template: Java vs C++

The difference between Java templates and C++ templates. [link](./cpp/template-java-vs-cpp.md)

## Template instantiation

Introduce explicit specialization, explicit instantiation, implicit instantiation, overloading resolution. [link](./cpp/template-instantiation.md)

## Left Value and Right Value

Introduce left value and right value. [link](./cpp/ptr-lvref-rvref-mv.md)

## Const Smart Pointer

Introduce const smart pointer. [link](./cpp/const-smart-ptr.md)

## CRTP

Introduce C++ crtp. [link](./cpp/crtp.md)

## Mixin

Introduce C++ mixin. [link](./cpp/mixin.md)

## RTTI

Introduce C++ rtti. [link](./cpp/rtti.md)

## optional, variant, any

Introduce C++17 optional, variant, any. [link](./cpp/optional-variant-any.md)

## Virtual function

Introduce C++ virtual function. [link](./cpp/virtual-function.md)

## decltype, SFINAE, concept

Introduce decltype, SFINAE, concept. [link](./cpp/decltype-sfinae-concept.md)

# Frontend

## Preprocessing and Lexing

### Preprocessing Directives

Introduce preprocessing directives. [link](./frontend/pp-lex/pp-directives.md)

## Parsing, Sema and IR Generation

### Save / Load AST

How to save / load the clang AST? [link](./frontend/parse-sema-ir/ast-save-load.md)

### RecursiveASTVisitor

An introduction to `RecursiveASTVisitor`.  [link](./frontend/parse-sema-ir/RecursiveASTVisitor.md)

### Template Instantiation

An example of parsing template, instantiating functions, and generating IR. [link](./frontend/parse-sema-ir/clang-inst.md)

### DeclContext

Introduce Clang `DeclContext`. [link](./frontend/parse-sema-ir/DeclContext.md)

### Clang-Repl

A C++ interpreter. [link](./frontend/parse-sema-ir/clang-repl.md)

### Partt Parser

Introduce partt parser. [link](./frontend/parse-sema-ir/pratt-parser.md)

### C++ and LR parser

Why can't we use LR (1) parser to parse C++?  [link](./frontend/parse-sema-ir/cpp-and-lr-parser.md)

# Backend

## Optimization

### LTO

Introduce LLVM LTO and debug the source code. [link](./backend/optimization/lto.md)

## 数据流分析

介绍编译优化所需要的数据流分析框架. [link](./backend/optimization/数据流分析基础.pptx)

## Codegen

### LLVM MC

Introduce LLVM MC. [link](./backend/codegen/llvm-mc.md)

# Linking

## Comdat

How to handle conflicting symbols? [link](./linking/comdat.md)

## Linking Script

Introduce linking script. [link](./linking/linking-script.md)

## lld

Debug the source code of lld. [link](./linking/lld.md)

## llvm-link

Debug the source code of llvm-link. [link](./linking/llvm-link.md)

# Binary

## Binary Analysis

组会分享-二进制分析简介: [link](./binary/组会分享-二进制分析简介.pptx)

## DWARF

组会分享-DWARF简介: [link](./binary/组会分享-dwarf简介.pptx)

# Paper

## Continuous program optimization: Design and evaluation

IEEE Transactions on Computers 2002

Thomas Kistler, Member, IEEE, and Michael Franz, Member, IEEE

本文通过收集代码的动态运行数据, 对代码进行PGO优化, 并用优化后的代码热更新优化前的代码, 整个过程会持续进行. [link](./paper/Continuous-program-optimization-Design-and-evaluation.md)

## Enabling Fine-Grained Incremental Builds by Making Compiler Stateful

CGO 24

Ruobing Han[1], Jisheng Zhao[1], Hyesoon Kim[1]

[1] Georgia Institute of Technology Atlanta, USA

设计了一种有状态编译方法, 可以复用历史编译产物, 从而加速增量编译. 

具体而言, 有状态编译器将保留上次编译时pass的休眠信息, 并在后续编译中绕过休眠的pass.

作者在Clang上扩展了本文的有状态编译方法, 相比Clang加速了6.72%. [link](./paper/Enabling-Fine-Grained-Incrementa- Builds-by-Making-Compiler-Stateful.md)

## Incremental Whole Program Optimization and Compilation

CGO 17

Patrick W. Sathyanathan[1], Wenlei He[1], and Ten H. Tzen[1] 

[1]Microsoft Corp, USA

面向全程序的增量编译优化.

关键技术: 

* 描述符号之间依赖关系的依赖图, 数据流格, 用于最小化需要重新分析/编译的代码数量.
* 轻量级checksum, 用于检测函数和变量的修改.
* 在多级内敛展开下仍能确保无差异的代码生成. (Binary等价)

相比Visual C/C++ compiler达到了7倍的加速比. [link](./paper/Incremental-Whole-Program-Optimization-and-Compilation.md)

## ThinLTO: Scalable and Incremental LTO

CGO 17

Teresa Johnson[1], Mehdi Amini[2], Xinliang David Li[1]

[1] Google, USA

[2] Apple, USA

相关文档也可以参考LLVM Project Blog: https://blog.llvm.org/2016/06/thinlto-scalable-and-incremental-lto.html

现有LTO框架不scalable, 在大型项目上优化效率较低. 因此本文提出了一种scalable且支持增量的LTO框架: ThinLTO. [link](./paper/ThinLTO-Scalable-and-Incrementa- LTO.md)

## The Architecture of Montana: An Open and Extensible Programming Environment with an Incremental C++ Compiler 

Michael Karasick

Programming Environments and Compilation IBM T.J. Watson Research Center 

ACM SIGSOFT Software Engineering Notes, Volume 23, Issue 6

Montana即包含了构建系统, 也包含了C++编译器. 支持细粒度的增量编译.

最大的问题是其没有采用C++标准, 而是自定义标准, 需要开发者重新学习其自定义标准.  [link](./paper/Montana.md)

# Tool

## Clang Tool Example

An example on how to implement a clang tool. [link](./tool/clang-tool-example.md)

## Clang Plugin Example

An example on how to implement a clang plugin. [link](./tool/clang-plugin-example.md)

# Misc

## Save LLVM Build Time

Save the build time of LLVM. [link](./misc/save-build-time.md)

## Clang Driver

Introduce clang driver and the workflow of LLVM. [link](./misc/clang-driver.md)

## ccc-print-bindings

A brief introduce of  `ccc-print-bindings`. [link](./misc/clang-ccc.md)

## LLVM RTTI

Introduce LLVM RTTI. [link](./misc/LLVM-RTTI.md)

## Google Rust Report

Research shows that most developers believe that Rust's compilation speed is unacceptable. [link](./misc/google-rust-report.md)

# Starts

C++年度总结: https://cloud.tencent.com/developer/article/2257751

程序语言与编译技术相关资料: https://github.com/shining1984/PL-Compiler-Resource

推荐编译原理课程: 中科大编译原理

推荐静态分析课程: 南京大学软件分析

推荐基于LLVM的自制编译器教程: https://llvm.org/docs/tutorial/

推荐链接教程: 深入理解计算机系统 第7章 链接

推荐二进制分析教程: 二进制分析实战

