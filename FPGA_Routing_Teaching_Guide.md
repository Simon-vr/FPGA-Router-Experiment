
# FPGA 布线零基础教学文档（面向 lab3 作业）

> 目标读者：完全零基础、没学过布局布线的同学。
>
> 你看完这份文档后，应该能回答三个问题：
>
> 1) 现实中的 FPGA 布线到底在解决什么问题；
> 2) 这个作业把现实问题抽象成了什么模型；
> 3) 代码里每个类和每个方法具体在做什么。

---

## 1. 现实中的 FPGA 布线问题是什么？

### 1.1 一句话定义

FPGA 布线（routing）是在**已经放置好的逻辑块**之间，选择一组实际可用的连线资源，让每条信号都从驱动端连到所有接收端，同时尽量满足时序和资源约束。

### 1.2 工程里真实会发生什么

一个真实的 FPGA 设计流程通常是：

1. 你写 Verilog/VHDL（例如一个图像处理模块）。
2. 综合工具把行为代码变成门级网络（netlist）。
3. 放置（placement）阶段决定每个逻辑块放在芯片哪个位置。
4. 布线（routing）阶段连接这些逻辑块。
5. 时序分析检查时钟频率能不能达到目标。

在第 4 步里，工具要解决的是：

- 每条 net（一个驱动，多个负载）走哪条物理路径。
- 不同 net 不能非法抢同一条独占资源。
- 资源要尽量省，延迟要尽量小。

### 1.3 现实中的例子（不是比喻）

假设你做一个 16 位加法器 + 寄存器的电路：

- 加法器输出 `sum[15:0]` 需要连接到下一级寄存器组。
- 其中某一位信号（比如 `sum[7]`）可能从坐标 `(2,1)` 的逻辑块，连到 `(10,6)` 的寄存器输入。
- 同时它还可能分叉到状态逻辑（例如溢出检测）所在位置。

这时这条信号就是一个 net：

- 一个 source（驱动端）
- 多个 sink（接收端）

路由器要在芯片给定的水平/垂直金属线和开关矩阵中，找出一棵连通树把它们接起来。

---

## 2. 这个作业怎样把现实问题抽象成模型

这个作业做了一个非常经典、但简化过的抽象。

### 2.1 抽象对象

1. FPGA 被建模为 `N x N` 的网格（tile 阵列）。
2. 每个 tile 里有路由资源节点（`RRNode`）：
   - 水平线 `H_WIRE`
   - 垂直线 `V_WIRE`
   - 逻辑引脚 `CB_WIRE`
3. 节点之间可连通关系用无向图边表示（`connect`）。
4. 每条电路信号建模为 `Net`：一个 source + 多个 sink。

### 2.2 关键简化

这个教学模型故意省掉了很多工业复杂度：

- 没有显式时序代价（只看连通性）。
- 每个资源容量近似为 1（通过 `RRNode::net` 是否为空体现）。
- 没有完整 rip-up and reroute（拆线重布）框架。
- 没有工艺参数（电阻电容）参与代价函数。

这正是教学项目的价值：你先把“图搜索 + 资源约束 + 多终端连接”真正搞懂，再上更复杂版本。

---

## 3. 程序整体数据流（从输入到验证）

程序主流程在 `main.cpp`：

1. 读取命令行参数：`./main <circuit_file> <W>`。
2. 读取网表文件第一行 `n`，并设置 `gridSize = n + 1`。
3. 创建 `FPGA fpga(gridSize, W)` 和 `Design design`。
4. 逐行读 net：每行第一个三元组是 source，后续三元组是 sinks。
5. 把 net 加入 `design`。
6. （你需要启用）`MyRouter` 并调用 `routeDesign(fpga, design)`。
7. 调用 `design.verifyRouting()` 检查每条 net 是否从 source 到达所有 sink。

当前模板默认没调用路由器，所以会报：`Routing not complete`。

---

## 4. 每个类代表什么？每个方法做什么？

下面逐类讲解，按“角色 -> 数据 -> 方法”理解。

---

### 4.1 `RRNode`：路由资源图中的一个节点

#### 角色

`RRNode` 是最底层单位，代表一个可被占用的路由资源点。

#### 成员含义

- `type`：节点类型，`H_WIRE` / `V_WIRE` / `CB_WIRE`
- `(x, y)`：所属 tile 坐标
- `idx`：该类型下的编号（track index 或 pin index）
- `connections`：与哪些 `RRNode` 相连（图的邻接表）
- `net`：当前被哪条 net 占用（为空表示可用）

#### 方法逐条解释

