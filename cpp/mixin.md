# Mixin

作者：jiannanya
链接：https://www.zhihu.com/question/646752903/answer/3422104625
来源：知乎
著作权归作者所有。商业转载请联系作者获得授权，非商业转载请注明出处。



在C++中，Mixin模式是一种代码复用技术，它允许将一组功能（通常封装在一个类或结构体中）混入到另一个类或结构体中，从而创建出具有这些功能的新类型。Mixin模式不是C++语言本身直接支持的模式，而是通过一些设计技巧和语言特性（如模板、继承、组合等）来实现的。

Mixin模式有助于避免[类继承](https://zhida.zhihu.com/search?q=类继承&zhida_source=entity&is_preview=1)中的一些问题，比如多重继承带来的复杂性和脆弱性，同时提供了一种更为灵活的方式来复用和组合功能。

在C++中实现Mixin模式，通常可以通过以下步骤进行：

1. **定义Mixin类**：创建一个或多个Mixin类，每个类封装了一组特定的功能。这些类通常设计为模板类，以便能够与其他类型一起使用。
2. **混入功能**：通过继承或组合的方式，将Mixin类的功能混入到目标类中。这可以通过继承Mixin类或使用Mixin类作为成员变量来实现。
3. **使用混入的功能**：在目标类中，可以直接调用Mixin类提供的[成员函数](https://zhida.zhihu.com/search?q=成员函数&zhida_source=entity&is_preview=1)或使用其成员变量。

下面是一个简单的示例，展示了如何在C++中使用Mixin模式来复用代码：

```cpp
#include <iostream>  
#include <string>  
  
// Mixin类：提供打印功能  
template <typename T>  
class Printable {  
public:  
    void print() const {  
        std::cout << "Printable content: " << static_cast<const T&>(*this).getContent() << std::endl;  
    }  
};  
  
// 目标类：混入Printable功能  
class MyData : public Printable<MyData> {  
private:  
    std::string content;  
  
public:  
    MyData(const std::string& c) : content(c) {}  
  
    // 实现Printable所需的getContent函数  
    const std::string& getContent() const {  
        return content;  
    }  
};  
  
int main() {  
    MyData data("Hello, Mixin!");  
    data.print();  // 输出：Printable content: Hello, Mixin!  
    return 0;  
}
```

在这个示例中，`Printable`是一个Mixin类，它提供了一个`print`函数来打印内容。`MyData`类通过继承`Printable<MyData>`混入了打印功能。注意，`Printable`是一个模板类，它接受一个类型参数`T`，这个类型参数用于在`print`函数中调用`getContent`函数。`MyData`类提供了`getContent`函数的实现，以便`Printable`能够访问其内容。

通过这种方式，我们可以将不同的功能（如打印、序列化、验证等）封装成Mixin类，并在需要时将它们混入到目标类中，从而实现代码的复用和灵活组合。

**注意：Mixin模式不一定要用继承实现**

在C++中，Mixin通常指的是一种代码复用模式，允许你将功能组合或混入到不同的类中以创建新的类型。这与多重继承不同，因为它旨在提供一种更模块化、更灵活的方式来复用代码，而不是通过继承一个固定的类层次结构。

如果我们尝试将Mixin和智能指针结合起来讨论，一个可能的场景是，在Mixin模式中，你可能需要使用智能指针来管理资源或确保对象生命周期的正确性。例如，如果你的Mixin类包含动态分配的资源，那么使用智能指针而不是原始指针可以确保这些资源在不再需要时得到正确释放。

下面是一个简化的例子，展示了如何在Mixin模式中使用智能指针：

```cpp
#include <iostream>  
#include <memory> // 引入智能指针相关的头文件  
  
// Mixin类：提供资源管理的功能  
class ResourceManager {  
public:  
    void acquireResource() {  
        std::cout << "Acquiring resource...\n";  
        // 这里应该是实际的资源获取逻辑  
    }  
  
    void releaseResource() {  
        std::cout << "Releasing resource...\n";  
        // 这里应该是实际的资源释放逻辑  
    }  
};  
  
// 使用智能指针管理Mixin资源的类  
class MyClass {  
private:  
    std::unique_ptr<ResourceManager> resourceManager; // 使用unique_ptr管理资源  
  
public:  
    MyClass() : resourceManager(std::make_unique<ResourceManager>()) {} // 初始化时创建ResourceManager对象  
  
    // 混入的功能：获取和释放资源  
    void acquireResource() {  
        resourceManager->acquireResource();  
    }  
  
    void releaseResource() {  
        resourceManager->releaseResource();  
    }  
};  
  
int main() {  
    MyClass myObject;  
    myObject.acquireResource(); // 输出：Acquiring resource...  
    myObject.releaseResource(); // 输出：Releasing resource...  
    // 当myObject离开作用域时，resourceManager会被自动删除，因为它是一个unique_ptr  
    return 0;  
}
```

在这个例子中，`ResourceManager`是一个Mixin类，它提供了资源管理的功能。`MyClass`则通过`std::unique_ptr`智能指针来管理`ResourceManager`对象的生命周期。当`MyClass`的对象不再需要时，`std::unique_ptr`会自动释放它所指向的`ResourceManager`对象，从而避免了[内存泄漏](https://zhida.zhihu.com/search?q=内存泄漏&zhida_source=entity&is_preview=1)和其他资源管理问题。

请注意，这只是一个简单的示例，实际使用Mixin模式和智能指针时，你可能需要处理更复杂的场景，如多线程环境下的资源管理、自定义删除器等。

# Mixin Pattern

写法:

```c++
template<class T>
class Base : public T {
    ......
};
```

问题:

有如下的BasePrint类用于提供打印功能：

```c++
class BasePrint {
public :
    virtual void print();
};
```

实际应用中我们对这个基类进行继承扩展，实现定制不同版本的打印功能，然后自己的实际类中继承对应的打印类，选用自己需要的打印版本。

```c++
class DerivePrint1 : public BasePrint
{
public :
    // 这里为了方便就随便输出了，实际应用中比如这个print可以封装为带色彩输出或者格式化输出
    virtual void myprint() {
        cout<<"Hello World 1!"<<endl;
    }
    virtual void print() {
        myprint();
    }
};

class myClass : public DerivePrint1{
};

int main()
{
    myClass my;
    my.print();
}
```

那么如果此时又新增了另一种打印，我们的类中想要同时使用这两种打印呢？

```c++
class DerivePrint1 : public BasePrint
{
public :
    virtual void myprint() {
        cout<<"Hello World 1!"<<endl;
    }
    virtual void print() {
        myprint();
    }
};

class DerivePrint2 : public BasePrint
{
public :
    virtual void myprint() {
        cout<<"Hello World 2!"<<endl;
    }
    virtual void print() {
        myprint();
    }
};

class myClass:public DerivePrint1,public DerivePrint2{
};

int main()
{
    myClass my;
    // error，触发了多重继承的问题
    my.print();
}
```

可以看到，多重继承存在引发[Diamond-Problem](https://stackoverflow.com/questions/2064880/diamond-problem)的风险。于是C++ 引入了虚继承，但虚继承在某些场景下也是有一定弊端的，为此C++ 作者发明了[Template Parameters as Base Classes](https://books.google.com.hk/books?id=PSUNAAAAQBAJ&lpg=PA767&ots=DrtrIigY4J&dq=27.4. Template Parameters as Base Classes&hl=zh-CN&pg=PA767#v=onepage&q&f=false)，这便是C++ 中实现mixin的方式。

回到上面的问题，以mixin的方式来实现的话：

```c++
template <typename T>
class DerivePrint1 : public T
{
public :
    void print() {
        cout<<"Hello World 1!"<<endl;
        T::print();
    }
};

template <typename T>
class DerivePrint2 : public T
{
public :
    void print() {
        cout<<"Hello World 2!"<<endl;
        T::print();
    }
};

class myClass{
public:
    void print(){
        cout<<"myClass"<<endl;
    }
};


int main()
{
    myClass my1;
    my1.print();
    cout<<"-----------------------"<<endl;
    DerivePrint1<myClass> my2;
    my2.print();
     cout<<"-----------------------"<<endl;
    DerivePrint2<myClass> my3;
    my3.print();
     cout<<"-----------------------"<<endl;
    DerivePrint2<DerivePrint1<myClass>> my4;
    my4.print();
     cout<<"-----------------------"<<endl;
    DerivePrint1<DerivePrint2<myClass>> my5;
    my5.print();
}

其输出结果如下：
myClass
-----------------------
Hello World 1!
myClass
-----------------------
Hello World 2!
myClass
-----------------------
Hello World 2!
Hello World 1!
myClass
-----------------------
Hello World 1!
Hello World 2!
myClass
```

可以看到，通过**parameterized inheritance**的方式实现mixin的设计，可以将多个独立的类综合起来，其中每个类就像是一个功能片段一样，且避免了多继承的风险，同时语义清晰，且允许线性化继承层级。



mixin是一种设计思想，用于将相互独立的类的功能组合在一起，以一种安全的方式来模拟多重继承。而c++中mixin的实现通常采用Template Parameters as Base Classes的方法，既实现了多个不交叉的类的功能组合，又避免了多重继承的问题。