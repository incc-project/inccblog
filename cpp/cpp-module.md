# 一文读懂C++20 新特性之module（模块）

https://blog.csdn.net/Jxianxu/article/details/127499762

## 概述
C++20中新增了“模块(module)”的概念，用以解决传统的头文件在编译时间及程序组织上的问题。那么传统的在源文件中包含头文件的方式有哪些弊端呢？概括下来，主要有以下几点：

* 效率低下。我们一般在头文件中提供C++实体（类，函数，变量等）的前置声明，C++预处理器在遇到头文件时会就地将头文件中的所有字节原模原样地拷贝到包含此头文件的源文件中，而不去分析是否头文件中有不需要被包含的信息。这导致编译时间增加。
* 传递性。头文件中定义的C++实体可在包含此头文件的的文件中传递。
* 标识符不能被有效隔离。倘若多个头文件中定义了同样的标识符(如变量等)，并且这些头文件被同一源文件包含，那么这些标识符会产生冲突。

C++20中引入的“模块”的概念将致力于解决上述问题。在进入模块的正式介绍之前，我们先明确一个概念。

**模块单元（Module Unit）**

C++中的基本编译单元称为“翻译单元（即translation unit）”。一个翻译单元由一个源文件和该源文件所直接或间接包括的头文件的内容组成。C++20中的模块是一种新的翻译单元，称为模块单元。

好，现在进入正题

## 声明模块单元

### 声明模块接口单元

​        模块接口单元是可以被其他源文件导入的模块单元，它用来声明其可供导出的C++实体(类，函数，变量等)，当然在模块接口单元中也可以进行C++实体的定义。

```cpp
export module 模块名;
```

```cpp
//my_module.cppm

export module myModule;  //module关键字指明这是一个模块，export关键字指明这是一个模块接口单元

void internal_function(){ //这是一个只在该模块内部可见的函数，不可导出
	//do something;
}
export void say_hello(){  //export关键字指明本模块导出函数say_hello()
	internal_function();
	//....
	return;
}
export void say_world();  //导出函数say_world, 函数在此处只提供声明

//当需要导出的C++实体太多时，不必一一指定，可以在下面的大括号中统一指定
export
{
    int var;
    void i_am_export_function(){ //...; };
    //other export entity
}

// main.cpp

import myModule;
import <string_view>;

int main(){
	say_hello();
    say_world();
    i_am_export_function();
	internal_helper();//error, 此函数没有被导出，不能访问
	return 0;
}
```

### 声明模块实现单元

​        模块实现单元提供C++实体的定义，上面的例子中， 导出函数say_world()在模块接口单元myModule中只提供了声明，我们可以在模块实现单元中对其进行定义。

```cpp
module 模块名;//注意：没有export关键字
module myModule;

void say_world() {
    //...
}
```

### 声明模块分区

​        当模块的规模过大时，可以将大模块划分为多个模块分区

```cpp
export module mainModule:modulePartition; // 为模块 'mainModule' 声明模块接口分区：'modulePartition' 
export void part_funtion();

module mainModule:partB; //为模块‘mainModule’声明模块实现分区
void partB_function()    //注意，此函数只在模块mainModule中可见
{
    //...
}
```

### 声明全局模块

​        当我们通过module关键字声明一个模块时，从声明语句后开始，我们就再不允许通过#include来导入头文件了。C++标准规定需用import关键字导入头文件，或者在全局模块中用#include来导入头文件这一方法来作为过渡。

注意：如果一个模块单元有一个全局模块片段，那么它的首个声明必须是 module;。然后在全局模块片段中只能出现预处理指令。然后用一个标准的模块声明结束这个全局模块片段，后面就是模块内容。

```cpp
/// myModule.cppm （'myModule' 的主模块接口单元）

module; ///声明一个全局模块片段,在全局模块片段中，可通过#include的方式导入传统的头文件

#include <stdlib.h>
#include <iostream>

export module A;         //此语句后就不能使用#include包含头文件了

export void export_function();
```

## 导入模块单元

我们通过import关键字导入模块

### 导入其他模块

```cpp

/// A.cpp （'A' 的主模块接口单元）
export module A;
 
export char const* hello() { return "hello"; }
 
/// B.cpp （'B' 的主模块接口单元）
export module B;
 
import A;//导入模块A
 
//export import A;//注意此处的export关键字
 
export char const* world() { return "world"; }
 
/// main.cpp （非模块单元）
#include <iostream>
import B;
 
int main()
{
    std::cout << hello() << ' ' << world() << '\n';
}
```

