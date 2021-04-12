# code-craft2021
## 赛题分析
总体来讲，这是一个简化的服务器资源调度问题，处在这样一个场景下：
>在云端服务器上，每天都会有用户的虚拟机请求到来，要么增加虚拟机，要么删除虚拟机，在初始状态下，我们没有任何服务器，为了满足用户的虚拟机部署请求，我们需要购买服务器，有很多种服务器可以购买，每种服务器都有自己的购买成本，以及每一天运行需要的维护成本，那么，应该采取什么样的策略来购买服务器，才能**既满足用户的需求，但又能尽量地减少成本呢？**
>
场景中涉及到的两个主体，是**虚拟机**以及**服务器**，涉及到的资源限制以及冲突，是**服务器的标准cpu核数以及内存数**与**虚拟机需要的cpu核数与内存数**之间的冲突，此外，赛题中还给了一个信息，即
> **NUMA**架构:每台服务器内部都存在两个 NUMA 节点:A 和 B,服务器拥有的资源(CPU 和内存)均匀分布在这两个节点上。

相应地，虚拟机也有单节点部署与双节点部署两种操作，单节点部署的虚拟机所有资源由一个节点提供，双节点部署的虚拟机所有资源由两个节点均摊。

**赛题的核心目的，或者说我们在设计算法时的最终目标，是减少经过N天的请求后的总成本**，该如何下手呢？

不妨先来看一下赛题中给的输入流程：
1. 首先会给出各种可购买的服务器的cpu、内存、价格、维护成本、类型，以及可能产生的虚拟机的类型、cpu、内存需求，还有是否是双节点部署。
2. 在每一天开始时，都会给出当天所有按序的虚拟机请求，包括add以及delete，add操作会给出虚拟机的类型、ID，ID是由输入唯一指定的，每个请求不重复。

以上就是所有的输入，也是我们能掌握的信息，那么，我们可以进行什么操作呢？

1. 根据当天的请求，提前购买服务器，保证不出现虚拟机增加请求无法满足的情况。
2. 在处理当天的请求之前，可以对已部署的虚拟机进行**迁移**，并且**迁移没有任何成本**， 迁移的作用是调整现有服务器的状态，更好地去适应新的虚拟机请求。可以从服务器的A节点向B节点迁移，也可以在服务器之间进行迁移，迁移次数有上限，是**已部署的虚拟机的5/1000向下取整**。
3. 对add请求，指定一台服务器部署，如果是单节点部署，还要指定在哪个节点上。

这意味着，我们可以从购买、迁移、部署三个方向下手，在每一个操作上，选择的策略不同，都会影响最终的成本。接下来，我将按照购买、迁移、部署的顺序，依次介绍我们的思路和策略选择。
## 思路整理
### 服务器购买策略的选择
#### 基本思路
  购买服务器，在我们小组刚开始讨论时，经过大家的思考，首先提出了一个核心的**基本准则**：
  >服务器这么贵，而且只要用了，不管用多少都要花钱，那就尽量不要产生资源的浪费，要用就拉满，**压榨服务器的所有性能，最好一滴都不剩**，称之为**紧缩思想**。
  >
  这其实是挺自然的想法，毕竟大家本来就比较穷hhhh。
  以这个准则为基础，我们进一步想到三种基本购买方式：
  * 题目既然上来就给出了所有天的请求，是否可以在第一天，就根据请求情况，一次性购买好足够满足所有请求的服务器，**反正服务器买了不用也没有维护成本， 早买晚买都一样**，这样还能考虑到所有的数据情况，也许最终得到的结果会比较接近全局最优解？
  * 直接一次性买好有点过于乐观了，因为每一天部署的情况都不一样，题目也要求**每天都要输出当天购买的服务器**，一次性买好怎么确定哪天买哪些服务器来输出呢？在每一天开始，根据当天的请求一次性购买满足当天的虚拟机请求的服务器比较靠谱。这是一种贪心策略，着眼于当天。
  * 前两种都是**离线购买的方式**， 是否可以**在线购买**呢， 也就是只着眼于当前的虚拟机请求，来一个处理一个，处理不了了，再买服务器，**够用就行，不够再说**，这是一种更加局部的贪心策略,只着眼于当前的请求。
