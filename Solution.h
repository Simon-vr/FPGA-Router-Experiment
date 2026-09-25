/**
 * @file Solution.h
 * @brief 路由器接口与两种具体实现。
 *
 * Router 为抽象基类；MyRouter 为顺序详细布线路由器：
 *  - routeDesign(): 依次处理每个 net，先用 disassemble_mst() 将多端 net
 *    拆成 MST 边集，再用 singleroute_bfs/astar/mikami 对每对引脚布线；
 *  - 运行时通过构造参数 method_ 选择详细布线方法（bfs | astar | mikami）；
 *  - visual_ 为 true 时通过 RoutingStateDumper 逐步导出 JSON（本次实验禁止打开）。
 *
 * NegotiatedRouter 为 PathFinder 协商布线路由器（详见类内注释）。
 */
#ifndef SOLUTION_H
#define SOLUTION_H

class FPGA;
class Design;
class Net;
class RRNode;
#include <set>   
#include <utility>  
#include <vector>
#include <string>

class Router {
public:
    Router(){}
    virtual ~Router(){}
    virtual void routeDesign(FPGA &fpga, Design &design) = 0;
};

// MyRouter：顺序全局/详细布线器
//  - method_ = "bfs"   : BFS 最短跳数（兼容旧 router_type "my"）
//  - method_ = "astar" : A*，启发函数为 Manhattan 距离
//  - method_ = "mikami": Mikami-Tabuchi 按节点类型扩展的双端线搜索
class MyRouter:public Router{
public:
    explicit MyRouter(std::string outputDir = "vistual/results_sequence_bfs",
                      std::string method = "bfs",
                      bool visual = false)
        : outputDir(std::move(outputDir)), method_(std::move(method)), visual_(visual) {}
    virtual ~MyRouter(){}
    void routeDesign(FPGA &fpga, Design &design);
private: 
    using NodePair = std::pair<RRNode*, RRNode*>;
    int measure_manhattan(RRNode *a, RRNode *b);

    void sortNets(std::vector<Net*> &nets);
    
    std::set<NodePair> disassemble_mst(Net &net);

    bool singleroute_bfs(Net &net, RRNode *source, RRNode *sink);
    bool singleroute_Astar(Net &net, RRNode *source, RRNode *sink);
    bool singleroute_mikami(Net &net, RRNode *source, RRNode *sink);
    Net* ripup_net(RRNode *obstacle);
    std::string outputDir;
    std::string method_;
    bool visual_;
};

// ============================================================================
// 协商布线路由器 (Negotiated Router)
// 基于 PathFinder 算法实现，支持迭代式的拆线重布以解决拥塞问题
// ============================================================================

/**
 * @class NegotiatedRouter
 * @brief 实现 PathFinder 协商布线算法的路由器
 *
 * 协商布线的核心思想：
 * 1. 允许多个net在迭代过程中临时共享资源
 * 2. 通过历史代价和共享惩罚引导后续迭代避开拥塞区域
 * 3. 最终达到无冲突的全局最优解
 */
class NegotiatedRouter : public Router {
public:
    /**
     * @brief 构造函数
     * @param outputDir 可视化输出目录
     * @param maxIterations 最大迭代次数，默认30
     * @param enableDumping 是否启用可视化输出，默认false
     * @param numThreads 并行线程数，默认16
     */
    explicit NegotiatedRouter(std::string outputDir = "results_negotiated",
                              int maxIterations = 30,
                              bool enableDumping = false,
                              int numThreads = 16)
        : maxIterations_(maxIterations),
          enableDumping_(enableDumping),
          outputDir(std::move(outputDir)),
          numThreads_(numThreads) {}
        
    
    virtual ~NegotiatedRouter() = default;
    
    /**
     * @brief 执行协商布线
     * @param fpga FPGA芯片对象
     * @param design 设计网表
     */
    void routeDesign(FPGA &fpga, Design &design) override;

private:
    using NodePair = std::pair<RRNode*, RRNode*>;
    
    // 算法参数
    const int maxIterations_;       // 最大迭代次数
    const bool enableDumping_;      // 是否启用可视化
    std::string outputDir;          // 输出目录
    int numThreads_;                // 线程数
    
