https://ieeexplore.ieee.org/abstract/document/931893

IEEE Transactions on Computers 2002

Thomas Kistler, Member, IEEE, and Michael Franz, Member, IEEE

本文通过收集代码的动态运行数据, 对代码进行PGO优化, 并用优化后的代码热更新优化前的代码, 整个过程会持续进行.

两个关键技术:

* object layout adaptation: 优化数据的存储布局, 提升Cache命中率.
* dynamic trace scheduling: 优化指令调度以提升指令并行度.

局限性: 动态优化的成本可能大于其收益.

实验结果: 在有利的场景下可以实现96%的效率提升.