##### 一次性购买全部服务器
  这种购买方式，由于有输出问题，我们并没有想到比较好的解决方案。
##### 一次性购买当天服务器
###### 出现的问题
  采用这种方式，可以将购买服务器问题转化成一个**整数规划**或者是**装箱问题**等。这种方法绝对是可行的，也可以找到比较好的解，但是我们并没有（摊手）。 因为要计算剩余需要的资源，就要看能部署多少虚拟机到原来的服务器上，然而，虚拟机部署时有单节点和双节点的区别，这就导致不能通过简单地加减法来算剩余需要的资源。
###### 解决方案
 1. 将所有虚拟机一视同仁，全部视为双节点，这样可以进行一定程度的简化，只需要考虑原有服务器所有剩余的cpu和内存数的较小值，然后加和，就得到总的可用资源，对当天的请求也如此，单节点的cpu和内存请求直接暴力翻倍为双节点。这样就可以直接相减得到一个cpu和内存的需求量，算是一个（假上界），因为服务器是各自独立的，A服务器剩余的资源不可以为B所用，所以可能出现较大的虚拟机依然无法部署的情况，需要单独处理。
 2. 在算出了需要的cpu和内存数后，问题就简单地变成了一个二维多选择装箱问题，我们考虑了整数规划和智能算法（包括遗传算法、粒子群算法等），但是发现求解过程复杂度其实很高，因为解的维度是跟可购买服务器的种类有关的，假如用整数规划，有1000种可买的服务器，那解的维度就是1000维，需要算出每个维度的值，代表该种服务器的购买数目，这是个很耗时的操作，如果用智能算法，则迭代次数也将很大，所以暂未想到好的解决方案。
  
###### 一种简单思路
  计算服务器的cpu/内存比，以及所需要的cpu/内存比，尽可能使其拟合，这样可以尽量避免资源浪费，从所有服务器中找到这个比值最接近的一种服务器，然后只买这一种，买够为止。经过测试，这种思路很依赖数据集的分布情况，在训练集1上表现良好，在训练集2上表现极差，加上有了效果更好的第三种方案，故放弃，但其实这种思路过于简单了，是有改进空间的。
##### 在线购买
  所谓在线购买，是**当且仅当当前add请求无法满足时才购买服务器**，这是基本思路。
###### 基本购买策略
  选取服务器的策略，我刚开始只考虑容量问题，购买的是**既能装下当且服务器，并且cpu和内存数与当前请求最接近的**服务器，衡量接近程度，采用的是cpu和内存的L2距离之和。也就是：
  > (Server.cpu - required_cpu)^2 + (Server.mem - required_mem)^2
  > 
  这种思路虽简单粗暴，但效果竟然还不错，这是因为，采用L2距离可以在一定程度上同时兼顾cpu/内存比比较均衡的虚拟机和比较极端的虚拟机，这两种情况都可以找到对应的服务器用。
###### 进一步的考虑
  进一步考虑到服务器的成本问题，将价格和维护成本乘上一个系数一起算进去，找到综合条件下的最接近服务器，这种方法被实验证明是有效的，进一步降低了总的成本，也是我们最终采用的方案。
### 服务器部署策略
#### 基本策略
  在考虑服务器的部署时，我第一时间想到的是操作系统中，进程的内存分配策略，这两个场景实在是太像了（orz）。虚拟机就像是进程，每一天服务器都是一个内存页面，把虚拟机放到服务器上，就像是把进程放到内存中，只不过我们部署虚拟机时需要同时考虑cpu和内存两个维度的限制，而内存分配只需要考虑进程大小就可以了。基本的内存分配策略有以下几种：
  * 首次适应（first fit）。从头到尾按照内存地址的顺序，找到第一个可以容纳当前进程的页面，放置进去。
  * 循环首次适应（next fit）。在遍历页面时，不再从头开始，而是从上次成功放置的页面开始遍历，找到第一个可容纳当前进程的页放进去。
  * 最佳适应（best fit）。从所有页面中找到能容纳当前进程的最小页，放置进去。
  * 最坏适应（worst fit）。从所有页面中找到能容纳当前进程的最大页，放置进去。


