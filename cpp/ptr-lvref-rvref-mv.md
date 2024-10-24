# 指针vs引用

指针和引用在binary层面是等价的, 都是描述一块存储地址的内存. 在编译成binary时, 对引用的所有使用都会自动取地址, 不需要程序员手动写`*`, 相比指针用起来比较方便. 然而这就是它们本质的区别吗? 设计引用的初衷是什么?

 参考:https://blog.csdn.net/a3192048/article/details/84621775

根据<<c++的设计与演化>>一书中所描述的, 设计引用的初衷是为了解决运算符重载中的问题, 考虑这个例子:

```cpp
a = b - c;
```

b和c本质会作为参数传给运算符重载函数. C语言对所有参数都采用值传递, 为了兼容C, C++也要采用相同的设计, 不能像Java那样一切皆引用. 为了提高传输效率, 我们需要对对象取地址, 通过指针传递参数:

```cpp
a = &b - &c;
```

很明显, 上述语句是存在二义性的, 编译器不知道我们想做的是指针地址的减法运算, 还是两个对象之间的减法运算. 因此设计出了引用的概念.

引用的另一个作用就是相对指针更加安全. 引用在编译层面保证了:

* 不存在空引用, 保证不操作空指针.
* 必须初始化, 保证不存在野指针.
* 一个引用永远指向他初始化的那个对象, 保证指针指向的地址不变.

# 在binary视角上理解变量,指针与引用

## 变量

变量可以理解为一块内存, 内存中存储着变量的数值, 变量的地址就是内存地址.

有些编译器会将变量全部视为寄存器, 这也是一种理解方式. 不过我更喜欢将变量视为内存. 寄存器在这个视角下承担以下功能:

* 辅助内存寻址(寄存器寻址, 间接寻址,...). 
* 充当两个内存间数据传输的介质(像x86-64 ISA不支持内存之间的直接数据传输, 需要寄存器中转).
* 加速. 一个操作序列可能会频繁访存, 可以先将数据加载到寄存器中, 在寄存器中完成一系列数据操作后再一次性写回内存. 注意这种优化会使源码中一些变量丧失原本的意义, 影响我们的理解, 推荐在调试阶段关闭编译优化.

可以看到, 在没有编译优化的情况下, 我们甚至可以认为寄存器是透明的, 只是一种访存/数据传输机制. 

总之, 变量本质就是内存地址, 各种变量操作语句本质就是描述内存上的数据变换.

## 左值(实名变量)和右值(匿名变量)

左值和右值都是变量, 都会存储在一块内存中, 也都有内存地址. 只不过右值在语义层面不允许取地址罢了.

## 指针和引用

指针和引用也是变量, 自然有一块相应的内存, 只不过这个内存地址存放的数据是另一个变量的内存地址. 二者在binary层面无本质区别.

注意使用引用时编译器会自动为我们解引用, 具体表现为打印引用时打印的就是目标变量的值, 而非地址, 另外对引用取地址打印的也是目标变量的内存地址, 而非这个引用的内存地址.

## 左值引用和右值引用

左值引用和右值引用都是引用变量, 都对应一块存放地址的内存. 二者在binary层面无本质区别.

# 构造函数的binary表示

构造函数有一个隐式的参数, 用来描述存放对象的地址. 调用构造函数前应该首先指定一块内存地址接收新建对象, 然后将这块内存的地址通过rdi寄存器传给构造函数(这个地址一般也是对象中第一个成员变量的地址). 构造函数不会通过rax寄存器返回对象地址, 而是直接将对象写入rdi寄存器传入的内存中. 

另外, 在赋值/传参/返回/作用域结束等位置, 编译器会为我们自动生成默认/拷贝/移动构造函数以及析构函数的调用.

# 右值(临时变量,匿名变量)引用问题

## 右值初始化非const引用

C++中是禁止用右值初始化非const引用的. 我们来看一个反例.

```c++
void inc(int &x) { x++; }
int main() {
    // int x = 1;
    double x = 1;
    inc(x);
    return x;
}
```

上述代码中, 我们使用`int x = 1`是没问题的. 然而, 如果使用`double x = 1`, 在调用`inc(x)`时会生成一个由类型转换产生的临时变量, 此时函数`inc(int&)`实质是增加的这个临时变量, 修改结果不会反映到`x`中.

