
# Equivalence by Canonicalization for Synthesis-Backed
Refactoring

JUSTIN LUBIN, University of California, Berkeley, USA  
JEREMY FERGUSON∗, University of California, Berkeley, USA  
KEVIN YE∗, University of California, Berkeley, USA  
JACOB YIM∗, University of California, Berkeley, USA  
SARAH E. CHASINS, University of California, Berkeley, USA  

  
PLDI 24 'June  
  
开源工具: Cobbler  
https://github.com/justinlubin/cobbler  
  
  
## 摘要  

通过判断等价性寻找可替代的函数，以达到优化代码运行速度  
  
## 本文方法  

1.规范化输入  
2.规范化函数  
3.判断输入与生成函数是否等价
4.若等价则可进行替换  

###  规范化函数
问：是否可以找到一个代码片段S以及对应的参数σ使得σS=输入程序P？  
  
对此，提出一个最基本的情况，即无需填充参数即可比较。  
对于一个给定的代码片段进行以下两步:  
1.Inlining，即替换所有可以替代的函数  
2.Partial evaluation， 即计算可事先计算的值  
通过上述两个步骤就可以得到一个最简的函数  
**注：这张paper主要是针对elm（haskell家族）展开的工作，放在其他语言（如：C++）则这种方法不一定合适**
  
  
然而，多数函数的最简片段都有需要填入的位置参数，  
对此，这项工作对于两个不同的语言采用了不同的“填洞”对策，分别为：  
1. Python: E-graph(Equivalent graph)  
2. Elm: Higher-order unification  
  
经过以上两个步骤的两个代码片段几个进行语法比较，判断是否等价  
  
### 替代函数生成函数
  
1. 一个枚举器，枚举所有函数  
2. 规范化生成函数  
3. 实例化（即“填洞”）    
4. 判断等价性  
5. 返回替代函数    

## 性能

1.优化后的python代码最好效果可以把运行时间进步1.95倍  
2.运行时间与拼接函数呈指数级别的增长  
其余RQ可以直接参考paper

## 个人想法

1.是否可以在别的语言展开类似工作？  
2.对于拼接函数那块是否可以进行优化？