这几种算法理论上的孰优孰劣，不展开讨论，直接进行实验，在衡量最小和最大时，依旧采用的是cpu和内存的L2距离之和。经过测试，应用在我们的问题场景下，这几种策略效果最好的是最佳适应算法（best fit，以下简称：BF），远好过剩余几种，采纳。
#### 进一步优化的尝试
  在基本的部署策略思路清晰之后，我们进一步考虑了节点平衡问题对总成本的影响，这主要是单节点虚拟机的部署带来的，所谓节点平衡，我们设计了一种衡量指标：
>计算部署在某个节点后，两个节点的剩余cpu和内存之间的差距，这个差距越大，说明两个节点之间越不平衡。
>
自然地，我们就会考虑，节点是越平衡越好呢，还是越不平衡越好呢？经过我们的分析，似乎各有各的好处，如果节点比较平衡，那么对接下来的双节点部署虚拟机较为有利，如果节点不平衡，则对单节点部署虚拟机较为有利。似乎难分伯仲，于是进行实验，实验表明，按这种衡量指标，不管是做节点平衡，还是不平衡，对最终成本影响极小（orz）。因此，最终我们在部署时，并未考虑节点的平衡性。
### 服务器迁移策略
#### 迁移三问
   关于迁移，我们也只想到了一种比较简单的思路，因为迁移实际上是个比较复杂的操作，需要考虑以下几个问题：
   1. 从哪台服务器迁出？ 迁入到哪台服务器？如何选择？
   2. 迁入到某服务器，放置在A节点还是B节点？ 应采取什么准则？
   3. 是否需要做服务器内节点间迁移？

我们依旧是按照开始所述的**紧缩思想**，迁移的目的是尽可能压榨已有服务器的性能，尽量不产生浪费，在这个前提下，自然想到要从服务器使用率比较低的服务器往使用率比较高的服务器上迁移，**尽可能腾空使用率低的服务器**，这样就可以减少每日的维护成本。
#### 具体的迁移流程
1. 将已有服务器按使用率进行排序，并设置一个比例参数，作为可能迁出的服务器比例，或者是迁出窗口大小。
2. 按使用率从低到高遍历窗口内所有服务器。
3. 对该服务器内的每一个虚拟机，看是否有使用率高的服务器可以将其装下，如果有则迁出。
4. 不断重复23，直至当日迁移次数用完或者是窗口内服务器遍历完。

加上迁移操作后，总成本降低了很多，效果显著，但迁移耗时太多，于是我们做了一点性能上的改进。
对操作3， 在遍历虚拟机前，先对虚拟机进行**排序**，再按虚拟机从小到大进行迁移判断，**如果较小的服务器都迁移不出去的话，那比他更大的更迁移不了**， 直接查看下一台服务器，这样可以减少无效判断。

## 初赛
由于时间有限，并且代码主要就是我自己在写，队友不太会C++（虽然我也不太会），也各有各的事情，所以实现的想法也比较有限，没有做过多尝试。最终初赛我们采取的就是上面几个比较简单直接的思路，三天的时间，其实很紧张。看群里面很多dalao都在讲调参的事情，但我们采用的策略里面，似乎没什么参数可调（orz），所以甚至30次提交机会都是最后一下午乱用的，最终在线上数据集的的结果大概是在14.47—14.50e 之间徘徊，有限的几个参数怎么调都无法寸进，但所幸还是进了复赛,初赛第10名。但愿我们的**紧缩思想**可以帮助我们尽可能逼近全局最优，其实是有点担心我们的简单策略是下限高上限低的，之后需要进行更多优化尝试。不过，第一次参加这种比赛，就算进不了决赛圈，收获已经颇丰。继续加油！

