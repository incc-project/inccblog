<font color="red">推荐调试`TexNodeDumper`</font>

# RecursiveASTVisitor

>  AST nodes are modeled on a class hierarchy that does not have a common ancestor

AST nodes模型化的这些类包括Stmt，Type， Attr，Decl，DeclContext等，这些高度抽象的类又有众多子类，为了实现统一方式访问这些内部数据结构，RecursiveASTVisitor采用了“非严格意义”访问者设计模式， RecursiveASTVisitor是“抽象访问者”，“访问者”则是用户自己定义的RecursiveASTVisitor子类，“抽象元素类”是如上所述的Stmt，Type等。严格意义上的访问者设计模式，“元素类”都有一个统一的接口（如accept（））；而在这里，“元素类”没有统一的接口，发起访问只能通过“访问者”，而且没有统一的访问接口。

RecursiveASTVisitor主要完成以下三任务（称为#Task1，#Task2,#Task3）：

```shell
1、traverse the AST (i.e. go to each node);

2、at a given node, walk up the class hierarchy, starting from the node's dynamic type, until the top-most class (e.g. Stmt,Decl, or Type) is reached.

3、 given a (node, class) combination, where 'class' is some base class of the dynamic type of 'node', call a user-overridable function to actually visit the node.
```

Task1要求给定一个root节点，深度优先方法递归遍历下去。Task1只是一种dispatch过程，由TraverseDecl、TraverseStmt、TraverseType等Traverse*（*表示node类型）成员函数实现，具体访问node还需Task2和Task3完成。

Task2，Task3实现的是针对一个具体节点的user-overridable function，Task2通过WalkUpFrom*实现，Task3通过Visit实现。下面通过例子简单说明。

```cpp
class Visitor : public RecursiveASTVisitor<Visitor> {
           TraverseNamespaceDecl(decl);
    virtual bool VisitDecl(Decl * decl){
         std::cout<<"Visit Decl!"<<std::endl;
          return true;}
    virtual bool VisitNamedDecl(NamedDecl *decl) {
std::cout<<"VisitNamedDecl!"<<decl->getQualifiedNameAsString()<<std::endl;
      return true;
    }
   virtual bool VisitNamespaceDecl(NamespaceDecl *decl){
      if(decl)
std::cout<<"VisitNamespaceDecl:"<<decl->getQualifiedNameAsString()<<std::endl;
         return true;}
}；
Visitor vt;
vt.TraverseNamespaceDecl(decl);//decl是NamespaceDecl指针 
```

 Test1：假设被编译文件包含Namespace In{}申明。打印如下：

```shell
Visit Decl!
Visit NamedDecl!In
Visit NamespaceDecl:In
```

Test2：假设被编译文件包含：Namespace In{int a;}，打印如下：

```shell
Visit Decl!
Visit NamedDecl!In
Visit NamespaceDecl:In
Visit Decl!
Visit NamedDecl!In::a
```

(1) Test2在Test1基础上还遍历了Namespace内部的申明，所以TraverseNamespace是以Namespace为root深度遍历整棵树。查看RecursiveASTVisitor.h实现过程如下：

```cpp
Derived &getDerived() { return *static_cast<Derived *>(this); }

#define TRY_TO(CALL_EXPR)                                                      \
  do {                                                                         \
    if (!getDerived().CALL_EXPR)                                               \
      return false;                                                            \
  } while (0)

template <typename Derived>
bool RecursiveASTVisitor<Derived>::TraverseDecl(Decl *D) {
  if (!D)
    return true;
  if (!getDerived().shouldVisitImplicitCode() && D->isImplicit())
    return true;
  switch (D->getKind()) {
#define ABSTRACT_DECL(DECL)
#define DECL(CLASS, BASE)                                                      \
  case Decl::CLASS:                                                            \
    if (!getDerived().Traverse##CLASS##Decl(static_cast<CLASS##Decl *>(D)))    \
      return false;                                                            \
    break;
#include "clang/AST/DeclNodes.inc"}

template <typename Derived>
bool RecursiveASTVisitor<Derived>::TraverseDeclContextHelper(DeclContext *DC) {
  if (!DC)
    return true;
  for (auto *Child : DC->decls()) {
    if (!isa<BlockDecl>(Child) && !isa<CapturedDecl>(Child))
      TRY_TO(TraverseDecl(Child));
  }

#define DEF_TRAVERSE_DECL(DECL, CODE)                                          \
  template <typename Derived>                                                  \
  bool RecursiveASTVisitor<Derived>::Traverse##DECL(DECL *D) {                 \
    TRY_TO(WalkUpFrom##DECL(D));                                               \
    { CODE; }                                                                  \
    TRY_TO(TraverseDeclContextHelper(dyn_cast<DeclContext>(D)));               \
    return true;                                                               \
  }
…
DEF_TRAVERSE_DECL(
    NamespaceDecl,
    {})
```

