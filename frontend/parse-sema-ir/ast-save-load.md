AST树的本地化存储和读入借助ASTWriter和ASTReader，Clang还提供了一些高层次的类ASTUnit（Utility class for loading a ASTContext from an AST file），将AST树保存为二进制文件，也可以加载AST文件构建ASTContext。

- 加载AST文件构建ASTContext：

  ```
  ASTUnit::LoadFromASTFile("input.ast",Diags,opts);
  ```

- 将AST树保存为二进制文件。

```
ASTUnit* Unit=ASTUnit::LoadFromCompilerInvocationAction(invocation, Diags1); 

if(Unit&& !Unit->Save("output")){//这里的保存成功是返回false std::cout<<"save success!"<<std::endl; }
```

除了代码的方式, clang也提供了命令行的方式保存和加载ast:

-emit-ast: https://discourse.llvm.org/t/driver-support-for-ast-features-ui-proposal/13823

> 注意-ast-dump打印出的是部分ast, 完整可编译的要用-emit-ast
