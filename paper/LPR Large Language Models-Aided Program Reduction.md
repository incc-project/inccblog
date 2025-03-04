# LPR: Large Language Models-Aided Program Reduction

### Abstract

**程序归约：**通过自动最小化触发编译器错误的程序来促进编译器的调试。（如Perses和Vulcan）或通过利用特定语言的知识专门针对某一特定语言进行优化（先解释程序规约，那我可以先解释一下什么是增量编译）

然而，在程序缩减中协同地组合跨语言的通用性和对特定语言的最优性两者还有待探索。（提出but）

本文提出了语言通用程序约简（LPR）——第一个利用语言通用程序模型（LLM）进行多语言程序约简的LLM辅助技术

**核心思想：**利用Perses等程序约简器的语言通用性和LLM学习到的语言特定语义

就是先用Perses等程序约简器将程序约简到合适的适合LLM处理的小规模，然后让LLM通过学习语义来创造新的约简机会，进一步减小程序。

**evaluation：**50 benchmark across three programming language（C、Rust and JavaScript )

**Vulcan:**最先进的语言通用程序缩减器，本文说明它比Vulcan更加的实用和优越

LPR分别在C，Rust和JavaScript的基准测试中产生了24.93%，4.47%和11.71%的较小程序，超过了Vulcan。此外，LPR和Vulcan具有互补的潜力。对于C-Reduce优化的C语言，通过将Vulcan应用于LPR产生的输出，我们可以获得与C-Reduce相当的程序大小。

在用户感知的效率层面，LPR在减少大型和复杂程序时更有效，分别为10.77%，34.88%，在C、Rust和JavaScript中分别完成所有基准测试的时间比Vulcan少36.96%。

### Introduction

问题：程序约简中的一个关键挑战还没有被适当地解决——在跨语言的通用性和对特定语言的专用性之间的折衷

**语言专用约简（language-specific reduction）：**利用特定语言的语义来转换和缩小某些语言的程序。在约简方面更有效，但需要对语言特性有深入的理解，并需要大量的时间和工程工作。所以只有有限的一组语言有定制设计的约简器（如C [27]、Java [15，16]和SMT-LIBv 2 [26]）

**语言类属约简（language-generic reduction）：**只使用适用于任何编程语言的转换。可以应用于各种语言，但是缺乏语言特征和语义的知识，因此不能执行能够实现进一步约简的语言专用变换（例如，函数内联）。就是不能达到很好的一个约简结果。

本文试图将两者的优势结合，在跨语言的通用性和特定语言的专用性之间找到一个最佳点

此外，利用LLM可以简化reducer的定制和扩展，因为手动实现特定于语言的reducer或向现有reducer添加功能（例如使用Clang前端实现C-Reduce）将是耗时且劳动密集型的。C/C++特定程序转换）

**挑战：**

* 当LLM被指示将源代码作为输入处理时，由于LLM的固有限制[9，19，40]，LLM可能无法理解代码中的细微差异[20]并且被不相关的上下文[30]分散注意力。
* 输入程序通常包含数万行代码，超出输入限制
* 没有有效指导的情况下，LLM不清楚要执行哪些转换。

为了解决这些挑战，LPR设计了多级提示方法。具体来说，LPR首先要求LLM识别给定转换的潜在目标列表，然后依次指示LLM对每个目标应用转换。多级提示以更集中的方式引导LLM，通过排除可能分散LLM注意力的不相关上下文和其他目标。

**LLMs-Aided Program Reduction（LPR）：**LPR在调用语言通用程序约简器和LLM之间交替（我们在实验中使用Perses）和LLM。最初，Perses有效地将大型程序减少到LLM可管理的大小。随后，LLM根据指定所需转换的特定用户定义提示进一步转换Perses的输出。

Input ——> Perses Output ——> LLM ——> Perses ······

已经确定了五种语言的通用转换，以实现进一步的简化：函数内联、循环展开、数据类型消除、数据类型简化和变量消除。

LPR：

* 通过将LLM和语言通用程序约简器的功能相结合，LPR既具有跨语言的通用性，又具有特定语言的语义感知能力。此外，LPR还可以通过简单地设计新的prompts来方便灵活地扩展新的转换。
* 提出了一种多层次的提示方法来指导LLM执行程序转换，并在实践中证明了它的有效性。我们提出了五种通用的LLM转换，以减少程序或为语言通用程序减少器创造更多的减少机会。
* 我们在C、Rust和JavaScript三种常用语言的50个benchmarks上对LPR进行了全面评估。结果表明LPR具有很强的有效性和通用性。

### Background

**HDD：**将程序解析成解析树，然后在树的每一级应用DDMin [50]，以尽可能多地删除不必要的树节点。

**Perses：**比HDD更进一步，对形式语法执行某些转换，以避免生成语法错误的程序变体。