此处需要注意的是导入的模块不具有传递性，比如我们在模块B中导入模块A，则模块B中能看到模块A中导出的内容。main.cpp中导入了B，则在main.cpp中可以看到B中的导出接口，但是看不到A中的导出接口，如果main.cpp中也想看到A中的导出接口的话，那么在B中必须用export import A；的方式导出。

### 导入模块分区

​    我们通过 export(可选) import :分区名的方式导入模块分区    

```cpp
/// A-B.cpp， 为主模块A定义模块接口分区B 
export module A:B; 
...

/// A-C.cpp， 为主模块A定义模块实现分区C
module A:C;
...

/// A.cpp，定义主模块分区A
export module A; 

import :C;         //导入模块实现分区C
export import :B;  //导入模块接口分区B

...
```

### 导入传统头文件

我们在上文提到过，module声明后将不再允许通过#include方式导入传统头文件，C++标准允许我们通过import关键字来导入传统头文件

```cpp
export module A;

import <iostream>; //导入传统头文件<iostream>, <iostream>只在本模块中可见
export import<string_view>; //导入传统头文件，export关键字使得导入A的文件也能看到<string_view>

main.cpp
#include <iostream> //注意：#include必须在import之前
import module A;
int main()
{

}
```

值得注意的是，这种方法似乎目前编译器还不支持，在g++ 11.2.0中会报如下错误：

```shell
error: failed to read compiled module: No such file or directory
note: compiled module file is 'gcm.cache/./usr/include/c++/11/iostream.gcm'
note: imports must be built before being imported
fatal error: returning to the gate for mechanical issue
```

我通过上文中在全局模块片段中包含头文件的方法解决

在g++中编译时，要指定 -fmodules-ts开关才能编译module，并且被导入的模块需要先于导入它的模块编译，比如main.cpp中导入了moduleA，则需要先编译moduleA：

g++ -o out moduleA.cpp main.cpp -std=c++2a -fmodules-ts

## 模块使用限制

### 关于export关键字的限制

(1) export不能导出具有内部链接的C++实体，静态变量、函数以及定义在匿名命名空间中的类、变量、函数皆具有内部链接。

```cpp
export module myModule;
//静态变量，函数具有内部链接
export static int data = 1; 
export static void function() {} 

//匿名命名空间具有内部链接
namespace {
    export class Demo {} 
}

//main.cpp
import myModule;
int main() { 
    std::cout<<data<<"\n"; //error: 'data' was not declared in this scope
    function();            //error: 'function' was not declared in this scope
    Demo d;                //error: 'Demo' was not declared in this scope
}
```

(2) 只能在命名空间作用域或全局命名空间中导出实体。

```cpp
export class Demo {
    int a;
    export int b; // Illegal
};

export void function() {
    export int value = 30; // Illegal
}
```

(3) “using声明”中所指向的具有非内部链接或非模块链接的C++实体也可进行导出，但using namespace声明不能被导出。

```cpp
namespace NS1 {
    export class Demo1{};
    class Demo2{};
 
    namespace {
        class Demo3{};
    }
}
 
export using NS1::Demo1; // Okay
export using NS1::Demo2; // Illegal
 
export using NS1::Demo3; // Illegal
 
export using namespace NS1; // Illegal
```

(4) 命名空间也可以被导出，但是只能导出命名空间中那些满足条件的C++实体。

```cpp

export namespace NS {
    int Var= 8; // Okay. Var is exported as NS::Var;
 
    static int ill= 0; // Illegal
 
    namespace {
        void ill_function() {} // Illegal
    }
}
```

(5) 被导出的C++实体需在第一次声明时用export关键字，其后的声明或定义均不需再指定export。

```cpp
export class Domo; 
 
class Domo{ // Implicit `export` keyword
    int m_a;
    int m_b;
};
 
class Demo1; //do not export Demo1
 
export class Demo1; // Illegal!, Deomo1 is not declared as exported class
```

### 关于import关键字的使用限制

(1) import语句必须出现在任何C++实体的声明之前。

```cpp
export module myModule;

import mouduleB;

void func();

import moduleC; // illegal! Move it above `function`
```

(2) import语句仅能出现在全局作用域中。

```cpp
void function() {
    import myModule; // Illegal
}

namespace {
    import myModule; // Illegal
}
```

## 参考文献

Modules (since C++20) - cppreference.com

cxx-modules - GCC Wiki

# 头文件、模块和预编译头文件

https://learn.microsoft.com/en-us/cpp/build/compare-inclusion-methods?view=msvc-170

Historically, you'd include the standard library with a directive like `#include <vector>`. However, it's expensive to include header files because they're reprocessed by every source file that includes them.

