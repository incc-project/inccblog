https://zhuanlan.zhihu.com/p/620724283

本文主要内容为C++中RTTI的简单介绍和LLVM RTTI的使用方法、简单实现解析。

本文会先后讲解以下内容：

1. 简单介绍C++中`typeid`和`dynamic_cast`两个关键字的基本使用和基本实现原理
2. 分析其性能和空间开销，引入LLVM为什么要自己实现RTTI而不是用标准实现
3. 详解LLVM RTTI的使用方法和超简单实现

本文中相关示例代码均可在以下仓库中找到：

[zhangyi1357/FunGadgets: Some little programs for learning C++ and coding (github.com)github.com/zhangyi1357/FunGadgets](https://link.zhihu.com/?target=https%3A//github.com/zhangyi1357/FunGadgets)

## 1. C++标准RTTI

C++提供了`typeid`和`dynamic_cast`两个关键字来提供动态类型信息和动态类型转换，使用需要在在编译器选项中指定`-rtti`（clang和gcc都默认开启），关闭则可以设置选项`-fno-rtti`，其具体使用方法可以参考cppreference网站中的示例。

### 1.1 typeid

`typeid`[使用示例](https://link.zhihu.com/?target=https%3A//en.cppreference.com/w/cpp/language/typeid)：

```cpp
#include <iostream>
#include <string>
#include <typeinfo>
 
struct Base {}; // non-polymorphic
struct Derived : Base {};
 
struct Base2 { virtual void foo() {} }; // polymorphic
struct Derived2 : Base2 {};
 
int main()
{
    int myint = 50;
    std::string mystr = "string";
    double *mydoubleptr = nullptr;
 
    std::cout << "myint has type: " << typeid(myint).name() << '\n'
              << "mystr has type: " << typeid(mystr).name() << '\n'
              << "mydoubleptr has type: " << typeid(mydoubleptr).name() << '\n';
 
    // std::cout << myint is a glvalue expression of polymorphic type; it is evaluated
    const std::type_info& r1 = typeid(std::cout << myint); // side-effect: prints 50
    std::cout << '\n' << "std::cout<<myint has type : " << r1.name() << '\n';
 
    // std::printf() is not a glvalue expression of polymorphic type; NOT evaluated
    const std::type_info& r2 = typeid(std::printf("%d\n", myint));
    std::cout << "printf(\"%d\\n\",myint) has type : " << r2.name() << '\n';
 
    // Non-polymorphic lvalue is a static type
    Derived d1;
    Base& b1 = d1;
    std::cout << "reference to non-polymorphic base: " << typeid(b1).name() << '\n';
 
    Derived2 d2;
    Base2& b2 = d2;
    std::cout << "reference to polymorphic base: " << typeid(b2).name() << '\n';
 
    try
    {
        // dereferencing a null pointer: okay for a non-polymorphic expression
        std::cout << "mydoubleptr points to " << typeid(*mydoubleptr).name() << '\n'; 
        // dereferencing a null pointer: not okay for a polymorphic lvalue
        Derived2* bad_ptr = nullptr;
        std::cout << "bad_ptr points to... ";
        std::cout << typeid(*bad_ptr).name() << '\n';
    }
    catch (const std::bad_typeid& e)
    {
         std::cout << " caught " << e.what() << '\n';
    }
}
```

其在clang14编译器下编译运行的结果为：

```text
myint has type: i
mystr has type: NSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEE
mydoubleptr has type: Pd
50
std::cout<<myint has type : So
printf("%d\n",myint) has type : i
reference to non-polymorphic base: 4Base
reference to polymorphic base: 8Derived2
mydoubleptr points to d
bad_ptr points to...  caught std::bad_typeid
```

以上的示例中有一点值得特别注意，非多态对象（没有[虚函数表](https://zhida.zhihu.com/search?q=虚函数表&zhida_source=entity&is_preview=1)）的`typeid`结果是在编译期确定的，这是因为`typeid`的实现原理就是在虚函数表中多插入了一项指向`type_info`的指针，编译器会为类型维护相应的`type_info`结构体。所以没有虚函数表的对象自然没有这项特性，会在编译期就确定其类型。

### 1.2 dynamic_cast

`dynamic_cast`[使用示例](https://link.zhihu.com/?target=https%3A//en.cppreference.com/w/cpp/language/dynamic_cast)：

```cpp
#include <iostream>
 
struct V
{
    virtual void f() {} // must be polymorphic to use runtime-checked dynamic_cast
};
 
struct A : virtual V {};
 
struct B : virtual V
{
    B(V* v, A* a)
    {
        // casts during construction (see the call in the constructor of D below)
        dynamic_cast<B*>(v); // well-defined: v of type V*, V base of B, results in B*
        dynamic_cast<B*>(a); // undefined behavior: a has type A*, A not a base of B
    }
};
 
struct D : A, B
{
    D() : B(static_cast<A*>(this), this) {}
};
 
struct Base
{
    virtual ~Base() {}
};
 
struct Derived: Base
{
    virtual void name() {}
};
 
int main()
{
    D d; // the most derived object
    A& a = d; // upcast, dynamic_cast may be used, but unnecessary
 
    [[maybe_unused]]
    D& new_d = dynamic_cast<D&>(a); // downcast
    [[maybe_unused]]
    B& new_b = dynamic_cast<B&>(a); // sidecast
 
    Base* b1 = new Base;
    if (Derived* d = dynamic_cast<Derived*>(b1); d != nullptr)
    {
        std::cout << "downcast from b1 to d successful\n";
        d->name(); // safe to call
    }
 
    Base* b2 = new Derived;
    if (Derived* d = dynamic_cast<Derived*>(b2); d != nullptr)
    {
        std::cout << "downcast from b2 to d successful\n";
        d->name(); // safe to call
    }
 
    delete b1;
    delete b2;
}
```

其在Clang14编译器下的编译运行结果为：

```text
downcast from b2 to d successful
```

以上结果可以看到，对于指针转换失败的情况`dynamic_cast`会返回`nullptr`，这就使得我们可以在程序中判断基类指针是否是特定类型从而针对特定类型做出特别的操作。

### 1.3 实现原理

前文也有简单提到过以上RTTI机制的实现原理，这里具体解释一下。

对于非多态类型，没有虚函数表和虚[函数指针](https://zhida.zhihu.com/search?q=函数指针&zhida_source=entity&is_preview=1)的类型对象，`typeid`可以在编译期即完成计算，也就不存在RTTI机制，`dynamic_cast`更是没有必要，使用`static_cast`即可满足需求。

对于多态类型，概念图示如下[[1\]](https://zhuanlan.zhihu.com/p/620724283#ref_1)（不同编译器实现细节有所不同，但大致情况类似），在虚函数表中多维护了一个指向`type_info`结构体的指针，通过该指针即可很容易地完成`typeid`的功能。



![img](./LLVM-RTTI.assets/v2-2b9be4fadaf20ded84b057b177b8c398_720w.webp)

XX继承自X

而`dynamic_cast`的实现则要稍微多考虑一些情况，首先是派生类[对象指针](https://zhida.zhihu.com/search?q=对象指针&zhida_source=entity&is_preview=1)/引用向基类对象指针/引用的转换，称为向上转换（向上向下转换定义参考下图[[2\]](https://zhuanlan.zhihu.com/p/620724283#ref_2)），向上转换实际上编译器会实现为静态转换，并不需要运行时动态转换，因为子类一定包含基类。

![img](./LLVM-RTTI.assets/v2-20315da1926b4eb535d6de92522747da_720w.webp)

对于向下转换编译器则有真正的工作要做：例如对以下的继承链[[3\]](https://zhuanlan.zhihu.com/p/620724283#ref_3)来说，将一个原本是`C`类型的对象的`A*`指针转换为`C*` 指针也很快，只需要检查一下`type_info`结构体是否相同，但是如果要转换成其他类型则需要遍历树中`A`到`C`的所有路径。

```text
A* p = new C;
C* pC = dynamic_cast<C*> p;  // 成功，效率很高，仅一次比较
B* pB = dynamic_cast<B*> p;  // 成功，需要遍历树中A到C的路径，直到找到B
X* pX = dynamic_cast<X*> p;  // 成功，需要遍历树种A到C的路径，直到找到X
D* pD = dynamic_cast<D*> p;  // 失败，需要遍历树中A到C的路径，最后没找到，返回nullptr
P* pP = dynamic_cast<P*> p;  // 失败，P非多态类型
```

![img](./LLVM-RTTI.assets/v2-22085f4a00a46235e38b3294cf35ace5_720w.webp)

其实除了向上向下转换还有一种侧向转换sidecast，比如说从`B*`转换到`X*`，同样会对继承关系进行遍历确定是否有唯一的X可供转换，在此就不作详细展开了。

## 2. C++标准RTTI的局限性[[4\]](https://zhuanlan.zhihu.com/p/620724283#ref_4)

主要分为两方面来讨论，分别是空间和时间上的开销。

### 空间

首先是开启RTTI后，编译器会为每个具有虚函数表的类都储存RTTI信息，即使很多时候我们只是想在其中一两个类中使用这些信息，我们为那些我们不使用的特性付出了空间成本，不符合zero overhead原则。在此之外，对于我们真的想获取其类型元信息的类型，我们大多数时候也并不需要完整的`type_info`结构体，而只需要其中关于类型的一小部分信息而已。

LLVM项目用`-fno-rtti`选项关闭RTTI特性后，可执行文件的体积有5-10%的减少。

### 性能

`dynamic_cast`在很多情况下需要动态遍历继承树，并且一条条比对`type_info`中的类型元信息，在有的编译器中该比对被实现为字符串比较，效率更为低下。如果`dynamic_cast`使用得较多，则性能开销不小。

最后该RTTI系统基于虚函数表实现，对于没有虚函数表的类型，则根本无法提供相关功能。

## 3. LLVM RTTI

LLVM中需要很多运行时类型检查，优化器的不少部分都需要做类型检查和动态类型转换，如此大量地使用RTTI带来的性能开销是很恐怖的。所以LLVM实现了一套更为简洁、表现力更强的RTTI机制[[5\]](https://zhuanlan.zhihu.com/p/620724283#ref_5)，这套机制的开销更小，且不为不使用的部分支付性能或空间，随之而来的缺点就是需要用户侵入式地在类型实现中定义指定的函数和变量。

### 3.1 使用方法

上面提到，要使用LLVM的RTTI系统，就需要用户在类的类型中定义好相关函数和变量，不过定义好后，其使用时非常简洁的。以下分为两个方面讲解具体使用方法。

### 3.1.1 如何使用已经支持LLVM RTTI的类型

LLVM RTTI主要提供了以下三个模板供使用

- `isa<>`
  `isa<>`根据指针或引用是否指向特定类的实例返回`true`或`false`。例如

```cpp
Base* p1 = new Derived;
Base& p2 = new Base;
isa<Derived>(p1);  // returns true
isa<Derived>(p2);  // return false
```

- `cast<>`
  `cast<>`运算符是一个“检查转换”操作。它将指针或引用从基类转换为派生类，如果它实际上不是正确类型的实例，则会导致断言失败。如果您有一些信息使您相信某些东西属于正确的类型，则应该使用这种方法。示例如下（以下仅作示例，如果你要用isa和cast组合完成是否是特性类型的检查，请使用`dyn_cast`）：

```cpp
static bool isLoopInvariant(const Value *V, const Loop *L) {
  if (isa<Constant>(V) || isa<Argument>(V) || isa<GlobalValue>(V))
    return true;

  // Otherwise, it must be an instruction...
  return !L->contains(cast<Instruction>(V)->getParent());
}
```

- `dyn_cast<>`
  `dyn_cast<>`运算符是一个“检查转换”操作。它检查操作数是否属于指定类型，如果是，则返回指向它的指针（此运算符不适用于引用）。如果操作数的类型不正确，则返回空指针。因此，这与 C++ 中的`dynamic_cast<>`运算符非常相似，并且应该在相同的情况下使用。通常，`dyn_cast<>`运算符用在`if`语句或其他一些流程控制语句中，如下所示：

```cpp
if (auto *AI = dyn_cast<AllocationInst>(Val)) {
  // ...
}
```

以上三个指针还提供了`xxx_if_present`版本，区别仅仅是可以接受空指针并返回`false`，在此就不进行详细展开。

### 3.2.1 如何使类型支持LLVM RTTI[[6\]](https://zhuanlan.zhihu.com/p/620724283#ref_6)

参考[[6\]](https://zhuanlan.zhihu.com/p/620724283#ref_6)中有更加具体的讲解，这里仅仅给出前后的代码修改对比，作简单讲解，要想深入了解具体细节需要自己阅读原文。

对于如下的继承关系：

```cpp
class Shape {
  public:
    Shape() {}
    virtual double computeArea() = 0;
};

class Square : public Shape {
    double SideLength;

  public:
    Square(double S) : SideLength(S) {}
    double computeArea() override;
};

class Circle : public Shape {
    double Radius;

  public:
    Circle(double R) : Radius(R) {}
    double computeArea() override;
};

class SpecialSquare : public Square {};
class OtherSpecialSquare : public Square {};
class SomeWhatSpecialSquare : public Square {};
```

要支持LLVM RTTI，修改上述代码如下：

```cpp
class Shape {
  public:
    /// Discriminator for LLVM-style RTTI (dyn_cast<> et al.)
    enum ShapeKind {
        SK_Square,
        SK_SpecialSquare,
        SK_OtherSpecialSquare,
        SK_SomeWhatSpecialSquare,
        SK_LastSuqare,
        SK_Circle,
    };

  private:
    const ShapeKind Kind;

  public:
    ShapeKind getKind() const { return Kind; }

  public:
    Shape(ShapeKind K) : Kind(K) {}
    virtual double computeArea() = 0;
};

class Square : public Shape {
    double SideLength;

  public:
    Square(double S) : Shape(SK_Square), SideLength(S) {}
    double computeArea() override { return SideLength * SideLength; }

    static bool classof(const Shape *S) {
        return S->getKind() >= SK_Square && S->getKind() <= SK_LastSuqare;
    }
};

class Circle : public Shape {
    double Radius;

  public:
    Circle(double R) : Shape(SK_Circle), Radius(R) {}
    double computeArea() override { return 3.14 * Radius * Radius; }

    static bool classof(const Shape *S) { return S->getKind() == SK_Circle; }
};

class SpecialSquare : public Square {};
class OtherSpecialSquare : public Square {};
class SomeWhatSpecialSquare : public Square {};
```

阅读以上代码会发现其实实现非常简单，

- 只需要在基类中维护一个`Kind`信息，然后每个子类构造时赋予相应的`Kind`
- 即使有[多重继承](https://zhida.zhihu.com/search?q=多重继承&zhida_source=entity&is_preview=1)，也可以通过合理排布枚举量的顺序达到常数时间内判断类型信息的效果
- 每个类还实现了静态的`classof`函数，供具体`isa`和`cast`等的实现使用

值得注意的是，基类`Shape`中并没有实现`classof`函数，因为其是一个抽象类，永远不会有该类的示例，也就不需要实现相应的`classof`函数，枚举量中也并没有相应的`SK_Shape`类型。

### 3.2 超简单实现

LLVM中的实现要全面得多，可以处理引用、指针、数组等各种各样的情况，但这里主要为了展现原理，给出一个最最简单的实现，方便读者理解。如有兴趣可以去阅读LLVM的实现[[7\]](https://zhuanlan.zhihu.com/p/620724283#ref_7)，不过以下实现已经足够理解其本质原理。

```cpp
template <typename To, typename From> inline bool isa(const From *Val) {
    return To::classof(Val);
}

template <typename To, typename From> inline To *cast(From *Val) {
    assert(isa<To>(Val) && "Invalid cast!");
    return static_cast<To *>(Val);
}

template <typename To, typename From> inline To *dyn_cast(From *Val) {
    return isa<To>(Val) ? cast<To>(Val) : nullptr;
}
```

同时给出一个简单的使用示例：

```cpp
int main() {
    Shape *pShape = new Square(2);
    if (isa<Square>(pShape)) {
        std::cout << "S is Square with area = " << pShape->computeArea()
                  << std::endl;
    }
    if (Square *pSquare = cast<Square>(pShape)) {
        std::cout << "cast succeed, S is Square with area = "
                  << pSquare->computeArea() << std::endl;
    }
    if (Circle *Cir = dyn_cast<Circle>(pShape)) {
        std::cout << "This won't happen" << std::endl;
    }
    delete cast<Square>(pShape);

    pShape = new Circle(2);
    if (isa<Square>(pShape)) {
        std::cout << "This won't happen" << std::endl;
    }
    if (Circle *Cir = dyn_cast<Circle>(pShape)) {
        std::cout << "Cir is Circle with area = " << Cir->computeArea()
                  << std::endl;
    }
    if (Square *pSquare = cast<Square>(pShape)) {
        std::cout << "Assertion will fail" << std::endl;
    }
}
```

其在Clang14编译器下编译运行的结果为：

```text
S is Square with area = 4
cast succeed, S is Square with area = 4
Cir is Circle with area = 12.56
a.out: ./SimpleCasting.h:11: To *cast(From *) [To = Square, From = Shape]: Assertion `isa<To>(Val) && "Invalid cast!"' failed.
[1]    23878 IOT instruction  ./a.out
```

## 4. 总结

下面给出LLVM RTTI和C++标准RTTI的对比总结：

|                |                                                              |                                                              |
| -------------- | ------------------------------------------------------------ | ------------------------------------------------------------ |
| 对比项目       | C++ RTTI                                                     | LLVM RTTI                                                    |
| 可执行文件体积 | -                                                            | 小5%-10%                                                     |
| 性能开销       | 向上转换等同于静态转换，向下/侧向转换需要遍历继承树，且需要对比type_info信息 | 均只需要一次判断，非常快                                     |
| 支持范围       | 仅有虚函数表的类型可用                                       | 任何类型均可使用                                             |
| 类型信息       | 类型名字符串                                                 | 仅有类型自身类别信息                                         |
| 用户易用性     | 非常容易使用，无侵入性代码                                   | 需要用户在类中维护类型枚举量，每次加入新的继承类型都需要修改基类，并在新的类型中实现相关函数。麻烦且容易出错。 |

总的来说，LLVM RTTI相比C++自身的RTTI更加精巧，仅仅维护需要使用的信息快速高效地完成所需功能——大量类型转换，同时还额外获得了空间上的优势和对于非多态类型的支持，但是天下没有免费的午餐，代价就是代码维护者需要花更多的心思去维护类型的信息并手动实现部分函数。

最后提醒一点，如果项目中需要大量使用`dynamic_cast`等功能，这时候要做的第一件事不是寻找更加高效的RTTI实现，而是应该思考项目设计是否合理，大部分情况下这都是设计上的失误导致的。如果在确认设计后仍然认为需要大量动态转型，这时再考虑自行实现RTTI机制。

## 参考

1. [^](https://zhuanlan.zhihu.com/p/620724283#ref_1_0)C++对象模型之RTTI的实现原理 https://zhuanlan.zhihu.com/p/267794224
2. [^](https://zhuanlan.zhihu.com/p/620724283#ref_2_0)C++：从技术实现角度聊聊RTTI https://heapdump.cn/article/5109918
3. [^](https://zhuanlan.zhihu.com/p/620724283#ref_3_0)C++ GCC 对象模型 dynamic_cast实现 (Part 2) https://zhuanlan.zhihu.com/p/194431590
4. [^](https://zhuanlan.zhihu.com/p/620724283#ref_4_0)What can make C++ RTTI undesirable to use? https://stackoverflow.com/questions/5134975/what-can-make-c-rtti-undesirable-to-use
5. [^](https://zhuanlan.zhihu.com/p/620724283#ref_5_0)The isa<>, cast<> and dyn_cast<> templates https://llvm.org/docs/ProgrammersManual.html#isa
6. ^[a](https://zhuanlan.zhihu.com/p/620724283#ref_6_0)[b](https://zhuanlan.zhihu.com/p/620724283#ref_6_1)How to set up LLVM-style RTTI for your class hierarchy https://llvm.org/docs/HowToSetUpLLVMStyleRTTI.html
7. [^](https://zhuanlan.zhihu.com/p/620724283#ref_7_0)LLVM RTTI Source Code https://llvm.org/doxygen/Casting_8h_source.html