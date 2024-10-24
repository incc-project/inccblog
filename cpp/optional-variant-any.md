https://blog.csdn.net/alexhu2010q/article/details/117575655

## std::optional

> The class template std::optional manages an optional contained value, i.e. a value that may or may not be present.
> A common use case for optional is the return value of a function that may fail.

这个东西比较难讲清楚具体是干啥的，这里直接举一个例子：比如说有一个读取文件的函数，这个函数会把文件的内容，存储在一个string中，作为结果返回。

因为读取可能会失败，所以这个函数有必要返回一个信息，来表明读取文件是否成功，如果用返回空字符串的方式来表示读取失败，不太好，因为文件可能本身就是空文件，所以空字符串不能代表读取失败。

在C++17之前，可以这么写这个函数：

```cpp
string ReadFileAsString(const string& path, bool& isSuccess)
{
	ifstream stream(path);
	string res;
	if(stream)
	{
		...//做读取操作, 存到res中
		stream.close();
		isSuccess = true;
	}
	else
		isSuccess = false;
		
	return res;
}
```

而通过std::optional，就可以这么写：

```cpp
#include <optional>
// 函数返回类型由string改成了optional<string>
std::optional<string> ReadFileAsString(const string& path)
{
	ifstream stream(path);
	string res;
	if(stream)
	{
		...//做读取操作, 存到res中
		stream.close();
		return res;//
	}
	else
		return {};//注意返回的是空的
}

int main()
{
	std::optional<string> data = ReadFileAsString("data.txt");//这里的data类似一个智能指针
	if(data)
		cout << "Successfully read file:" << data.value() << endl;
	else
		cout << "Failed to read file" << endl;
		
	return 0;
}
```

也可以使用optional的value_or函数，当结果失败时赋予初始值：

```cpp
string res = data.value_or("Read Data Failed");// 如果data为空，则res初始化为括号里的值
```

总的来说，std::optional可以表示特定类型的失败的情况，函数返回类型为`std::optional<T>`，当正常返回T时，代表返回正常结果，当返回`{}`时，代表返回错误结果。
`std::optional<T>`顾名思义，可选的，它可以返回T对象，也可以不返回T对象。

<font color="red">能达到类似golang(res,err)这种多值返回的效果, 还挺方便的.</font>

## std::variant

在C++11里，如果想要一个函数返回多个变量，可以让其返回std::pair或者std::tuple，C++17提供了std::variant:

> The class template std::variant represents a type-safe union. An instance of std::variant at any given time either holds a value of one of its alternative types, or in the case of error - no value (this state is hard to achieve, see valueless_by_exception).

看std::variant，举个简单的例子：

```cpp
std::variant<string, int> data;
data = "hello";
cout << std::get<string>(data) <<endl;//print hello
data = 4;
cout << std::get<int>(data) <<endl;//print 4
cout << std::get<string>(data) <<endl;//编译通过，但是runtime会报错，显示std::bad_variant_access
data = false;//能编译通过
cout << std::get<bool>(data) <<endl;//这句编译失败
```

再介绍一些相关的api：

```cpp
//std::variant的index函数
data.index();// 返回一个整数，代表data当前存储的数据的类型在<>里的序号，比如返回0代表存的是string, 返回1代表存的是int

// std::get的变种函数，get_if
auto p = std::get_if<std::string>(&data);//p是一个指针，如果data此时存的不是string类型的数据，则p为空指针，别忘了传的是地址
```

利用C++17的特性，可以写出这样的代码：

```cpp
// 如果data存的数据是string类型的数据
if(auto p = std::get_if<string>(&data)){
	string& s = *p;
}
```