**Vulcan：**扩展了Perses，通过引入新颖的辅助归约器，以通过替换标识符/子树并删除解析树上的树节点的局部组合来穷尽地搜索较小的变体

然而，尽管LLM很有用，但一些研究人员[20]指出，目前的LLM在区分程序之间的细微差别方面很弱。此外，LLM的记忆和处理能力随着输入大小的增加而下降[9，29]。此外，人们不能期望LLM自动完成复杂任务;他们必须得到相应的指导[20，46]。因此，对于程序缩减来说，直接要求LLMs缩减数万行的程序是不切实际的。

### Approach

**motivation：**

![image-20250228114615497](C:\Users\wrx\Documents\lpylqbz Files\notes\LPR Large Language Models-Aided Program Reduction.assets\image-20250228114615497-1741068295724-5-1741068299237-7.png)

**workflow：**

![image-20250228115018335](C:\Users\wrx\Documents\lpylqbz Files\notes\LPR Large Language Models-Aided Program Reduction.assets\image-20250228115018335-1741068292299-3.png)

1. input

* P：待缩减的程序。
* ψ：需要在缩减过程中保持的属性。
* prompts，LLM生成一个归约的程序Pmin

output：缩减后的程序 Pmin，它仍然满足属性 ψ

2. 初始化

Pmin 被初始化为 PPP，即一开始程序的最小化版本是原始程序

3. repeat：

**获取转换列表**： 算法通过 `getTransformList(prompts)` 函数获取可能的转换操作列表（即 `transformList`）。

**遍历每个转换**： 对于每个转换操作：

* 获取该转换的**主要问题** (`primaryQuestion`)。
* 获取该转换的**后续问题** (`followupQuestion`)。
* 根据主要问题，获取一个目标列表（`targetList`），指定哪些程序部分可以被转换影响 (`getTargetList(P, primaryQuestion)`)。

对于目标列表中的每个目标：

* 让LLM应用该转换操作于目标部分，生成临时的程序版本 Ptmp
* 检查Ptmp是否满足属性ψ，满足则替换

替换完后运行 `Perses` 进行进一步的缩减，直到程序达到最小，并且满足ψ



在示例中，primaryQuestion中的目标列表其实就是那三个循环

followupQuestion可以定义为“给定程序{ PROGRAM }和循环for（i=0;.），通过循环展开对其进行优化”

**Transformations：**

为了通过LLM进一步寻找减少bug-tringgering程序的机会，我们提出了五个通用的变换来指导LLM，即函数内联(Function Inlining)，循环展开(Loop Unrolling)，数据类型消除(Data type Elimination)，数据类型简化(Data Type Simplification)和变量消除（Variable Elimination）。

* Function Inlining: 该转换方法识别程序中的一个函数，并将其内联，消除该函数的所有调用点，将它们替换为函数体的代码。这种方法可以减少程序中的标记（tokens），同时为进一步优化提供机会，因为很多导致程序出错的程序中常常包含函数调用。
* Loop Unrolling: 循环展开是一种常见的循环优化方法，通过将循环结构“展开”成多次执行单次迭代的代码段，优化程序执行。在程序中，这种方法也可以帮助找到更多的优化机会，因为语言通用的简化工具可能无法直接拆解或去除循环结构，但可以减少展开后多次重复的代码。
* Data Type Elimination:在出错的程序中，一些数据类型可能与错误无关。例如，在C语言中，typedef定义的标识符，或在Rust中用type关键字创建的类型别名。数据类型消除方法可以消除这些别名，并将其替换为原始的数据类型，从而减少冗余。
* Data Type Simplification:对于复杂数据类型（如结构体、数组和指针），并非所有成员对于触发错误的性质都是必要的。比如一个包含三个整数成员的结构体，可以简化为三个独立的整数变量，甚至可能只有一个变量对触发错误至关重要。数据类型简化将复杂的数据类型转换为更简单的原始数据类型（如整数或浮点数），以减少冗余。
* Variable Elimination:中间变量在程序中很常见，消除这些变量对于程序优化非常重要。有些变量虽然未被使用，但却很难去除。例如，删除一个未使用的参数时，不仅要删除函数中的该参数定义，还要同时删除调用该函数时传递的相应参数。这对于通用的简化工具来说可能很困难。变量消除方法可以优化并去除这些中间变量和未使用的变量。

**Multi-level Prompts：**

![image-20250228134231897](C:\Users\wrx\Documents\lpylqbz Files\notes\LPR Large Language Models-Aided Program Reduction.assets\image-20250228134231897-1741068307989-10.png)

此策略排除不相关的上下文，确保查询更有针对性，从而提高LLM生成高质量结果的可能性。对于其他转换，提示遵循类似的模板-首先提示LLM识别目标列表，然后指示它尝试优化每个目标。

### Evaluation

 主要针对reduction effectiveness and efficiency

**RQ1：**What is the effectiveness of LPR in program reduction?

**RQ2：**To what extent is the efficiency of LPR in program reduction perceived by users?