- `RRNode(rrType type, int x, int y, int idx)`

  - 构造节点，初始 `net = nullptr`。
- `~RRNode()`

  - 析构函数，无额外逻辑。
- `void connect(RRNode &node)`

  - 建立双向连接。
  - 断言保证不会重复连边。
- `bool isConnected(RRNode &node)`

  - 查询当前节点是否与另一个节点相连。
- `rrType getType()` / `int getX()` / `int getY()` / `int getIdx()`

  - 基础属性 getter。
- `bool isUsed()`

  - `net != nullptr` 即被某条 net 占用。
- `void setNet(Net &net)` / `Net* getNet()`

  - 设置或读取该资源归属。
- `vector<RRNode*>& getConnections()`

  - 返回邻接节点列表，供 BFS/DFS 搜索路径。
- `operator<<`

  - 打印成 `RRNode (x, y).TYPE.idx`，便于调试。

---

### 4.2 `FpgaTile`：一个网格单元（局部路由资源容器）

#### 角色

一个 tile 里保存该位置的所有路由资源节点，以及与相邻 tile 的关系。

#### 成员含义

- 坐标：`x, y`
- 轨道数：`W`
- 邻居指针：`left/right/up/down`
- `logicPin`：4 个逻辑引脚（索引 1..4）
- `vWires` / `hWires`：本 tile 的垂直/水平资源
- `rrNodes`：该 tile 拥有的全部 `RRNode`

#### 方法逐条解释

- `FpgaTile(int x, int y, int W)`

  - 只保存坐标和参数，邻居先置空。
- `~FpgaTile()`

  - 释放本 tile 创建的全部 `RRNode`。
- `getX()/getY()/getRRNodes()`

  - 基础访问接口。
- 邻居 `get/set`：`getLeft`, `setLeft`, `getRight`, `setRight`, `getUp`, `setUp`, `getDown`, `setDown`

  - 在 `FPGA` 构造时由全局建立邻接关系。
- `void generateContents()`

  - 生成本 tile 内部资源：
    - 若存在 `down`，生成 `W` 条 `V_WIRE`
    - 若存在 `right`，生成 `W` 条 `H_WIRE`
    - 若同时存在 `down && right`，生成 4 个 `CB_WIRE`（逻辑引脚）
  - 并建立 connection box：
    - 每条 `V_WIRE` 连到 pin1/pin2
    - 每条 `H_WIRE` 连到 pin3/pin4
- `void populateSwitchbox()`

  - 建立 tile 与邻居之间的 switchbox 连接。
  - 包括直连（同方向 track）和若干转弯连接（按取模规则映射 track index）。
- `RRNode& getLogicPin(int idx)`

  - 获取引脚节点。索引必须已存在，否则断言失败。
- `RRNode& getVWire(int idx)` / `RRNode& getHWire(int idx)`

  - 按 track index 访问线资源，越界会断言。

#### 非常重要的边界规则

只有 `down && right` 的 tile 才有逻辑引脚。

也就是说在这个模型里，最右一列和最下一行通常没有 `CB_WIRE`。因此输入网表里的逻辑引脚坐标必须避开这些边界，否则会触发断言。

---

### 4.3 `FPGA`：整个芯片网格与资源图的拥有者

#### 角色

顶层容器，负责构建所有 tile 及其连接关系。

#### 成员含义

- `N`：网格大小
- `W`：通道宽度（每个方向的 track 数）
- `tileMap[x][y]`：坐标到 tile 的映射
- `tiles`：平铺数组，方便遍历

#### 方法逐条解释

- `FPGA(int gridSize, int W)`

  - 四步完成建图：
    1) 创建全部 tile
    2) 连接 tile 邻居指针
    3) 每个 tile 调 `generateContents()`
    4) 每个 tile 调 `populateSwitchbox()`
- `~FPGA()`

  - 释放全部 tile。
- `FpgaTile& getTile(int x, int y)`

  - 坐标访问。
- `vector<FpgaTile*>& getTiles()`

  - 全量遍历入口。
- `int getN()` / `int getW()`

  - 参数 getter。
- `int getNumSegmentsUsed()`

  - 统计全图 `RRNode::isUsed()==true` 的数量。
  - 注意这统计的是“被占用节点数”，不是几何长度。

---

### 4.4 `Net`：一条待连接信号（source + sinks）

#### 角色

描述“我要连哪些点”，并保存“我最终用了哪些资源”。

#### 成员含义

- `source`：源引脚（引用）
- `idx`：net 编号
- `sinks`：目标引脚集合
- `usedRRs`：此 net 选择的路径节点集合