## 复赛
10天的训练赛很快过去，这期间经历了上课开会改代码各种杂七杂八的事情，还有每周一次的组会，所幸我的导师人很好，极为善解人意，我在组会上提出在参加这个比赛之后，竟然得到导师的支持，愿意让我尽可能地去尝试一下，能走多远走多远，作为一个cv的研究生，其实做的事情与这个比赛没啥关系，之所以我会来参加，可能更多还是因为要弥补一下本科过得太水没有参加的遗憾吧（成电搞这个比赛的氛围真的很浓，成渝赛区也几乎是成电内战，当然重庆那边也有很多强队）。
4.11清晨，我们一行三人踏上了前往华为南研所的旅程。前一天的晚上，我睡得并不好，导致一路上都昏昏沉沉的，脑子不怎么转，帮队员调一个很简单的bug都搞了很久，所幸到了现场还算是进入了状态。主办方的工作组很热情，我们一进门，就有数位小姐姐来迎接，每个人都挑选一件文化衫，然后去一起合影吃饭。不过不得不说，华为盒饭水平有待提高，我总觉得不如我做的（小声bb）。到了1点钟，所有参赛队伍进入一个大房间，每个队伍一张桌子，房间里还有各种饮料零食可吃，还是挺令人心情愉悦的，很快，复赛正式开始。
复赛正式赛的题目相比于练习赛，除了数据集上的不同以外，还有一个规则上的变化点，**我们有一次机会，可以在某一天迁移时突破每日的迁移上限，至多对所有的虚拟机进行迁移**，这对那种重迁移算法的队伍来说算是重大利好，对我们来说也算有利。可惜，练习赛的排名已经证明，我们一直以来采用的基本算法，虽然泛化性强，但是难以做到最好，最终的正式赛排名也是不出所料，没有奇迹发生，我们的效果就是个中等水平。最终20名无缘决赛，不过还是想复盘记录下失败的原因。

我们在设计算法时，更多的是重思想和策略，但是细节把握不够，可能在各种数据集上都能得到中等甚至偏上的成绩，但是在特定条件想做到极致，很难。这个问题在正式赛的三小时里，表现得尤为明显，在各个队伍紧张地进行调参时，我们发现自己陷入了无惨可调的窘境，**一句话，我们的算法第一次跑出来是什么成绩，就是什么成绩，没有太多优化的余地。**所以我们就眼看着自己的排名从十四五，一直掉到了20名。参数多，就可以更好地去拟合条件和数据，而我们的算法是严重与数据割裂的极为局部的贪心策略。这是导致我们最后失败的原因。
我相信成绩好的队伍，跟我们用的算法，从根本上就是不同的，可能在某些思想上具有一致性，但是不会如此局促。我们利用的数据信息实在很有限，比如购买服务器，只是在当前虚拟机满足不了时才会买，这就只能利用一个请求信息。但实际上，题目已经给出了当天一整天的乃至K天的请求，显然，与那种可以充分利用这些信息的算法相比，我们的算法很容易陷入局部解，难以寸进。不过很可惜，由于知识储备的不足，即便已经意识到这个问题， 也并未能想出好的解决方案，我们考虑过遗传算法粒子群算法等智能算法，但是没有很好地应用到我们的场景中来，还是太生疏了。
在比赛最后一小时里，我们已经充分意识到这是一次一轮游，所以就开始开心地哈哈哈了，我们在购买时加了一点点的随机，希望能跑出一点进步，事实上确实也有进步，不过是非常非常微小的，但是我们还是很开心。在江山赛区，想进前四名，就要把成本控制在15.5e左右，但是我们不管怎么调，都稳定在16.8e，变化的是第4位，这就很离谱了，这结果跑的我们想笑。
# 总结
这次挑战赛，从报名开始，陆陆续续持续了大概一个月，这段时间里付出了很多，翘了不少课，少交了不少作业，最后止步决赛，拿了江山赛区二等奖。不能说什么都没有收获，至少我C++水平得到了大幅度的提高，对一些数据结构还有容器有了更加清晰的认识，二等奖的奖品是一人一个华为手环4和机试绿卡，还有证书，算是个安慰吧。明年的比赛，如果到时候有时间的话，应该还会来参加的吧，感觉打比赛还是很有意思的一件事。
这个学期的浪费余额，就到此为止吧！
# 我们的队名：排水渠过弯
# 明年再见！