    // 统计信息
    int totalSuccessfulRoutes_ = 0; // 成功布线的连接数
    int totalFailedRoutes_ = 0;     // 失败的连接数
    int convergenceIteration_ = -1; // 收敛时的迭代次数
    
    // OpenMP 并行化相关
    std::vector<std::vector<RRNode*>> tempPaths_;             // 临时路径存储
    
    // ============================================================================
    // 代价计算相关
    // ============================================================================
    
    /**
     * @brief 计算节点的协商代价
     *
     * 代价公式：cost = (baseCost + historicalCost) * presentSharingCost * iterationFactor
     *
     * @param node 目标节点
     * @param currentNet 当前布线的net
     * @param iteration 当前迭代次数（用于调整惩罚强度）
     * @return float 节点的总代价
     */
    float calculateNodeCost(RRNode* node, Net* currentNet, int iteration) const;
    
    /**
     * @brief 计算曼哈顿距离启发函数
     * @param a 起点
     * @param b 终点
     * @return int 曼哈顿距离
     */
    int measureManhattan(RRNode *a, RRNode *b) const;
    
    // ============================================================================
    // MST分解（复用MyRouter的实现）
    // ============================================================================
    
    /**
     * @brief 使用Kruskal算法将多终端net分解为MST边集
     * @param net 目标net
     * @return std::set<NodePair> MST边集
     */
    std::set<NodePair> disassembleMST(Net &net) const;
    
    // ============================================================================
    // 路由算法
    // ============================================================================
    
    /**
     * @brief 协商A*搜索算法
     *
     * 特点：
     * - 使用协商代价代替固定边权
     * - 允许临时通过已被占用的节点（代价更高）
     * - 支持多条net共享资源时的动态调整
     *
     * @param net 当前布线的net
     * @param source 源节点
     * @param sink 目标节点
     * @param iteration 当前迭代次数
     * @return true 布线成功
     * @return false 布线失败
     */
    bool negotiatedAStar(Net &net, RRNode* source, RRNode* sink, int iteration);
    
    /**
     * @brief 并行安全的协商A*搜索
     * @param resultPath 输出参数：收集路径节点到vector
     * @return true 成功
     */
    bool negotiatedAStarParallel(Net &net, RRNode* source, RRNode* sink,
                                 int iteration, std::vector<RRNode*>& resultPath);
    
    // ============================================================================
    // 迭代控制相关
    // ============================================================================
    
    /**
     * @brief 拆除所有net的布线（Rip-up阶段）
     * @param nets 所有net的列表
     */
    void ripupAllNets(std::vector<Net*> &nets);
    
    /**
     * @brief 重置所有节点的占用计数
     * @param fpga FPGA芯片
     */
    void resetAllOccupancy(FPGA &fpga);
   
    /**
     * @brief 重置所有节点的历史代价
     * @param fpga FPGA芯片
     */
    void resetAllHistoricalCosts(FPGA &fpga);
    
    /**
     * @brief 更新所有节点的历史代价
     *
     * 在每次迭代结束时调用，将当前占用情况累加到历史代价中
     *
     * @param fpga FPGA芯片
     */
    void updateHistoricalCosts(FPGA &fpga);
    
    /**
     * @brief 检查是否存在拥塞
     *
     * 拥塞定义：节点被超过1个net占用（在当前简化模型中容量为1）
     *
     * @param fpga FPGA芯片
     * @return true 存在拥塞
     * @return false 无拥塞（收敛成功）
     */
    bool checkCongestion(FPGA &fpga) const;
    
    /**
     * @brief 获取拥塞统计信息
     * @param fpga FPGA芯片
     * @return std::pair<int, int> (拥塞节点数, 总冲突次数)
     */
    std::pair<int, int> getCongestionStats(FPGA &fpga) const;
    
    /**
     * @brief 记录当前迭代的占用情况
     *
     * 遍历所有net的路径，统计每个节点的占用次数
     *
     * @param design 设计网表
     */
    void recordOccupancy(Design &design);
    
    // ============================================================================
    // 调试与可视化
    // ============================================================================
    
    /**
     * @brief 打印当前迭代统计信息
     * @param iteration 当前迭代次数
     * @param fpga FPGA芯片
     */
    void printIterationStats(int iteration, FPGA &fpga) const;
    
    /**
     * @brief 打印最终统计信息
     */
    void printFinalStats() const;
};

#endif