可以看到允许右值初始化非const引用是非常危险的, 因此C++ Release 2.0禁止了这一行为.

## 右值初始化const引用

不过, 右值初始化const引用是安全的. 因为有了const修饰, 我们只能读这个临时变量, 不能修改它. 这种行为是安全的, 也方便了开发者的使用. 考虑这个例子:

```cpp
void test(const std::string &s) {}
int main() {
    // std::string s = "hello world";
    // test(s);
    test("hello world");
    return 0;
}
```

如果不允许右值初始化const引用, 我们每次都要新建一个字符串变量, 写起来就比较麻烦了.

## 右值引用与移动语义

右值引用的引入主要是为了解决移动语义问题, 所以我们在在介绍右值引用前, 首先要明白为什么需要移动语义.

## 为何需要移动语义

来看看c++11前的复制过程:

```cpp
vector<string> vstr;
// build up a vector of 20,000 strings, each of 1000 characters
...
vector<string> vstr_copy(vstr); // make vstr_copy1 a copy of vstr
```

很明显, 20000*1000=20000000个字符全部从`vstr`复制到了`vstr_copy`中. 不过有些情况下我们确实需要将数据全部拷贝一份, 这种深拷贝做法还是合理的.

不过这种深拷贝的做法在所有情况下都是合理的吗? 答案是否定的. 例如:

```cpp
vector<string> test() {
    vector<string> temp;
	// build up a vector of 20,000 strings, each of 1000 characters
	...
    return temp;
}

vector<string> vstr_copy2(test());
```

这种情况下, 很明显`temp`只是临时存储一下我们的20000000个字符, 真正的管理者是`vstr_copy2`. 然而, 编译器却会将`temp`中的内容全部再拷贝一次到`vstr_copy2`中(迟钝的编译器甚至还会先将`temp`中的内容复制给一个临时变量, 然后再从这个临时变量复制给`vstr_copy2`), 十分影响效率.

如果编译器能直接将数据的所有权从`temp`转移到`vstr_copy2`, 效率肯定是更高的. 这就是移动语义.

可以理解为在文件剪切时, 文件的底层存储位置不变, 只是改变了记录.

那么c++11前有办法能实现移动语义吗?

首先用指针肯定是能实现的, 一切对象都手动new, 手动delete, 只不过这样相当于退化成C, 我们不考虑.

那么用左值引用或者const&可以实现移动语义吗?

从原理出发. 对于左值引用而言, 其实质是变量使用权的借用, 相当于创建文件的快捷方式. 这里我们要租借的变量是一个临时变量`test()`. 且不说C++不允许用临时变量初始化左值引用, 就算能租借成功, 临时对象的自动析构也会释放相应的内存, 这样`vstr_copy2`存储的将是一块废弃的内存. 总之, 左值引用无法实现移动语义.

那么const&这种右值引用可以实现移动语义吗? 也是不行的. 为了实现移动语义, 我们确实需要右值引用, 但需要的是可对内容进行修改的右值引用. 在这个例子中, 我们需要获取`test()`返回的vector临时对象, 然后**将其内部的数组指针置为null以放弃该内存的所有权**, 这样才能避免临时对象的析构带来的内存释放问题, `vstr_copy2`才能真正的独占这块内存. 然而, const限制了我们对临时变量的内容进行修改. 因此const&也无法实现移动语义.

综上, 我们需要一个可以对右值进行修改的引用, 这就是c++11新增的右值引用&&.

举例:

```cpp
#include <iostream>

class A {
public:
    int n;
    int *arr;
    A(int n, int base) : n(n) {
        arr = new int[n];
        for (int i = 0; i < n; i++) {
            arr[i] = base + i;
        }
        std::cout << "default constructor: ";
        showData();
    }
    ~A() {
        std::cout << "destructor: ";
        showData();
        n = 0;
        delete arr;
    }
    A(A& a) : n(a.n) {
        arr = new int[n];
        for (int i = 0; i < n; i++) {
            arr[i] = a.arr[i];
        }
        std::cout << "copy constructor: ";
        showData();
    }
    A(A&& a) : n(a.n) {
        arr = a.arr;
        a.arr = nullptr;
        std::cout << "move constructor: ";
        showData();
    }
    void showData() {
        if (arr == nullptr) {
            std::cout << n << ", nullptr" << std::endl;
            return;
        }
        std::cout << n << ", " << (void*)arr << ": ";
        for (int i = 0; i < n; i++) {
            std::cout << arr[i] << ", ";
        }
        std::cout << std::endl;
    }
};

int main() {
    A a(3, 1);
    A a1(a);
    A a2(A(3, 2));
    return 0;
}
```