**RQ3：**What is the effectiveness of each transformation in LPR?

**Experimental Setup：**

**环境设置：**使用Perses作为语言无关的reducer，因为他的效率比Vulcan高

LLM：gpt-3.5turbo-0613 Temperature = 1.0 n=5

环境：Ubuntu 22，Intel（R）Xeon（R）Gold 6348 CPU@2.60 GHz和512 GB RAM

单进程、单线程

1. three benchmark suites：Benchmark-C，Benchmark-Rust和Benchmark-JS

* **Benchmark-C：**这个基准测试集包含20个复杂的大型C语言程序，这些程序在LLVM或GCC中触发了真实的错误。这些程序在之前的研究中就已经被收集和使用过（参考文献[38, 48, 52]）。
* **Benchmark-Rust**：这个基准测试集包含20个触发Rust程序错误的案例，也被前期的研究使用过（参考文献[48]）。
* **Benchmark-JS**：这是本研究中新创的一个私有基准测试集。它使用FuzzJIT工具对广泛使用的JavaScript引擎JavaScriptCore（版本c6a5bcc）进行模糊测试（fuzzing），并随机收集了10个导致JIT编译器出现错误的JavaScript程序。

2. three baseline：perses, Vulcan and C-reduce

* **Perses**： Perses是一个非常高效的程序简化工具，用于减少程序冗余。它的主要特点是在简化过程中，能够转换和规范化编程语言的正式语法，避免生成语法无效的变种（syntactically invalid variants）。这种方法确保简化后的程序在语法上是合法的，并且适用于多种编程语言。
* **Vulcan**： Vulcan是基于Perses的工具，提供了三个手动设计的辅助简化器，进一步在Perses的结果上寻找简化机会。与Perses相比，Vulcan能够进一步减少程序中的标记数量（tokens），但牺牲了部分运行时间。Vulcan同样是语言无关的，适用于多种编程语言。
* **C-Reduce (v2.9.0)**： C-Reduce是一个在C语言中表现最为有效的程序简化工具，但它也可以应用于其他编程语言，尽管它没有针对这些语言进行专门的定制。C-Reduce在简化C语言程序方面的表现非常优秀，是该领域的标准工具之一。

 **RQ1: Reduction Effectiveness**

衡量该标准使用tokens size。针对每个benchmark对LPR和LPR+Vulcan重复5次，并在表中显示平均值和标准偏差值。

LPR improves Perses by producing 51.33%, 14.87% and 38.66% smaller programs on three benchmarks. Moreover,LPR+Vulcan improves Vulcan, by 36.73%, 14.39% and 28.15%. On C language, LPR+Vulcan performs comparably to C-Reduce, a language-specific reducer for C language.

![image-20250228140723854](C:\Users\wrx\Documents\lpylqbz Files\notes\LPR Large Language Models-Aided Program Reduction.assets\image-20250228140723854-1741068324343-13.png)

**RQ2: Reduction Efficiency**

From the perspective of users, LPR is more efficient than Vulcan on more complex programs with longer processing time, while Vulcan reduces faster than LPR on simpler and shorter programs.

![image-20250228141848663](C:\Users\wrx\Documents\lpylqbz Files\notes\LPR Large Language Models-Aided Program Reduction.assets\image-20250228141848663-1741068328757-16.png)

**RQ3: Effectiveness of Each Transformation**

In Benchmark-C, all proposed transformations contribute to the further reduction by either shrinking the programs directly or providing reduction opportunities to Perses.

![image-20250228143616173](C:\Users\wrx\Documents\lpylqbz Files\notes\LPR Large Language Models-Aided Program Reduction.assets\image-20250228143616173-1741068334275-19.png)



![image-20250228143625066](C:\Users\wrx\Documents\lpylqbz Files\notes\LPR Large Language Models-Aided Program Reduction.assets\image-20250228143625066-1741068338379-22.png)

![image-20250228143754627](C:\Users\wrx\Documents\lpylqbz Files\notes\LPR Large Language Models-Aided Program Reduction.assets\image-20250228143754627-1741068343787-25.png)

### Conclusion

该论文提出了一种名为 LPR（LLMs-aided Program Reduction）的方法，首次利用大语言模型（LLMs）进行程序归约。LPR 结合了 LLMs 的能力与现有的语言无关的程序归约技术，实现了既能执行特定语言转换，又能适用于多种语言的归约方式。实验结果表明，在 50 个基准测试中，LPR 在 C、Rust 和 JavaScript 语言上的归约效果分别比 Vulcan 提高了 24.93%、4.47% 和 11.71%。此外，LPR 与 Vulcan 互补，通过先使用 LPR 再用 Vulcan 归约，可以达到与 C-Reduce 相近的结果。在效率方面，LPR 在处理复杂程序时表现优越，归约时间相比 Vulcan 分别减少了 10.77%、34.88% 和 36.96%。