#### 方法逐条解释

- `Net(RRNode &source, int idx)`

  - 设定源点和编号。
- `~Net()`

  - 空析构。
- `void addSink(RRNode &dest)`

  - 加入一个 sink，断言保证不重复。
- `RRNode& getSource()` / `set<RRNode*>& getSinks()`

  - 访问源和汇。
- `void clearPath()`

  - 清空 `usedRRs`，用于重试。
- `void addRRToPath(RRNode &node)`

  - 把节点记录到本 net 路径集合。
- `set<RRNode*>& getPath()`

  - 获取已记录路径。
- `int getIdx()`

  - 获取 net 编号。
- `void finalizeRouting()`

  - 把 `usedRRs` 中每个节点的 `net` 指针设置为当前 net。
  - 这一步是“提交占用”。
- `bool verifyRouting()`

  - 从 source 出发，只沿“`getNet()==this`”的边做 BFS。
  - 检查是否到达所有 sinks。
  - 若 source 本身未标记为本 net，会立即失败。

---

### 4.5 `Design`：这次作业的 net 集合与验证入口

#### 角色

`Design` 不拥有 FPGA 结构，它只管理所有 net，并统一做验证。

#### 成员

- `vector<Net*> nets`

#### 方法逐条解释

- `Design()` / `~Design()`

  - 构造/析构，析构里释放 `nets`。
- `void addNet(Net &net)`

  - 添加 net 之前做合法性检查：
    - net 必须至少有一个 sink
    - source 不能和已有 net 的 source/sink 冲突
    - sink 不能和已有 net 的 source/sink 冲突
- `int getNumNets()` / `Net& getNet(int idx)` / `vector<Net*>& getNets()`

  - net 集合访问。
- `bool verifyRouting()`

  - 依次调用每条 net 的 `verifyRouting()`。
  - 任意一条失败就返回 `false`。

---

### 4.6 `Router` 与 `MyRouter`：你要实现的算法接口

#### 角色

- `Router`：抽象基类，定义统一接口 `routeDesign(FPGA&, Design&)`。
- `MyRouter`：你的具体实现类。

#### 方法

- `virtual void routeDesign(FPGA&, Design&) = 0`

  - 所有路由器必须实现。
- `void MyRouter::routeDesign(FPGA&, Design&)`

  - 当前模板为空；你要在这里实现算法。

---

### 4.7 其他文件

- `main.cpp`

  - 程序入口、输入解析、创建对象、调用路由、最终验证。
  - 当前路由调用被注释，需要你开启。
- `Makefile`

  - `g++ -std=c++17` 编译所有 `.cpp`，产出可执行文件 `main`。
- `benchmark/*`

  - 测试网表输入样例。

---

## 5. 输入文件怎么映射到对象（以 tiny 为例）

`benchmark/tiny` 第一行是 `4`，代码里会变成 `gridSize = 5`。

然后每行 net 形如：

`x y p x y p x y p ...`

- 第一个三元组是 source。
- 后面每个三元组是一个 sink。

例如这一行：

`2 1 4 1 1 2`

表示：

- source = tile `(2,1)` 的 pin `4`
- sink = tile `(1,1)` 的 pin `2`

程序会把它构造成：

1. `Net* net = new Net(fpga.getTile(2,1).getLogicPin(4), idx)`
2. `net->addSink(fpga.getTile(1,1).getLogicPin(2))`
3. `design.addNet(*net)`

最后遇到 `-1 -1 -1 ...` 表示结束。

---

## 6. 你要实现的核心算法：从“可验证通过”开始

最推荐的起步方案：

1. 按 net 顺序路由。
2. 对每个 net：
   - 先把 source 加入路径集合。
   - 维护一个“已连通树”集合（初始只含 source）。
3. 对 net 的每个 sink：
   - 从“已连通树”出发做 BFS，找一条到该 sink 的可行路径。
   - 可行条件：节点未被其他 net 占用，或已被当前 net 占用。
4. 回溯路径，把路径节点加入 `usedRRs` 和“已连通树”。
5. 该 net 所有 sink 完成后，调用 `finalizeRouting()` 提交占用。

### 6.1 为什么这样能通过 `verifyRouting`

`verifyRouting` 的判定条件是：

- source 节点的 `getNet()` 必须是当前 net。
- 从 source 沿同 net 节点能到达全部 sinks。

所以你至少要保证：

- source 在 `usedRRs` 里（否则 `finalizeRouting` 不会标 source）。
- 每次连 sink 时接到已有树上，而不是孤立路径。