关闭编译优化带来的影响:

```cpp
g++ -std=c++14 -g -O0 -fno-elide-constructors main.cpp
```

输出:

```shell
default constructor: 3, 0x55f4762a4eb0: 1, 2, 3, 
copy constructor: 3, 0x55f4762a52e0: 1, 2, 3, 
default constructor: 3, 0x55f4762a5300: 2, 3, 4, 
move constructor: 3, 0x55f4762a5300: 2, 3, 4, 
destructor: 3, nullptr
destructor: 3, 0x55f4762a5300: 2, 3, 4, 
destructor: 3, 0x55f4762a52e0: 1, 2, 3, 
destructor: 3, 0x55f4762a4eb0: 1, 2, 3,
```

## 左值引用转右值引用

考虑上述代码中的`A a1(a)`. 这里我们已经可以保证`a`在后续代码中不会被使用, 理论上转移控制权是最高效的. 然而, 编译器很难确定我们后续代码中是否使用了`a`, 也就不敢自动帮我们做优化. 

这时就需要一个机制强制将一个左值转换成右值, 实现变量所有权的转移. 我们当然可以使用`static_cast<A&&>`进行强制类型转换. 不过更优雅的方式是使用cpp提供的`std::move`. 这两种方式原理是一致的.

> 一定要注意后续代码中不要再使用`a`

## copy elision

c++中的copy elision是指一种编译器行为，即尽可能的消除不必要的拷贝构造和移动构造(当类可移动时则消除移动，不可移动时消除拷贝)，该行为在c++17之前视作编译器的优化，可以通过`-fno-elide-constructors`编译选项禁用掉；在c++17之后成为标准而非优化，即编译器在任何情况下都必须这么做，无法通过编译选项来禁用。

包含以下几种情况：

* RVO (Return Value Optimization)：当return之后的表达式为纯右值时，直接在函数调用的位置去构造返回值对象，省略掉拷贝/移动。 
* NRVO (Named Return Value Optimization)：当return之后的表达式为左值时，且该左值为该函数栈上的变量，则直接在函数调用的位置去构造返回值对象，省略掉拷贝/移动。
* 初始化变量时，当初始化表达式为纯右值，则执行copy elision。

> 注意: 就算拷贝构造函数和移动构造函数有副作用, 编译器仍会将它们消除.

例子:

```cpp
#include <iostream>

class MyClass {
public:
    int t1;
    int t2;
    int t3;
    int x;
    MyClass(int t1, int t2, int t3, int x) : x(x)
    {
        std::cout << "default constructor\n";
    }
    MyClass(MyClass& a) : x(a.x)
    {
        std::cout << "copy constructor\n";
    }
    MyClass(MyClass&& a) : x(a.x)
    {
        std::cout << "move constructor\n";
    }
    ~MyClass() {
        x = 0;
        std::cout << "destructor" << std::endl;
    }
};

MyClass test() {
    int x = 1;
    return MyClass(1, 1, 1, x);
}

int main()
{
    MyClass a(test());
    int x = a.x;
    return x;
}
```

不开启优化:

```
g++ -std=c++14 -g -O0 -fno-elide-constructors main.cpp
```

输出:

```shell
default constructor
move constructor
destructor
move constructor
destructor
destructor
```

binary:

首先要注意的是, rdi中的地址被保存到了`-0x48(%rbp)`. 也就是说`test()`最终应该将返回的对象的地址写入`-0x48(%rbp)`中.

```asm
    11f6:	48 89 7d b8          	mov    %rdi,-0x48(%rbp)
```

接下来`test()`将变量`x`存入`-0x34(%rbp)`中:

```asm
/home/hzy/hzydata/projects/test/cpptest/test.cpp:28
    int x = 1;
    1209:	c7 45 cc 01 00 00 00 	movl   $0x1,-0x34(%rbp)
```

接下来调用构造函数`MyClass(int)`, 将新建对象写入`-0x30(%rbp)`中:

```asm
/home/hzy/hzydata/projects/test/cpptest/test.cpp:29
    return MyClass(1, 1, 1, x);
    1210:	8b 55 cc             	mov    -0x34(%rbp),%edx
    1213:	48 8d 45 d0          	lea    -0x30(%rbp),%rax
    1217:	41 89 d0             	mov    %edx,%r8d
    121a:	b9 01 00 00 00       	mov    $0x1,%ecx
    121f:	ba 01 00 00 00       	mov    $0x1,%edx
    1224:	be 01 00 00 00       	mov    $0x1,%esi
    1229:	48 89 c7             	mov    %rax,%rdi
    122c:	e8 67 01 00 00       	call   1398 <_ZN7MyClassC1Eiiii>
```

接着调用移动构造函数`MyClass(MyClass&&)`, 将新建对象写入`-0x48(%rbp)`中, 这个移动构造函数是编译器自动创建的:

```asm
    1231:	48 8d 55 d0          	lea    -0x30(%rbp),%rdx
    1235:	48 8b 45 b8          	mov    -0x48(%rbp),%rax
    1239:	48 89 d6             	mov    %rdx,%rsi
    123c:	48 89 c7             	mov    %rax,%rdi
    123f:	e8 98 01 00 00       	call   13dc <_ZN7MyClassC1EOS_>
```

`test()`的最后会调用析构函数, 析构`-0x30(%rbp)`指向的对象, 这个析构函数也是编译器自动创建的:

```asm
    1245:	48 8d 45 d0          	lea    -0x30(%rbp),%rax
    1249:	48 89 c7             	mov    %rax,%rdi
    124c:	e8 c9 01 00 00       	call   141a <_ZN7MyClassD1Ev>
```

`main()`中, 会将`test()`返回的this指针保存在`-0x30(%rbp)`中:

```asm
/home/hzy/hzydata/projects/test/cpptest/test.cpp:34
    MyClass a(test());
    12ac:	48 8d 45 d0          	lea    -0x30(%rbp),%rax
    12b0:	48 89 c7             	mov    %rax,%rdi
    12b3:	e8 31 ff ff ff       	call   11e9 <_Z4testv>
```

接着调用移动构造函数, 将新建对象写入`-0x40(%rbp)`中:

```asm
    12b8:	48 8d 55 d0          	lea    -0x30(%rbp),%rdx
    12bc:	48 8d 45 c0          	lea    -0x40(%rbp),%rax
    12c0:	48 89 d6             	mov    %rdx,%rsi
    12c3:	48 89 c7             	mov    %rax,%rdi
    12c6:	e8 11 01 00 00       	call   13dc <_ZN7MyClassC1EOS_>
```

接着析构`-0x30(%rbp)`所指向的对象:

```asm
    12cb:	48 8d 45 d0          	lea    -0x30(%rbp),%rax
    12cf:	48 89 c7             	mov    %rax,%rdi
    12d2:	e8 43 01 00 00       	call   141a <_ZN7MyClassD1Ev>
```

最后将`a.x`保存至`x`. `x`前面还有`t1`, `t2`, `t3`3个成员变量, 因此应该偏移`0x1c`:

```asm
/home/hzy/hzydata/projects/test/cpptest/test.cpp:35
    int x = a.x;
    12d7:	8b 45 cc             	mov    -0x34(%rbp),%eax
    12da:	89 45 bc             	mov    %eax,-0x44(%rbp)
```

后面其实还有一个`-0x40(%rbp)`的析构, 这里就不展示了.

接下来开启优化:

```
g++ -std=c++14 -g test.cpp
```

输出:

```shell
default constructor
destructor
```

binary比较精简, 就直接展示了:

