# Corona-SpineCpp说明文档

## 1.用户故事

​	事情大概是这样的，我一朋友跟我说他在corona引擎里边使用spine动画很卡，让我帮忙看看能不能优化下来。

​	于是看了一下他的项目里边的spine的用法， 用的是 spine-lua运行库 + corona-spine适配器（spine.lua）的方案。

​	通过visual studio 的 profile工具可以大致了解到，cpu耗时会在这几个地方产生

- lua-c的交互栈的数据传递（spine顶点数越多，耗时越多）
- spine-lua运行库本身的消耗，其中内存相关的申请和回收会消耗掉挺多时间
- 在c++（corona）侧 ，一些对象的构造也会消耗一些时间（paint+ShapeObject）
- 因为他的项目里边使用的spine.lua版本比较旧，没有mesh复用的特性，构造paint和ShapeObject的时间就更多了。



​	**所以**

​	从上边的profile可以大致决定优化的方向

- 使用spine-cpp运行库
- 在c++层实现 corona-spine适配器 



## 	2.关于代码和一些测试资源

​	

- 在 ..\platform\test\assets2路径下，我放置了这个[开源项目](https://github.com/EsotericSoftware/spine-runtimes)的一些测试资源，包括spine-lua运行库， spine-solar2D适配器代码，spine艺术资源
- 在 ..\external\spine-cpp路径下， 存放了特定版本的[spine-cpp](https://github.com/EsotericSoftware/spine-runtimes)运行库



## 3.关于适配器代码

- ..\librtt\Rtt_Spine.cpp
- ..\librtt\Rtt_Spine.h
- ..\librtt\CoronaSpineExtension.cpp
- ..\librtt\CoronaSpineExtension.h

一共 4份文件， 2个cpp文件，2个h文件

- CoronaSpineExtension.cpp 负责替代spine-runtime中的Extension.cpp的职能,让spine-runtime的文件IO，内存申请释放等调用指向corona的Rtt_Malloc ，Rtt_CAlloc ，Rtt_FileXXX 
- Rtt_Spine.cpp 和spine-solar2D下的spine.lua是等价 ，主要spine的帧驱动，spine的绘制。代码的逻辑和spine.lua不能说很相似，只能说一模一样。;)



#### 	其他

- ​	Rtt_Spine仅仅实现了一小部分api， 这些api覆盖了
  - 回调设置
  - 字段访问
  - 带参数的api
  - ~~带返回值的api（没实现，但是可以参考display.colorSample，使用了回调进行参数返回）~~
- Rtt_Spine.cpp代码，写的比较随意
- 尚未进行大量的可靠性测试
- 目前仅在windows平台进行了一些简单测试
- 如果要进行性能测试，建议release下进行

## 4.测试

在assets2目录下，有一些简单的测试用例

为了放大性能上的一些差距，使用了特别多的spine实例进行了测试



## 5.关于index的bug

- 因为Rtt_Spine复用了ShapeObject对象， 会对内部的渲染数据执行更新，但是在android平台下，GLGeometry::Update 的逻辑会执行到 ‘else if ( fVBO )’ ， 这边没有进行index数据的更新，但是corona的作者在这个[issue](https://github.com/coronalabs/corona/issues/390#issuecomment-1126447298)下给了详细的解决方法。