Precompiled headers (PCH) were introduced to speed compilation by translating them once and reusing the result. But precompiled headers can be difficult to maintain.

In C++20, modules were introduced as a significant improvement on header files and precompiled headers.

<font color="red">Header units were introduced in C++20 as a way to temporarily bridge the gap between header files and modules. They provide some of the speed and robustness benefits of modules, while you migrate your code to use modules.</font>

<font color="red">Then, the C++23 standard library introduced support for importing the standard library as named modules. This is the fastest and most robust way to consume the standard library.</font>

To help you sort out the different options, this article compares the traditional `#include` method against precompiled headers, header units, and importing named modules.

The following table is arranged by compiler processing speed and robustness, with `#include` being the slowest and least robust, and `import` being the fastest and most robust.

| Method                                                       | Summary                                                      |
| :----------------------------------------------------------- | :----------------------------------------------------------- |
| `#include`                                                   | One disadvantage is that they expose macros and internal implementation. Internal implementation is often exposed as functions and types that start with an underscore. That's a convention to indicate that something is part of the internal implementation and shouldn't be used.  Header files are fragile because the order of #includes can modify behavior or break code and are affected by macro definitions.  Header files slow compilation. Particularly when multiple files include the same file because then the header file is reprocessed multiple times. |
| [Precompiled header](https://learn.microsoft.com/en-us/cpp/build/creating-precompiled-header-files?view=msvc-170) | A precompiled header (PCH) improves compile time by creating a compiler memory snapshot of a set of header files. This is an improvement on repeatedly rebuilding header files.  PCH files have restrictions that make them difficult to maintain.  PCH files are faster than `#include` but slower than `import`. |
| [Header units](https://learn.microsoft.com/en-us/cpp/build/walkthrough-header-units?view=msvc-170) | This is a new feature in C++20 that allows you to import 'well-behaved' header files as modules.  Header units are faster than `#include`, and are easier to maintain, significantly smaller, and also faster than pre-compiled header files (PCH).  Header units are an 'in-between' step meant to help transition to named modules in cases where you rely on macros defined in header files, since named modules don't expose macros.  Header units are slower than importing a named module.  Header units aren't affected by macro defines unless they're specified on the command line when the header unit is built--making them more robust than header files.  <font color="red">Header units expose the macros and internal implementation defined in them just as header file do, which named modules don't. </font> As a rough approximation of file size, a 250-megabyte PCH file might be represented by an 80-megabyte header unit file. |
| [Modules](https://learn.microsoft.com/en-us/cpp/cpp/modules-cpp?view=msvc-170) | This is the fastest and most robust way to import functionality.  Support for importing modules was introduced in C++20. The C++23 standard library introduces the two named modules described in this topic.  When you import `std`, you get the standard names such as `std::vector`, `std::cout`, but no extensions, no internal helpers such as `_Sort_unchecked`, and no macros.  The order of imports doesn't matter because there are no macro or other side-effects.  As a rough approximation of file size, a 250-megabyte PCH file might be represented by an 80-megabyte header unit file, which might be represented by a 25-megabyte module.  Named modules are faster because when a named module is compiled into an `.ifc` file and an `.obj` file, the compiler emits a structured representation of the source code that can be loaded quickly when the module is imported. The compiler can do some work (like name resolution) before emitting the `.ifc` file because of how named modules are order-independent and macro-independent--so this work doesn't have to be done when the module is imported. In contrast, when a header file is consumed with `#include`, its contents must be preprocessed and compiled again and again in every translation unit.  Precompiled headers, which are compiler memory snapshots, can mitigate those costs, but not as well as named modules. |

If you can use C++20 features and the C++23 standard library in your app, use named modules.

If you can use C++20 features but want to transition over time to modules, use header units in the interim.

If you can't use C++20 features, use `#include` and consider precompiled headers.

## See also

[Precompiled header files](https://learn.microsoft.com/en-us/cpp/build/creating-precompiled-header-files?view=msvc-170)
[Overview of modules in C++](https://learn.microsoft.com/en-us/cpp/cpp/modules-cpp?view=msvc-170)
[Tutorial: Import the C++ standard library using modules](https://learn.microsoft.com/en-us/cpp/cpp/tutorial-import-stl-named-module?view=msvc-170)
[Walkthrough: Import STL libraries as header units](https://learn.microsoft.com/en-us/cpp/build/walkthrough-import-stl-header-units?view=msvc-170#approach1)
[Walkthrough: Build and import header units in your Visual C++ projects](https://learn.microsoft.com/en-us/cpp/build/walkthrough-header-units?view=msvc-170)