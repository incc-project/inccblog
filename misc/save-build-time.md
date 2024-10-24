# 如何节省LLVM构建时间？

> 如果不需要调试的话, 用Release模式进行构建是最有效的加速方法. 
>
> 不推荐用RelWithDebInfo模式, 调试时非常不方便.

(1) 用`LLVM_TARGETS_TO_BUILD`指定构建目标(一般是x86), 避免构建所有目标:

```shell
cmake -DLLVM_TARGETS_TO_BUILD="X86"
```

(2) 通过设置`cmake -DBUILD_SHARED_LIBS=ON`将LLVM各个组件构建为共享库, 采用动态链接而非静态链接. 

(3) 构建llvm-tblgen的优化版本

一般而言, 我们不需要对TableGen自动生成的代码启用调试信息, 因此通过设置`cmake -DLLVM_OPTIMIZED_TABLEGEN=ON`对其采用Release模式进行编译即可.

最终CMake配置:

```shell
-DLLVM_ENABLE_PROJECTS="clang;lld" 
-DLLVM_TARGETS_TO_BUILD="X86" 
-DBUILD_SHARED_LIBS=ON 
DLLVM_OPTIMIZED_TABLEGEN=ON
```

# References

1. [Notes on setting up projects with LLVM](https://weliveindetail.github.io/blog/post/2017/07/17/notes-setup.html)
2. [第1章：构建LLVM时节约资源](https://blog.csdn.net/cppclub/article/details/135737248)