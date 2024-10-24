https://clang.llvm.org/docs/InternalsManual.html#declaration-contexts

### [Declaration contexts](https://clang.llvm.org/docs/InternalsManual.html#id45)

Every declaration in a program exists within some *declaration context*, such as a translation unit, namespace, class, or function. Declaration contexts in Clang are represented by the `DeclContext` class, from which the various declaration-context AST nodes (`TranslationUnitDecl`, `NamespaceDecl`, `RecordDecl`, `FunctionDecl`, etc.) will derive. The `DeclContext` class provides several facilities common to each declaration context:

> 注意不是所有Decl都继承自DeclContext, 比如别名, import这种Decl就不需要继承自DeclContext.
>
> 不过所有Decl都属于某个DeclContext, 比如所有Decl都属于TranslationUnitDeclContext, 当然实际还可以再细分
>
> DeclContext有点像一个数据库.

Source-centric vs. Semantics-centric View of Declarations

> `DeclContext` provides two views of the declarations stored within a declaration context. The source-centric view accurately represents the program source code as written, including multiple declarations of entities where present (see the section [Redeclarations and Overloads](https://clang.llvm.org/docs/InternalsManual.html#redeclarations)), while the semantics-centric view represents the program semantics. The two views are kept synchronized by semantic analysis while the ASTs are being constructed.
>
> 在Source视图下, DeclContext可以记录前向声明和重载关系

Storage of declarations within that context

> Every declaration context can contain some number of declarations. For example, a C++ class (represented by `RecordDecl`) contains various member functions, fields, nested types, and so on. All of these declarations will be stored within the `DeclContext`, and one can iterate over the declarations via [`DeclContext::decls_begin()`, `DeclContext::decls_end()`). This mechanism provides the source-centric view of declarations in the context.
>
> DeclContext可以记录一个Decl子树下的Decl.

Lookup of declarations within that context

> The `DeclContext` structure provides efficient name lookup for names within that declaration context. For example, if `N` is a namespace we can look for the name `N::f` using `DeclContext::lookup`. The lookup itself is based on a lazily-constructed array (for declaration contexts with a small number of declarations) or hash table (for declaration contexts with more declarations). The lookup operation provides the semantics-centric view of the declarations in the context.
>
> 快速查找符号表

Ownership of declarations

> The `DeclContext` owns all of the declarations that were declared within its declaration context, and is responsible for the management of their memory as well as their (de-)serialization.
>
> 管理当前DeclContext下所有Decl的所有权, 包括为期分配内存, 序列化/反序列化

All declarations are stored within a declaration context, and one can query information about the context in which each declaration lives. One can retrieve the `DeclContext` that contains a particular `Decl` using `Decl::getDeclContext`. However, see the section [Lexical and Semantic Contexts](https://clang.llvm.org/docs/InternalsManual.html#lexicalandsemanticcontexts) for more information about how to interpret this context information.



#### [Redeclarations and Overloads](https://clang.llvm.org/docs/InternalsManual.html#id46)

Within a translation unit, it is common for an entity to be declared several times. For example, we might declare a function “`f`” and then later re-declare it as part of an inlined definition:

```
void f(int x, int y, int z = 1);

inline void f(int x, int y, int z) { /* ...  */ }
```

The representation of “`f`” differs in the source-centric and semantics-centric views of a declaration context. In the source-centric view, all redeclarations will be present, in the order they occurred in the source code, making this view suitable for clients that wish to see the structure of the source code. In the semantics-centric view, only the most recent “`f`” will be found by the lookup, since it effectively replaces the first declaration of “`f`”.

(Note that because `f` can be redeclared at block scope, or in a friend declaration, etc. it is possible that the declaration of `f` found by name lookup will not be the most recent one.)

In the semantics-centric view, overloading of functions is represented explicitly. For example, given two declarations of a function “`g`” that are overloaded, e.g.,

```
void g();
void g(int);
```

the `DeclContext::lookup` operation will return a `DeclContext::lookup_result` that contains a range of iterators over declarations of “`g`”. Clients that perform semantic analysis on a program that is not concerned with the actual source code will primarily use this semantics-centric view.



#### [Lexical and Semantic Contexts](https://clang.llvm.org/docs/InternalsManual.html#id47)

Each declaration has two potentially different declaration contexts: a *lexical* context, which corresponds to the source-centric view of the declaration context, and a *semantic* context, which corresponds to the semantics-centric view. The lexical context is accessible via `Decl::getLexicalDeclContext` while the semantic context is accessible via `Decl::getDeclContext`, both of which return `DeclContext` pointers. For most declarations, the two contexts are identical. For example:

```
class X {
public:
  void f(int x);
};
```

Here, the semantic and lexical contexts of `X::f` are the `DeclContext` associated with the class `X` (itself stored as a `RecordDecl` AST node). However, we can now define `X::f` out-of-line:

```
void X::f(int x = 17) { /* ...  */ }
```

This definition of “`f`” has different lexical and semantic contexts. The lexical context corresponds to the declaration context in which the actual declaration occurred in the source code, e.g., the translation unit containing `X`. Thus, this declaration of `X::f` can be found by traversing the declarations provided by [`decls_begin()`, `decls_end()`) in the translation unit.

The semantic context of `X::f` corresponds to the class `X`, since this member function is (semantically) a member of `X`. Lookup of the name `f` into the `DeclContext` associated with `X` will then return the definition of `X::f` (including information about the default argument).

> 对于out-of-line member, 其lexical context与semantic context不一致



#### [Transparent Declaration Contexts](https://clang.llvm.org/docs/InternalsManual.html#id48)

In C and C++, there are several contexts in which names that are logically declared inside another declaration will actually “leak” out into the enclosing scope from the perspective of name lookup. The most obvious instance of this behavior is in enumeration types, e.g.,

```
enum Color {
  Red,
  Green,
  Blue
};
```

Here, `Color` is an enumeration, which is a declaration context that contains the enumerators `Red`, `Green`, and `Blue`. Thus, traversing the list of declarations contained in the enumeration `Color` will yield `Red`, `Green`, and `Blue`. However, outside of the scope of `Color` one can name the enumerator `Red` without qualifying the name, e.g.,

```
Color c = Red;
```

There are other entities in C++ that provide similar behavior. For example, linkage specifications that use curly braces:

```
extern "C" {
  void f(int);
  void g(int);
}
// f and g are visible here
```

For source-level accuracy, we treat the linkage specification and enumeration type as a declaration context in which its enclosed declarations (”`Red`”, “`Green`”, and “`Blue`”; “`f`” and “`g`”) are declared. However, these declarations are visible outside of the scope of the declaration context.

These language features (and several others, described below) have roughly the same set of requirements: declarations are declared within a particular lexical context, but the declarations are also found via name lookup in scopes enclosing the declaration itself. This feature is implemented via *transparent* declaration contexts (see `DeclContext::isTransparentContext()`), whose declarations are visible in the nearest enclosing non-transparent declaration context. This means that the lexical context of the declaration (e.g., an enumerator) will be the transparent `DeclContext` itself, as will the semantic context, but the declaration will be visible in every outer context up to and including the first non-transparent declaration context (since transparent declaration contexts can be nested).

The transparent `DeclContext`s are:

- Enumerations (but not C++11 “scoped enumerations”):

  ```
  enum Color {
    Red,
    Green,
    Blue
  };
  // Red, Green, and Blue are in scope
  ```

- C++ linkage specifications:

  ```
  extern "C" {
    void f(int);
    void g(int);
  }
  // f and g are in scope
  ```

- Anonymous unions and structs:

  ```
  struct LookupTable {
    bool IsVector;
    union {
      std::vector<Item> *Vector;
      std::set<Item> *Set;
    };
  };
  
  LookupTable LT;
  LT.Vector = 0; // Okay: finds Vector inside the unnamed union
  ```

- C++11 inline namespaces:

  ```
  namespace mylib {
    inline namespace debug {
      class X;
    }
  }
  mylib::X *xp; // okay: mylib::X refers to mylib::debug::X
  ```



#### [Multiply-Defined Declaration Contexts](https://clang.llvm.org/docs/InternalsManual.html#id49)

C++ namespaces have the interesting property that the namespace can be defined multiple times, and the declarations provided by each namespace definition are effectively merged (from the semantic point of view). For example, the following two code snippets are semantically indistinguishable:

```
// Snippet #1:
namespace N {
  void f();
}
namespace N {
  void f(int);
}

// Snippet #2:
namespace N {
  void f();
  void f(int);
}
```

In Clang’s representation, the source-centric view of declaration contexts will actually have two separate `NamespaceDecl` nodes in Snippet #1, each of which is a declaration context that contains a single declaration of “`f`”. However, the semantics-centric view provided by name lookup into the namespace `N` for “`f`” will return a `DeclContext::lookup_result` that contains a range of iterators over declarations of “`f`”.

`DeclContext` manages multiply-defined declaration contexts internally. The function `DeclContext::getPrimaryContext` retrieves the “primary” context for a given `DeclContext` instance, which is the `DeclContext` responsible for maintaining the lookup table used for the semantics-centric view. Given a DeclContext, one can obtain the set of declaration contexts that are semantically connected to this declaration context, in source order, including this context (which will be the only result, for non-namespace contexts) via `DeclContext::collectAllContexts`. Note that these functions are used internally within the lookup and insertion methods of the `DeclContext`, so the vast majority of clients can ignore them.

Because the same entity can be defined multiple times in different modules, it is also possible for there to be multiple definitions of (for instance) a `CXXRecordDecl`, all of which describe a definition of the same class. In such a case, only one of those “definitions” is considered by Clang to be the definition of the class, and the others are treated as non-defining declarations that happen to also contain member declarations. Corresponding members in each definition of such multiply-defined classes are identified either by redeclaration chains (if the members are `Redeclarable`) or by simply a pointer to the canonical declaration (if the declarations are not `Redeclarable` – in that case, a `Mergeable` base class is used instead).