```asm
00000000000011c9 <_Z4testv>:
_Z4testv():
/home/hzy/hzydata/projects/test/cpptest/test.cpp:27
        x = 0;
        std::cout << "destructor" << std::endl;
    }
};

MyClass test() {
    11c9:	f3 0f 1e fa          	endbr64 
    11cd:	55                   	push   %rbp
    11ce:	48 89 e5             	mov    %rsp,%rbp
    11d1:	48 83 ec 20          	sub    $0x20,%rsp
    11d5:	48 89 7d e8          	mov    %rdi,-0x18(%rbp)
/home/hzy/hzydata/projects/test/cpptest/test.cpp:28
    int x = 1;
    11d9:	c7 45 fc 01 00 00 00 	movl   $0x1,-0x4(%rbp)
/home/hzy/hzydata/projects/test/cpptest/test.cpp:29
    return MyClass(1, 1, 1, x);
    11e0:	8b 55 fc             	mov    -0x4(%rbp),%edx
    11e3:	48 8b 45 e8          	mov    -0x18(%rbp),%rax
    11e7:	41 89 d0             	mov    %edx,%r8d
    11ea:	b9 01 00 00 00       	mov    $0x1,%ecx
    11ef:	ba 01 00 00 00       	mov    $0x1,%edx
    11f4:	be 01 00 00 00       	mov    $0x1,%esi
    11f9:	48 89 c7             	mov    %rax,%rdi
    11fc:	e8 cf 00 00 00       	call   12d0 <_ZN7MyClassC1Eiiii>
/home/hzy/hzydata/projects/test/cpptest/test.cpp:30
}
    1201:	48 8b 45 e8          	mov    -0x18(%rbp),%rax
    1205:	c9                   	leave  
    1206:	c3                   	ret    

0000000000001207 <main>:
main():
/home/hzy/hzydata/projects/test/cpptest/test.cpp:33

int main()
{
    1207:	f3 0f 1e fa          	endbr64 
    120b:	55                   	push   %rbp
    120c:	48 89 e5             	mov    %rsp,%rbp
    120f:	53                   	push   %rbx
    1210:	48 83 ec 38          	sub    $0x38,%rsp
    1214:	64 48 8b 04 25 28 00 	mov    %fs:0x28,%rax
    121b:	00 00 
    121d:	48 89 45 e8          	mov    %rax,-0x18(%rbp)
    1221:	31 c0                	xor    %eax,%eax
/home/hzy/hzydata/projects/test/cpptest/test.cpp:34
    MyClass a(test());
    1223:	48 8d 45 d0          	lea    -0x30(%rbp),%rax
    1227:	48 89 c7             	mov    %rax,%rdi
    122a:	e8 9a ff ff ff       	call   11c9 <_Z4testv>
/home/hzy/hzydata/projects/test/cpptest/test.cpp:35
    int x = a.x;
    122f:	8b 45 dc             	mov    -0x24(%rbp),%eax
    1232:	89 45 cc             	mov    %eax,-0x34(%rbp)
/home/hzy/hzydata/projects/test/cpptest/test.cpp:36
    return x;
    1235:	8b 5d cc             	mov    -0x34(%rbp),%ebx
/home/hzy/hzydata/projects/test/cpptest/test.cpp:37
    1238:	48 8d 45 d0          	lea    -0x30(%rbp),%rax
    123c:	48 89 c7             	mov    %rax,%rdi
    123f:	e8 d0 00 00 00       	call   1314 <_ZN7MyClassD1Ev>
    1244:	89 d8                	mov    %ebx,%eax
    1246:	48 8b 55 e8          	mov    -0x18(%rbp),%rdx
    124a:	64 48 2b 14 25 28 00 	sub    %fs:0x28,%rdx
    1251:	00 00 
    1253:	74 05                	je     125a <main+0x53>
    1255:	e8 66 fe ff ff       	call   10c0 <__stack_chk_fail@plt>
    125a:	48 8b 5d f8          	mov    -0x8(%rbp),%rbx
    125e:	c9                   	leave  
    125f:	c3                   	ret    
```

可以看到, `test()`中直接将默认构造函数生成的对象交管给了`main()`, 没有调用析构函数. `main()`中直接接手了这个对象, 没有调用额外的构造函数, 最后析构了这个对象. 这里同时体现了ROV优化以及初始化表达式为纯右值的优化. 至于NROV, 可以将`test()`改写为下述代码自行尝试, 优化效果和上述过程是一样的:

