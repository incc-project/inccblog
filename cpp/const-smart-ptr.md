`shared_ptr<T>::operator->`返回的是T*类型指针，非const T*指针。因此通过`const shared_ptr<T>&`类型的ptr可以直接调用T各个原始的方法，不用担心const与非const问题。具体`shared_ptr::operator->`实现如下:

```c++
T * operator-> () const // never throws
{
       BOOST_ASSERT(px != 0);
       return px;
}
```


可以看出`shared_ptr<T>`的`operator->`中的const修饰，是指`shared_ptr<T>`对象自身，而非shared_ptr所管理的对象。

这个和普通原生指针还是有很大区别的，需要注意。