**union与std::variant的区别**
我发现std::variant跟之前学的union很类似，这里回顾一下C++的union关键字，详情可以参考[这里](https://www.cnblogs.com/jeakeven/p/5113508.html)：

```cpp
// union应该是C++11推出来的
union test
{
     char mark;
     double num;
     float score;// 共享内存
};
```

注意，这里sizeof(test)，返回的是最长的数据成员对应的大小，32位即是double的八个字节，而std::variant的大小是所有成员的总和，比如：

```cpp
variant<int, double>s;
cout << sizeof(int) <<endl;//4
cout << sizeof(double) <<endl;//8
cout << sizeof(s) <<endl;//16，这里应该都会有字节对齐
//sizeof(s) = sizeof(int) + sizeof(double);
```

所以union和variant又有这么个区别：

- union共享内存，所以存储效率更高，而variant内部成员各有各的存储空间，所以更加type-safe，所以说，variant可以作为union的替代品，如下图所示：
  ![在这里插入图片描述](https://img-blog.csdnimg.cn/20210605000236462.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L2FsZXhodTIwMTBx,size_16,color_FFFFFF,t_70)

**利用std::variant改进之前的读取函数**
前面使用std::optional，创建了一个返回类型为`std::optional<string>`的函数，在读取成功时，返回对应的string，否则返回`{}`(其实是利用initializer_list创建了空的std::optional返回)，然后用户可以通过判断返回的data是否为空来判断读取是否成功，这样写用户只能知道是否读取失败，不能知道具体失败的原因，而用std::variant可以改进这一点，代码如下：

```cpp
enum class ErrorCode
{
	None,
	NotFound,
	NoAccess
};

std::variant<string, ErrorCode> ReadFileAsString(const string& path)
{
	ifstream stream(path);
	string res;
	if(stream)
	{
		...//做读取操作, 存到res中
		stream.close();
		return res;
	}
	else
		return ErrorCode::NotFound;
}
```

总的来说，std::variant可以返回多种结果，它们的类型都是`std::variant<T1, T2...>`。
`std::variant<T1, T2...>`顾名思义，多选的，它可以返回T1对象，也可以返回T2、T3等对象，与union很像。

------

## std::any

在C++17之前，可以使用`void*`来作为存储任意类型对象的地址的指针，但是void*并不是类型安全的，C++17推出了std::any，可以用于存储任何数据类型的对象。

std::any的用法如下：

```cpp
// 方法一，创建一个std::any对象
std::any data = std::make_any<int>(4);

// 方法二，创建一个std::any对象
std::any data;
data = 4;

// 可以对data进行任意类型的赋值
data = "hello world";
data = false;
```

可以看出来，std::any的用法与std::variant的用法很像，std::variant需要在创建该对象时在`<>`里指定可以接受的参数类型，而std::any不需要指定参数类型（因为它可以接受任何类型），鉴于二者各自的特点，std::any显然更方便，但是std::any在type safe上不如std::variant，比如下面这段代码：

```cpp
std::any data;
data = "Hello World";
// 下面这个转换是不对的，因为上面的data类型是const char*, 并不是string
string s = std::any_cast<string>(data);
```

------

## std::any到底是怎么实现的

由于STL都是在头文件里实现的，所以可以直接到std::any的头文件`<any>`里去看一下源代码:

any类有一个私有成员变量，是一个union：

```cpp
union
{
	_Storage_t _Storage;// 看上去是用来存储对应对象的
	max_align_t _Dummy;// 最大字节对齐的size?
};
```

_Storage_t是一个结构体：

```cpp
struct _Storage_t
{
	union
	{
		aligned_union_t<_Any_trivial_space_size, void *> _TrivialData;
		_Small_storage_t _SmallStorage;//用来存储小的对象
		_Big_storage_t _BigStorage;//用来存储大的对象
	};
	uintptr_t _TypeData;
};
```

再看看_Big_storage_t类是怎么定义的：

```cpp
struct _Big_storage_t
{
	// 用于负责Padding的
	char _Padding[_Any_small_space_size - sizeof(void *)]; // "push down" the active members near to _TypeData
	void * _Ptr;//存储了一个void*指针
	const _Any_big_RTTI * _RTTI;//用来负责RTTI的
};
// 在别的地方可以看到
// constexpr size_t _Any_small_space_size = (_Small_object_num_ptrs - 2) * sizeof(void *);
// constexpr int _Small_object_num_ptrs = 6 + 16 / sizeof (void *);
// 所以_Any_small_space_size - sizeof(void *) = (4 + 16/sizeof(void *))* sizeof(void *) - sizeof(void*)
// 在32位上sizeof(void*) = 4, 所以等同于：
// char _Padding[32];
```

而_Small_storage_t的类定义如下：

```cpp
// 别的地方有：
using aligned_union_t = typename aligned_union<_Len, _Types...>::type;//(since C++14)

struct _Small_storage_t
{
	aligned_union_t<_Any_small_space_size, void *> _Data;// 关于aligned_union会在后面详细介绍
	const _Any_small_RTTI * _RTTI;
};
```

也就是说，std::any，存储对象的方式有两种，对于比较小的对象，会存储在拥有多种类型的union里，该对象位于stack上 ，此时用法与std::variant非常类似，对于比较大的对象，会存在堆上，用`void*`存储对应堆上的地址，在32位的机器上，这个size的分界值是32个bytes

最后总结一下std::variant与std::any的区别：

- std::variant支持的类型有限，而std::any可以支持任何类型
- std::variant更type safe
- std::variant理论上效率更高，因为对于较大的对象，它不会在heap上去分配内存
- std::any感觉不如std::variant，当实在得用std::any、不可用std::variant的时候，可能需要思考一下自己的代码设计是不是有问题

------

## 总结三种类型

std::optional、std::variant和std::any的保存对象范围是逐渐扩大的：

- std::optional，对于`<T>`，只有两种情况，一种是有T，一种是没T(failture的情况)
- std::variant，需要`<>`里指定任意类型，它可以是T1、T2…
- std::any，则不需要用`<>`来指定包含的对象类型，因为它支持任意对象，可以直接用任意类型的对象对其赋值

------

## 关于std::aligned_union

**什么是std::aligned_union**
在cplusplus网站上，是这么介绍的：

```cpp
template <size_t Len, class... Types> struct aligned_union;
```

> Aligned union
> Obtains a POD type suitable for use as storage for any object whose type is listed in Types, and a size of at least Len.
> The obtained type is aliased as member type aligned_union::type.
> Notice that this type is not a union, but the type that could hold the data of such a union.

**关于POD**
这里有必要介绍一下什么是POD type，POD全称是Plain Old Data，指的一种类，这种类没有自定义的ctor、dtor、copy assignment operator和虚函数，也被称为聚合类(aggregate class)，POD里也没有成员函数

> A Plain Old Data Structure in C++ is an aggregate class that contains only PODS as members, has no user-defined destructor, no user-defined copy assignment operator, and no nonstatic members of pointer-to-member type.

**aligned_union用法**
再回头看aligned_union，它是一个模板类，一个alighned_union对象代表着一块内存区间(storage)，这块区间的大小至少为Len个字节，这块区间可以作为一个union所需要的内存空间。

具体的可以看这么个例子：

```cpp
#include <memory>
#include <type_traits>

// include和use是两个简单的类
struct include
{
    std::string file;
};
struct use
{
    use(const std::string &from, const std::string &to) : from{ from }, to{ to }
    {
    }
    std::string from;
    std::string to;
};


{
	// 分配一块区间，这个区间可以作为include和use类对象的union
    std::aligned_union<sizeof(use), include, use>::type storage;

	// 前面只是分配了内存，没有创建真正的union
	// 取storage的内存地址，转化为void*，然后使用placement new在对应的位置创建一个use类对象
    use * p = new (static_cast<void*>(std::addressof(storage))) use(from, to);

    p->~use();.// 手动调用析构函数
}
```

再举个例子，这个是cplusplus官方给的例子：

```cpp
// aligned_union example
#include <iostream>
#include <type_traits>

// 一个简单的Union类，它可以存int、char或double类型
union U {
  int i;
  char c;
  double d;
  U(const char* str) : c(str[0]) {}
};       // non-POD

// sizeof(U)会取union里最大的数据成员，即sizeof(double)
typedef std::aligned_union<sizeof(U),int,char,double>::type U_pod;

int main() {
	// U_pod代表U这种union的内存分配器
    U_pod a,b;              // default-initialized (ok: type is POD)
    
    // 在a对应的地址，调用placement new，调用U的ctor
    new (&a) U ("hello");   // call U's constructor in place
    
    // 由于b和a是同种类型，而且是POD，可以直接进行赋值
    b = a;                  // assignment (ok: type is POD)
    
    // 把b直接转换成U&类型，然后print其i
    std::cout << reinterpret_cast<U&>(b).i << std::endl;

    return 0;
}
```