```cpp
MyClass test() {
    MyClass t(1, 1, 1, 1);
    return t;
}
```

以后可以放心返回std::string了, 不会发生拷贝, 再也没有心理压力了.

## &, const&与&&的重载场景

情况1:

```cpp
#include <iostream>

class A {
public:
    int n;
    int *arr;
    A(int n, int base) : n(n) {
        arr = new int[n];
        for (int i = 0; i < n; i++) {
            arr[i] = base + i;
        }
        std::cout << "default constructor: ";
        showData();
    }
    ~A() {
        std::cout << "destructor: ";
        showData();
        n = 0;
        delete arr;
    }
    A(A& a) : n(a.n) {
        arr = new int[n];
        for (int i = 0; i < n; i++) {
            arr[i] = a.arr[i];
        }
        std::cout << "copy constructor: ";
        showData();
    }
    A(const A& a) : n(a.n) {
        arr = new int[n];
        for (int i = 0; i < n; i++) {
            arr[i] = a.arr[i];
        }
        std::cout << "const copy constructor: ";
        showData();
    }
    A(A&& a) : n(a.n) {
        arr = a.arr;
        a.arr = nullptr;
        std::cout << "move constructor: ";
        showData();
    }
    void showData() const {
        if (arr == nullptr) {
            std::cout << n << ", nullptr" << std::endl;
            return;
        }
        std::cout << n << ", " << (void*)arr << ": ";
        for (int i = 0; i < n; i++) {
            std::cout << arr[i] << ", ";
        }
        std::cout << std::endl;
    }
};

int main() {
    A a(3, 1);
    const A ac(3, 2);
    A a1(a); // copy
    A a2(ac); // const copy
    A a3(A(3, 3)); // move
    return 0;
}
```

情况2, 删除move构造函数, `a3`调用const copy构造函数.

情况3, 删除const copy构造函数, `A a2(ac)`报错.

情况4, 删除move和const copy构造函数, `A a2(ac)`和`A a3(A(3, 3))`报错.

情况5, 删除copy构造函数, `A a1(a)`调用const copy构造函数.

到这里大概就明白了, 优先完美匹配, 无法完美匹配时按照如下规则进行类型转换:

* &&->const &
* &->const &

如果无法类型转化则编译报错.

## 引用折叠,万能引用与完美转发

我们来看一个自定义类型对象保存到vector中的例子(注意vector要求拷贝构造函数使用const&). 我们希望有一个函数`addVec`能够将传入的参数保存到vector中, 并且当参数类型为左值引用时使用拷贝语义加入vector,  当参数类型为右值引用时使用移动语义加入vector:

```cpp
class A {
public:
    int n;
    int *arr;
    A(int n, int base) : n(n) {
        arr = new int[n];
        for (int i = 0; i < n; i++) {
            arr[i] = base + i;
        }
        std::cout << "default constructor: ";
        showData();
    }
    ~A() {
        std::cout << "destructor: ";
        showData();
        n = 0;
        delete arr;
    }
    A(const A& a) : n(a.n) {
        arr = new int[n];
        for (int i = 0; i < n; i++) {
            arr[i] = a.arr[i];
        }
        std::cout << "copy constructor: ";
        showData();
    }
    A(A&& a) : n(a.n) {
        arr = a.arr;
        a.arr = nullptr;
        std::cout << "move constructor: ";
        showData();
    }
    void showData() const {
        if (arr == nullptr) {
            std::cout << n << ", nullptr" << std::endl;
            return;
        }
        std::cout << n << ", " << (void*)arr << ": ";
        for (int i = 0; i < n; i++) {
            std::cout << arr[i] << ", ";
        }
        std::cout << std::endl;
    }
};

std::vector<A> vec;

void addVec(A& a) {
    std::cout << "start push &" << std::endl;
    vec.emplace_back(a);
    std::cout << "end push &" << std::endl;
}

void addVec(A&& a) {
    std::cout << "start push &&" << std::endl;
    vec.emplace_back(std::move(a)); // 也可以(A&&), static_case<A&&>, 但不能只写左值a
    std::cout << "end push &&" << std::endl;
}

int main() {
    vec.reserve(10);
    A a(3, 1);
    addVec(a);
    addVec(A(3, 2));
    return 0;
}
```