在上面代码中，大量运用了宏（“##”是分隔强制连接标志），生成了许多成员函数。展开宏，合并函数，还原代码如下：

```cpp
template <typename Derived>                                                  
  bool RecursiveASTVisitor<Derived>::TraverseNamespaceDecl(DECL *D) {
  Derived * temp1= static_cast<Derived *>(this);// getDerived()
temp1-> WalkUpFromNamespaceDecl(D);//TRY_TO展开
  DeclContext *DC= dyn_cast<DeclContext>(D);
  If(!DC) return true;
//展开TraverseDeclContextHelper
      for (auto *Child : DC->decls()) {
if (!isa<BlockDecl>(Child) && !isa<CapturedDecl>(Child))      
//展开TraverseDecl
    if (!Child)
        return true;
    if (!temp1->shouldVisitImplicitCode() && Child->isImplicit())
        return true;
  }
switch (D->getKind()) {
…
case Decl::NamedDecl://test2中被编译文件定义了“int a”，需要用到该分支temp1->TraverseNamedDecl(static_cast<NamedDecl *>(D));
break;
}}
Return true；}
```

由上展开代码得，在Traverse某个node时，会for循环node中保存的Decls，然后每个Decls再调用对应的Traverse函数，所以Test2比Test1多遍历了”int a;”对应的node。

(2) 在Traverse node之初，会调用WalkUpFrom*函数。其内部实现如下：

```cpp
#define DECL(CLASS, BASE)                                                      \
  bool WalkUpFrom##CLASS##Decl(CLASS##Decl *D) {                               \
    TRY_TO(WalkUpFrom##BASE(D));                                               \
    TRY_TO(Visit##CLASS##Decl(D));                                             \
    return true;                                                               \
  }                                                                            \
  bool Visit##CLASS##Decl(CLASS##Decl *D) { return true; }
#include "clang/AST/DeclNodes.inc"
```

clang/AST/DeclNodes.inc定义了如下：

```cpp
#  define NAMESPACE(Type, Base) NAMED(Type, Base)
# define NAMED(Type, Base) DECL(Type, Base)
NAMESPACE(Namespace, NamedDecl)
NAMED(Named, Decl)
```

所以最终存在两个宏DECL(Namespace,NamedDecl),DECL(Named,Decl),还原代码得：

```cpp
bool RecursiveASTVisitor<Derived>::WalkUpFromNameSpaceDecl(NameSpaceDecl *D) {                            
Derived * temp1= static_cast<Derived *>(this);// getDerived()     
Temp1-> WalkUpFromNamedDecl(D);
Temp1->VisitNameSpaceDecl(D);
      return true;                                                               
  }      
bool RecursiveASTVisitor<Derived>::WalkUpFromNamedDecl(NamedDecl *D) {                            
Derived * temp1= static_cast<Derived *>(this);// getDerived()     
Temp1-> WalkUpFromDecl(D);
Temp1->VisitNamedDecl(D);
      return true;                                                               
  }  
bool RecursiveASTVisitor<Derived>::WalkUpFromNamedDecl(NamedDecl *D) {                            
Derived * temp1= static_cast<Derived *>(this);// getDerived()     
Temp1-> WalkUpFromDecl(D);
Temp1->VisitNamedDecl(D);
      return true;                                                               
  }  
  bool RecursiveASTVisitor<Derived>::WalkUpFromDecl(Decl *D) { return getDerived().VisitDecl(D); }
  bool VisitDecl(Decl *D) { return true; }
```

所以WalkUpFrom会不断递归调用父节点的WalkUpFrom函数，最终调用的是VisitDecl，VisitNamedDecl，VisitNamespaceDecl，这正是上面所说Task 2，如果用户实现了WalkUpFromXX可以阻断向上的递归。

**如何实现RecursiveASTVisitor继承类**

申明一个类A，继承模板类RecursiveASTVisitor<A>，当需要访问某种节点时，就重载函数VisitXXX（XXX b）【如VisitNameDecl（NameDecl）】。

# References

[1] https://www.cnblogs.com/zhangke007/p/4714245.html