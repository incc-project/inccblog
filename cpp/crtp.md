https://blog.csdn.net/u011436427/article/details/125597908

简单来说，CRTP有两大特性：

- 继承自模板类。
- 派生类将自身作为参数传给模板类。

```c++
// 我们先定义一个模板类作为基类
template <typename T>
class Base {
    ...
};
// 定义一个派生类，这个类继承以自身作为参数的基类
class Derived : public Base<Derived> {
    ...
};
```

问题来了，为什么要这样做呢？目的其实很明确，从基类对象的角度来看，派生类对象其实就是本身，这样的话只需要用一个static_cast就可以把基类转化成派生类，从而实现基类对象对派生对象的访问. **否则要通过动态绑定才能在基类中访问其派生类, 比较慢.**

```c++
template <typename T>
class Base {
public:
    void name() {
        (static_cast<T*>(this))->impl();
    }
};

class D1 : public Base<D1> {
public:
    void impl() {
        std::cout << "D1" << std::endl;
    }
};

class D2 : public Base<D2> {
public:
    void impl() {
        std::cout << "D2" << std::endl;
    }
};

template <typename T>
void test(Base<T> b) {
    b.name();
}

int main() {
    Base<D1> b1;
    Base<D2> b2;

    test(b1);
    test(b2);
    return 0;
}

// 传统方法是把Base变成interface, CRTP可以达到相同的效果, 且速度更快.
```