注意要事先通过`reverse`函数给`vec`分配一些capacity, 不然其动态扩容时会对内部所有对象进行一次拷贝, 干扰我们的调试.

编译:

```shell
g++ -std=c++14 -g -O0 -fno-elide-constructors main.cpp
```

结果:

```shell
default constructor: 3, 0x55c99f014f60: 1, 2, 3, 
start push &
copy constructor: 3, 0x55c99f015390: 1, 2, 3, 
end push &
default constructor: 3, 0x55c99f0153b0: 2, 3, 4, 
start push &&
move constructor: 3, 0x55c99f0153b0: 2, 3, 4, 
end push &&
destructor: 3, nullptr
destructor: 3, 0x55c99f014f60: 1, 2, 3, 
destructor: 3, 0x55c99f015390: 1, 2, 3, 
destructor: 3, 0x55c99f0153b0: 2, 3, 4, 
```

可以看到两个`addVec`分别调用了拷贝构造函数和移动构造函数, 符合我们的预期.

另外, `push_back`和`emplace_back`在像上面这种简单的情况下已经等价, 都会使用移动语义, 不过在某些特殊情况下还是`emplace_back`高效, 可以参考: https://stackoverflow.com/questions/4303513/push-back-vs-emplace-back

我们马上就发现了一个问题, 每次都要写两个版本的`addVec`太麻烦了, 能不能把它们合并呢? 如果能合并, 还有一个问题, 那就是 `addVec(&&)`中我们需要再move一次, 合并后我们怎么根据传入的是左值引用还是右值引用来决定要不要move呢? 这就是接下来要介绍的引用折叠, 万能引用与完美转发. 三者的联系如下:

1. 因为【引用折叠】特性，才有了万能引用。
2. 【完美转发】的特性是借助【万能引用】以及【forward模板函数】来实现。

先来看引用折叠, 所谓折叠, 可以理解为引用的引用. 分为四种情况:

* 左值引用的左值引用 T& &
* 右值引用的左值引用 T& &&
* 左值引用的右值引用 T&& &
* 右值引用的右值引用 T&& &&

当然, 直接在编译器中这么写是会报错的, 那么为什么还要提引用折叠的概念呢? 这就要介绍万能引用的概念.

所谓的万能引用斌更不是C++的新语法特性, 而是利用现有C++语法实现的一个功能. 因为这个功能即能接收左值类型的参数, 又能接收右值类型的参数, 所以叫万能引用. 形式如下:

```cpp
template<typename T>
ReturnType Function(T&& parem)
{
    // 函数功能实现
}
```

接下来，我们看一下为什么上面这个函数能**万能引用**不同类型的参数。

> 这里会用到boost库<boost/type_index.hpp>, 可以打印模板的类型,  方便我们进行调试.
>
> 直接导头文件就好, 不用链接.

```cpp
template <typename T>
void addVec(T&& a) {
    using boost::typeindex::type_id_with_cvr;
    // 利用Boost库打印模板推导出来的 T 类型
    std::cout << "T type：" << type_id_with_cvr<T>().pretty_name() << std::endl;

    // 利用Boost库打印形参的类型
    std::cout << "param type:" << type_id_with_cvr<decltype(a)>().pretty_name() << std::endl;
}
```

结果:

```shell
default constructor: 3, 0x55e971d5df60: 1, 2, 3, 
T type：A&
param type:A&
default constructor: 3, 0x55e971d5e3e0: 2, 3, 4, 
T type：A
param type:A&&
destructor: 3, 0x55e971d5e3e0: 2, 3, 4, 
destructor: 3, 0x55e971d5df60: 1, 2, 3,
```

可以看到万能引用确实可以自动判断引用类型, 原理是什么呢?

当我们调用`addVec(a);`时, T的类型为A&, 参数类型为A& &&, 虽然编译器不允许我们写A& &&, 但内部却可以先构造一个这样的临时类型. 但最终编译器是要生成一个正常的引用的, A& &&对应的正常引用是谁呢? 这就涉及到了引用折叠规则:

```shell
如果任一引用为左值引用，则结果为左值引用。否则（即两个都是右值引用），结果为右值引用。
```

