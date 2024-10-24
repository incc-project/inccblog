## 为什么不能用LR(1)解析器解析C++

https://stackoverflow.com/questions/243383/why-cant-c-be-parsed-with-a-lr1-parser

问题描述: 许多语言都可以用LR(1)解析器来解析, 比如C, 但一个例外是C++, 需要通过LR(\*)才能解析, 这是为什么?

首先要纠正的是, C语言也无法用标准的LR(1)解析器来解析. 考虑下面这个代码:

```c++
x * y ;
```

有两种不同的解释:

* x类型的指针, 名为y;
* x和y的乘积

在没有类型信息的前提下, 这明显是一个二义性文法.

因此, 纯LR解析器是无法处理这个问题的.

现实中大多数C/C++解析器通过交织语法分析与语义分析来处理这个二义文法, 即在语法解析过程中记录符号表信息, 当遇到x时, 解析器就是到x是类型还是变量了, 不过这种解析器就不是严格的上下文无关解析器了.

在实现中, 我们可以通过一些奇技淫巧在类似Bison这种现有的LR解析器中实现这种交织分析, 实际上, GCC4.0前就是这么做的. 不过之后GCC团队就放弃了这种方案, 采用手工编码解析, 可能是为了支持更好的错误诊断.

> 扩展:
>
> 还有另一种方法，这种方法既简洁又好用，而且可以很好地解析 C 和 C++，无需任何符号表, 该技术称之为[GLR 解析器](http://en.wikipedia.org/wiki/GLR_parser)。这是一种完全上下文无关的解析器（具有有效的无限前瞻功能）。
>
> DMS 软件工具包的 C 和 C++ 前端使用了这项技术（截至 2017 年 6 月，这些工具可以处理 MS 和 GNU 方言中的完整 C++17）。它们已用于处理数百万行大型 C 和 C++ 系统，通过完整、精确的解析生成包含源代码完整细节的 AST。（请参阅[AST 以了解 C++ 最棘手的解析。](https://stackoverflow.com/a/17393852/120163)）

抛开类型问题, LR(1)对于C++而言也是不够的. 

[Lambda the Ultimate](http://lambda-the-ultimate.org/)上有一个有趣的帖子，讨论了[C++ 的 LALR 语法](http://lambda-the-ultimate.org/node/2158#comment-27800)。[它包含一个博士论文](http://www.computing.surrey.ac.uk/research/dsrg/fog/FogThesis.pdf)的链接，其中包含对 C++ 解析的讨论，其中指出：

> “C++ 语法具有歧义性、依赖于上下文，并且可能需要无限前瞻来解决某些歧义”。

并给出了许多例子（参见 pdf 的第 147 页）。

例子1:

```cpp
int(x), y, *const z;
```

语义: 3个声明.

```cpp
int x;
int y;
int *const z;
```

例子2:

```cpp
int(x), y, new int;
```

语义: 3个逗号分隔的表达式, 一个对x的强制类型转换, 一个y, 一个int临时变量的地址, 最终返回int临时变量的地址.

```cpp
(int(x)), (y), (new int));
```

可以看到确定int(x)是声明还是表达式需要无限前瞻.

最后, 只要存在模板, 就很难有纯粹的上下文无关解析器 (当然, 确实是有GLR这种黑科技).

比如这个例子:

```cpp
template<bool> struct a_t;

template<> struct a_t<true> {
    template<int> struct b {};
};

template<> struct a_t<false> {
   enum { b };
};

typedef a_t<sizeof(void*)==sizeof(int)> a;

enum { c, d };
int main() {
    a::b<c>d; // declaration or expression?
}
```

如果不进行模板解析, 无法确定`a::b<c>d`是声明还是表达式, 从而产生二义性解析.

有空可以了解下GLR.