### 6.2 常见失败点

1. 忘记把 source 加入路径。
2. 找到 path 但没 `addRRToPath`。
3. 忘记调用 `finalizeRouting`。
4. 没处理“当前 net 允许复用自己已有节点”。
5. 输入用了边界 tile 的逻辑 pin，触发断言。

---

## 7. 这份作业与经典论文/工具的对应关系

### 7.1 经典论文脉络（你可以当作进阶阅读路线）

1. **Lee, 1961**

   - *An Algorithm for Path Connections and Its Applications*
   - DOI: `10.1109/TEC.1961.5219222`
   - 贡献：波前扩展 + 回溯的迷宫寻路思想（BFS 类）。
2. **McMurchie & Ebeling, 1995 (PathFinder)**

   - *PathFinder: A Negotiation-Based Performance-Driven Router for FPGAs*
   - DOI: `10.1145/201310.201328`（ACM 记录）
   - 贡献：协商拥塞（negotiated congestion），通过反复 rip-up/reroute 让冲突逐轮收敛。
3. **Betz & Rose, 1997 (VPR)**

   - *VPR: a new packing, placement and routing tool for FPGA research*
   - DOI: `10.1007/3-540-63465-7_226`
   - 贡献：把学术 FPGA CAD 的 P&R 流程系统化。
4. **Rose et al., 2012 (VTR project)**

   - *The VTR project: architecture and CAD for FPGAs from verilog to routing*
   - DOI: `10.1145/2145694.2145708`
5. **Elgammal et al., 2025 (VTR 9)**

   - *VTR 9: Open-Source CAD for Fabric and Beyond FPGA Architecture Exploration*
   - 说明现代开源 FPGA CAD 平台持续演进。

### 7.2 开源实现对照

1. **VTR/VPR**

   - 学术界最经典开放实现。
   - 文档明确使用 RR Graph 表示路由资源，路由输出含 `SOURCE/OPIN/CHANX/CHANY/IPIN/SINK`。
2. **nextpnr**

   - 面向多种真实 FPGA 架构的开源 P&R 工具。
   - README 明确定位为 vendor-neutral、timing-driven 的 place-and-route。

### 7.3 你的作业在这个谱系中的位置

这份 lab3 可以看成“PathFinder/VPR 思想的教学切片”：

- 你已经有 RR graph（`RRNode` + connections）。
- 你需要完成基础路由决策（`MyRouter`）。
- 你用连通性验证替代了工业级全量 QoR 指标。

这是非常标准的入门路径。

---

## 8. 对 0 基础同学的实操学习路线（建议按顺序）

1. 先跑通 `make` 和 `./main ./benchmark/tiny 12`，观察未路由失败信息。
2. 在 `main.cpp` 里启用 `MyRouter` 调用。
3. 在 `Solution.cpp` 先做“单 net、单 sink”的 BFS。
4. 扩展到“单 net、多 sink”（构造一棵路由树）。
5. 扩展到“多 net 顺序路由”。
6. 打印每条 net 的路径节点，和 `verifyRouting` 报错逐条对照。
7. 比较不同 `W` 下成功率和 `Segments used`。

---

## 9. 你现在最该记住的三句话

1. 这个作业本质是“在图上做受资源约束的多终端连接”。
2. `RRNode` 是资源，`Net` 是需求，`MyRouter` 是决策器。
3. 只要每条 net 的 source 到所有 sink 在同一连通子图里，`verifyRouting` 就能通过。

---

## 10. 参考资料（可继续深入）

1. C. Y. Lee, *An Algorithm for Path Connections and Its Applications*, 1961, DOI: `10.1109/TEC.1961.5219222`
2. L. McMurchie, C. Ebeling, *PathFinder: A Negotiation-Based Performance-Driven Router for FPGAs*, FPGA'95, DOI: `10.1145/201310.201328`
3. V. Betz, J. Rose, *VPR: a new packing, placement and routing tool for FPGA research*, 1997, DOI: `10.1007/3-540-63465-7_226`
4. J. Rose et al., *The VTR project: architecture and CAD for FPGAs from verilog to routing*, 2012, DOI: `10.1145/2145694.2145708`
5. VTR 官方文档（RR graph、routing file、basic flow）：https://docs.verilogtorouting.org/
6. nextpnr 项目主页：https://github.com/YosysHQ/nextpnr

---

如果你愿意，我下一步可以直接给你补一版“可通过基础样例”的 `MyRouter::routeDesign`（BFS 版本），并在代码里加必要的调试输出与错误定位信息。
