源码中塞上信号处理:

```cpp
#include <execinfo.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>

void signalHandler(int signal) {
    // stacktrace size
    void *array[20];
    size_t size;

    size = backtrace(array, sizeof(array) / sizeof(array[0]));

    fprintf(stderr, "\n==================================================\n");
    fprintf(stderr, "Error: signal %d:\n", signal);
    backtrace_symbols_fd(array, size, STDERR_FILENO);
    fprintf(stderr, "==================================================\n");
    exit(1);
}

void registerSignalHandler() {
    signal(SIGINT, signalHandler);
    signal(SIGABRT, signalHandler);
    signal(SIGFPE, signalHandler);
    signal(SIGILL, signalHandler);
    signal(SIGSEGV, signalHandler);
}
```

main函数中注册信号处理:

```cpp
int main() {
#ifndef NDEBUG
    registerSignalHandler();
#endif
	...
}
```

这样程序崩溃时就会自动打印崩溃栈.

测试程序bad.c:

```cpp
#include <stdio.h>
#include <execinfo.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>

void signalHandler(int signal) {
    void *array[10];
    size_t size;

    size = backtrace(array, sizeof(array) / sizeof(array[0]));

    fprintf(stderr, "\n==================================================\n");
    fprintf(stderr, "Error: signal %d:\n", signal);
    backtrace_symbols_fd(array, size, STDERR_FILENO);
    fprintf(stderr, "==================================================\n");
    exit(1);
}

void registerSignalHandler() {
    signal(SIGINT, signalHandler);
    signal(SIGABRT, signalHandler);
    signal(SIGFPE, signalHandler);
    signal(SIGILL, signalHandler);
    signal(SIGSEGV, signalHandler);
}

void baz() {
  int *foo = (int*)-1; // make a bad pointer
  printf("%d\n", *foo); // causes segfault
}

void bar() { baz(); }
void foo() { bar(); }

int main(int argc, char **argv) {
  registerSignalHandler();
  foo(); // this will call foo, bar, and baz.  baz segfaults.
}
```

执行结果:

```shell
clang -g -o bad bad.c
./bad

==================================================
Error: signal 11:
./bad(+0x1a39)[0x55d29fc79a39]
/lib/x86_64-linux-gnu/libc.so.6(+0x42520)[0x7f0469359520]
./bad(+0x1b27)[0x55d29fc79b27]
./bad(+0x1b49)[0x55d29fc79b49]
./bad(+0x1b59)[0x55d29fc79b59]
./bad(+0x1b79)[0x55d29fc79b79]
/lib/x86_64-linux-gnu/libc.so.6(+0x29d90)[0x7f0469340d90]
/lib/x86_64-linux-gnu/libc.so.6(__libc_start_main+0x80)[0x7f0469340e40]
./bad(+0x1955)[0x55d29fc79955]
==================================================
```

可以通过addr2line命令还原源码信息.

```shell
llvm-addr2line -e ./bad 0x1a39
/home/hzy/桌面/test/coredump/bad.c:11
```

> 如果是gcc编译的, 就用addr2line, 如果是clang编译的, 就用llvm-addr2line

关于addr2line:

      Linux下addr2line命令用于将程序指令地址转换为所对应的函数名、以及函数所在的源文件名和行号。当含有调试信息(-g)的执行程序出现crash时(core dumped)，可使用addr2line命令快速定位出错的位置。
    
      如果无法确定文件名或函数名，addr2line将在它们的位置打印两个问号；如果无法确定行号，addr2line将打印0或一个问号。
    
      参数说明：
    
      (1).-a:在函数名、文件名和行号信息之前，以十六进制形式显示地址。
    
      (2).-b:指定目标文件的格式为bfdname。
    
      (3).-C:将低级别的符号名解码为用户级别的名字。
    
      (4).-e:指定需要转换地址的可执行文件名，默认文件是a.out。
    
      (5).-f:在显示文件名、行号信息的同时显示函数名。
    
      (6).-s:仅显示每个文件名(the base of each file name)去除目录名。
    
      (7).-i:如果需要转换的地址是一个内联函数，则还将打印返回第一个非内联函数的信息。
    
      (8).-j:读取指定section的偏移而不是绝对地址。
    
      (9).-p:使打印更加人性化：每个地址(location)的信息都打印在一行上。
    
      (10).-r:启用或禁用递归量限制。
    
      (11).--help:打印帮助信息。
    
      (12).--version:打印版本号。
    
      dmesg命令：全称为display message，用来显示Linux内核环形缓冲区信息。

另一种方式是使用coredump, 主流的linux系统目前会用coredumpctl管理coredump日志:

查找bug:

```shell
coredumpctl list --since=today
```

记住PID

查看转储的信息:

```shell
coredumpctl info $PID
```

Stack trace在最后

调试:

```shell
coredumpctl gdb $PID
```