也就是说, A& &&等价于A&. 而对于`addVec(A(3, 2))`而言, 其T的类型为A&&, 参数类型为A&& &&, 折叠后的类型为A&&. 这就是万能引用的原理.

好, 现在来到第二个问题, 如何将这个T&& a类型的参数传给`emplace_back`?

这样:?

```cpp
    std::cout << "start push ==================================================" << std::endl;

    using boost::typeindex::type_id_with_cvr;
    // 利用Boost库打印模板推导出来的 T 类型
    std::cout << "T type：" << type_id_with_cvr<T>().pretty_name() << std::endl;
    // 利用Boost库打印形参的类型
    std::cout << "param type:" << type_id_with_cvr<decltype(a)>().pretty_name() << std::endl;

    vec.emplace_back(static_cast<T>(a));
    std::cout << "end push ==================================================" << std::endl;
```

打印出来的结果并没有调用移动构造函数, 显然是可以的. 因为a本质还是个左值.

解决方法其实很容易想到, 我们不是已经有参数类型T&&了吗? 那么对`a`进行一次强制类型转换就可以了:

```cpp
template <typename T>
void addVec(T&& a) {
    std::cout << "start push ==================================================" << std::endl;

    using boost::typeindex::type_id_with_cvr;
    // 利用Boost库打印模板推导出来的 T 类型
    std::cout << "T type：" << type_id_with_cvr<T>().pretty_name() << std::endl;
    // 利用Boost库打印形参的类型
    std::cout << "param type:" << type_id_with_cvr<decltype(a)>().pretty_name() << std::endl;

    vec.emplace_back(static_cast<T&&>(a));
    std::cout << "end push ==================================================" << std::endl;
}
```

结果:

```shell
default constructor: 3, 0x5599cd493f60: 1, 2, 3, 
start push ==================================================
T type：A&
param type:A&
copy constructor: 3, 0x5599cd4943e0: 1, 2, 3, 
end push ==================================================
default constructor: 3, 0x5599cd494400: 2, 3, 4, 
start push ==================================================
T type：A
param type:A&&
move constructor: 3, 0x5599cd494400: 2, 3, 4, 
end push ==================================================
destructor: 3, nullptr
destructor: 3, 0x5599cd493f60: 1, 2, 3, 
destructor: 3, 0x5599cd4943e0: 1, 2, 3, 
destructor: 3, 0x5599cd494400: 2, 3, 4,
```

这种模式叫做完美转发. 类似move, cpp也为完美转发提供了标准函数forward, 用的时候把T传进去就好了, 不用自己强制转换T&&(其实和move一样, 底层就是一个强制转换...):

```cpp
template <typename T>
void addVec(T&& a) {
    std::cout << "start push ==================================================" << std::endl;

    using boost::typeindex::type_id_with_cvr;
    // 利用Boost库打印模板推导出来的 T 类型
    std::cout << "T type：" << type_id_with_cvr<T>().pretty_name() << std::endl;
    // 利用Boost库打印形参的类型
    std::cout << "param type:" << type_id_with_cvr<decltype(a)>().pretty_name() << std::endl;

    vec.emplace_back(std::forward<T>(a));
    std::cout << "end push ==================================================" << std::endl;
}
```

# 查看/修改g++/clang++默认的cpp标准版本

查看:

```shell
g++ -dM -E -x c++ /dev/null | grep -F __cplusplus
#define __cplusplus 201703L
```

修改:

```shell
g++ -std=c++14 ...
```

clang++同理.

# 移动语义vs指针

当类中的字段过多时, 移动语义的效率也会受影响. 因为移动语义本质上还是要做一次成员变量浅拷贝. 解决方法一是给成员变量套层指针外壳(注意必须是指针外壳, 不能是引用外壳, 因为移动语义需要对象能够放弃所有权, 而引用一旦初始化是不能指向另一个目标的), 不过这样在设计上就十分不美观. 二是用指针传递对象, 通过活用智能指针既可以保证高效又可以保证内存安全. 实际上日常开发中智能指针可能会用得更多一些, 移动语义主要在编写/使用库的时候会用到. 

这个例子也告诉我们移动语义和指针这两个概念都有存在的必要. 谁也无法完全取代另一方.