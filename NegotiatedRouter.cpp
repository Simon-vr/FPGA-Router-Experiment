/**
 * @file NegotiatedRouter.cpp
 * @brief PathFinder 协商布线算法实现
 * 
 * 基于 McMurchie & Ebeling, FPGA'95 论文实现
 * 核心思想：通过迭代式的 Rip-up/Reroute 和协商代价函数解决拥塞问题
 */

#include "Solution.h"
#include "FPGA.h"
#include "FpgaTile.h"
#include "Design.h"
#include "Net.h"
#include "RRNode.h"
#include "vistual/VisualizationDump.h"
#include <unordered_map>
#include <unordered_set>
#include <iostream>
#include <queue>
#include <functional>
#include <vector>
#include <algorithm>
#include <stdexcept>
#include <string>
#include <iomanip>
#include <chrono>
#include <omp.h>
#include <cmath>

// ============================================================================
// 常量定义
// ============================================================================

namespace {
    // 代价函数参数
    constexpr float BASE_COST = 1.0f;           // 基准代价
    constexpr float PRESENT_SHARING_PENALTY = 0.5f;  // 当前共享惩罚系数
    constexpr float ITERATION_FACTOR_BASE = 1.0f;    // 迭代因子基础值
    constexpr float ITERATION_FACTOR_INC = 0.1f;     // 每轮迭代增加的系数
    
    // 拥塞阈值（在当前简化模型中，容量为1，超过1即为拥塞）
    constexpr int CAPACITY = 1;
}

// ============================================================================
// 主路由流程
// ============================================================================

void NegotiatedRouter::routeDesign(FPGA &fpga, Design &design) {
    using namespace std::chrono;
    auto startTime = steady_clock::now();
    
    std::vector<Net*>& nets = design.getNets();
    const int numNets = design.getNumNets();
    
    // 设置线程数
    omp_set_num_threads(numThreads_);
    
    std::cout << "\n" << std::string(70, '=') << std::endl;
    std::cout << "[Negotiated Router] PathFinder Algorithm Started" << std::endl;
    std::cout << "                   OpenMP Parallel Version (" << numThreads_ << " threads)" << std::endl;
    std::cout << std::string(70, '=') << std::endl;
    std::cout << "Total nets to route: " << numNets << std::endl;
    std::cout << "Max iterations: " << maxIterations_ << std::endl;
    std::cout << std::endl;
    
    // 初始化临时路径存储
    tempPaths_.resize(numNets);
    
    // 初始化可视化输出
    RoutingStateDumper* stateDumper = nullptr;
    if (enableDumping_) {
        stateDumper = new RoutingStateDumper("vistual/" + outputDir);
        stateDumper->dumpStep(fpga, design, "initial");
    }
    
    // 初始化：重置所有历史代价
    resetAllHistoricalCosts(fpga);
    
    // ============================================================================
    // 主迭代循环 (Rip-up / Reroute) - OpenMP 并行版本
    // ============================================================================
    for (int iteration = 0; iteration < maxIterations_; ++iteration) {
        // Step 1: Rip-up 所有net的布线
        ripupAllNets(nets);
        
        // Step 2: 重置所有节点的当前占用计数
        resetAllOccupancy(fpga);
        
        // 清空临时路径存储
        for (auto& path : tempPaths_) {
            path.clear();
        }
        
        // Step 3: 并行路由所有net (Phase A)
        totalSuccessfulRoutes_ = 0;
        totalFailedRoutes_ = 0;
        
        #pragma omp parallel for schedule(dynamic)
        for (int i = 0; i < numNets; ++i) {
            Net* net = nets[i];
            
            // 线程本地路径存储
            std::vector<RRNode*> localPath;
            localPath.reserve(100);
            
            // 使用MST分解多终端net
            std::set<NodePair> pinPairs = disassembleMST(*net);
            
            bool allSuccess = true;
            for (const NodePair &pair : pinPairs) {
                bool success = negotiatedAStarParallel(*net, pair.first, pair.second,
                                                       iteration, localPath);
                if (!success) {
                    allSuccess = false;
                    #pragma omp critical
                    {
                        std::cout << "[WARNING] Failed to route connection: "
                                  << *pair.first << " -> " << *pair.second
                                  << " (Net " << net->getIdx() << ")" << std::endl;
                    }
                }
            }
            // 写入线程对应的位置（每个Net只被一个线程写入，无竞争）
            tempPaths_[i] = std::move(localPath);
            /*
            if (allSuccess) {
                #pragma omp atomic
                ++totalSuccessfulRoutes_;
            } else {
                #pragma omp atomic
                ++totalFailedRoutes_;
            }
            */
        }
        // 隐式 barrier：所有线程完成路由
        
        // Step 4: 串行提交阶段 (Phase B)
        for (int i = 0; i < numNets; ++i) {
            Net* net = nets[i];
            // 批量设置路径
            net->setPath(tempPaths_[i]);
            // 清空并保存此次迭代的占用快照到RRNode
            for (RRNode* node : tempPaths_[i]) {
                node->clearPrevIterNets();
                node->addPrevIterNet(net);
            }
            // 提交到全局资源占用表
            net->finalizeRouting();
        }
        
        // Step 5: 记录当前资源节点占用情况（串行）
        recordOccupancy(design);
        
        // Step 6: 打印当前迭代统计
        printIterationStats(iteration, fpga);
        
        // Step 7: 可视化输出
        if (enableDumping_ && stateDumper) {
            stateDumper->dumpStep(fpga, design, "iter_" + std::to_string(iteration + 1));
        }
        
        // Step 8: 检查是否收敛（无拥塞）
        if (!checkCongestion(fpga)) {
            convergenceIteration_ = iteration + 1;
            auto endTime = steady_clock::now();
            auto duration = duration_cast<seconds>(endTime - startTime);
            
            std::cout << "\n" << std::string(70, '=') << std::endl;
            std::cout << "[SUCCESS] Routing converged at iteration "
                      << convergenceIteration_ << std::endl;
            std::cout << "Time elapsed: " << duration.count() << " seconds" << std::endl;
            std::cout << std::string(70, '=') << std::endl;
            
            printFinalStats();
            
            if (stateDumper) {
                stateDumper->dumpStep(fpga, design, "final_converged");
                delete stateDumper;
            }
            return;
        }
        
        // Step 9: 更新历史代价（串行）
        updateHistoricalCosts(fpga);
    }
    
    // 达到最大迭代次数仍未收敛
    auto endTime = steady_clock::now();
    auto duration = duration_cast<seconds>(endTime - startTime);
    
    std::cout << "\n" << std::string(70, '=') << std::endl;
    std::cout << "[WARNING] Failed to converge after " << maxIterations_
              << " iterations" << std::endl;
    std::cout << "Time elapsed: " << duration.count() << " seconds" << std::endl;
    std::cout << std::string(70, '=') << std::endl;
    
    printFinalStats();
    
    if (stateDumper) {
        stateDumper->dumpStep(fpga, design, "final_unconverged");
        delete stateDumper;
    }
}

// ============================================================================
// 代价计算
// ============================================================================

float NegotiatedRouter::calculateNodeCost(RRNode* node, Net* currentNet, int iteration) const {
    // 1. 基准代价
    float baseCost = BASE_COST;
    
    // 2. 历史代价：该节点在所有前序迭代中被使用的累积
    //    这是并行安全的，因为 historicalCost 在本轮迭代中是只读的
    float hCost = node->getHistoricalCost();
    
    // 3. 上一轮迭代的共享代价：如果节点上一轮被其他net占用，增加惩罚
    float pCost = 1.0f;
    
    // 检查上一轮迭代该节点是否被其他net占用
    if (node->isUsedByOtherInPrevIter(currentNet)) {
        // 节点上一轮被其他net占用，应用共享惩罚
        // 随着迭代增加，惩罚力度加大
        float over = node->getPrevIterNets().size() - CAPACITY; // 超过容量的net数量
        // pCost = 1.0f + PRESENT_SHARING_PENALTY * (1 + over * 0.1f);
        // pCost = 1.0f + PRESENT_SHARING_PENALTY * (over + 0.2f * over * over);收敛不了
        pCost = 1.0f + PRESENT_SHARING_PENALTY * pow(over + 1, 2); // 二次惩罚，增加收敛速度
    }
    // 如果上一轮该节点未被占用，或仅被当前net自己占用，不增加惩罚
    
    // 4. 迭代因子：随着迭代增加，对拥塞的惩罚力度加大
    float iterationFactor = ITERATION_FACTOR_BASE + ITERATION_FACTOR_INC * iteration;
    
    // 总代价 = (基准 + 历史) * 上一轮共享 * 迭代因子
    float totalCost = (baseCost + hCost) * pCost * iterationFactor;
    
    return totalCost;
}

int NegotiatedRouter::measureManhattan(RRNode *a, RRNode *b) const {
    return std::abs(a->getX() - b->getX()) + std::abs(a->getY() - b->getY());
}

// ============================================================================
// MST分解 (Kruskal算法)
// ============================================================================

std::set<NegotiatedRouter::NodePair> NegotiatedRouter::disassembleMST(Net &net) const {
    // 收集所有引脚（source + sinks）
    std::vector<RRNode*> pins;
    pins.push_back(&net.getSource());
    for (RRNode *sink : net.getSinks()) {
        pins.push_back(sink);
    }
    
    const int numPins = static_cast<int>(pins.size());
    if (numPins <= 1) {
        return std::set<NodePair>();
    }
    
    // 生成所有可能的边
    std::vector<std::pair<int, int>> edges;
    edges.reserve((numPins * (numPins - 1)) / 2);
    for (int i = 0; i < numPins; ++i) {
        for (int j = i + 1; j < numPins; ++j) {
            edges.push_back({i, j});
        }
    }
    
    // 按曼哈顿距离排序边
    std::sort(edges.begin(), edges.end(), [&](const std::pair<int, int> &a, 
                                              const std::pair<int, int> &b) {
        int wa = measureManhattan(pins[a.first], pins[a.second]);
        int wb = measureManhattan(pins[b.first], pins[b.second]);
        if (wa != wb) return wa < wb;
        if (a.first != b.first) return a.first < b.first;
        return a.second < b.second;
    });
    
    // 并查集初始化
    std::vector<int> parent(numPins);
    std::vector<int> rank(numPins, 0);
    for (int i = 0; i < numPins; ++i) {
        parent[i] = i;
    }
    
    std::function<int(int)> findRoot = [&](int x) -> int {
        if (parent[x] != x) {
            parent[x] = findRoot(parent[x]);
        }
        return parent[x];
    };
    
    auto unite = [&](int a, int b) -> bool {
        int ra = findRoot(a);
        int rb = findRoot(b);
        if (ra == rb) return false;
        
        if (rank[ra] < rank[rb]) std::swap(ra, rb);
        parent[rb] = ra;
        if (rank[ra] == rank[rb]) ++rank[ra];
        return true;
    };
    
    // Kruskal算法构建MST
    std::set<NodePair> mstEdges;
    for (const auto &e : edges) {
        if (!unite(e.first, e.second)) continue;
        
        RRNode *a = pins[e.first];
        RRNode *b = pins[e.second];
        if (b < a) std::swap(a, b);  // 确保有序
        mstEdges.insert({a, b});
        
        if (static_cast<int>(mstEdges.size()) == numPins - 1) break;
    }
    
    return mstEdges;
}

// ============================================================================
// 协商A*搜索算法
// ============================================================================

bool NegotiatedRouter::negotiatedAStar(Net &net, RRNode* source, RRNode* sink, int iteration) {
    // 边界情况：源点和汇点相同
    if (*source == *sink) {
        net.addRRToPath(*source);
        return true;
    }
    
    // A*数据结构
    std::unordered_map<RRNode*, RRNode*> parent;    // 父节点映射，用于回溯路径
    std::unordered_map<RRNode*, float> gScore;      // 从起点到当前节点的实际代价
    std::unordered_set<RRNode*> closedSet;          // 已处理节点集合
    
    // 优先队列：(f_score, node)，按f_score升序排列
    using PQItem = std::pair<float, RRNode*>;
    std::priority_queue<PQItem, std::vector<PQItem>, std::greater<PQItem>> openSet;
    
    // 初始化
    parent[source] = source;
    gScore[source] = 0.0f;
    
    // f = g + h，其中g使用协商代价，h使用曼哈顿距离
    float f0 = calculateNodeCost(source, &net, iteration) + measureManhattan(source, sink);
    openSet.emplace(f0, source);
    
    while (!openSet.empty()) {
        RRNode *current = openSet.top().second;
        openSet.pop();
        
        // 跳过已处理的节点
        if (closedSet.count(current)) continue;
        closedSet.insert(current);
        
        // 到达目标
        if (*current == *sink) {
            // 回溯路径并添加到net
            net.addRRToPath(*current);
            while (parent[current] != current) {
                current = parent[current];
                net.addRRToPath(*current);
            }
            return true;
        }
        
        // 扩展邻居
        for (RRNode *neighbor : current->getConnections()) {
            // 跳过已处理的节点
            if (closedSet.count(neighbor)) continue;
            
            // 计算通过当前节点到达邻居的代价
            float edgeCost = calculateNodeCost(neighbor, &net, iteration);
            float tentativeG = gScore[current] + edgeCost;
            
            // 如果找到更优路径，更新
            auto it = gScore.find(neighbor);
            if (it == gScore.end() || tentativeG < it->second) {
                parent[neighbor] = current;
                gScore[neighbor] = tentativeG;
                float f = tentativeG + measureManhattan(neighbor, sink);
                openSet.emplace(f, neighbor);
            }
        }
    }
    
    // 搜索失败，无路径可达
    return false;
}

// ============================================================================
// 并行安全的协商A*搜索
// ============================================================================

bool NegotiatedRouter::negotiatedAStarParallel(Net &net, RRNode* source, RRNode* sink,
                                                int iteration,
                                                std::vector<RRNode*>& resultPath) {
    if (*source == *sink) {
        resultPath.push_back(source);
        return true;
    }
    
    // 使用线程本地存储，避免频繁内存分配
    thread_local std::unordered_map<RRNode*, RRNode*> parent;
    thread_local std::unordered_map<RRNode*, float> gScore;
    thread_local std::unordered_set<RRNode*> closedSet;
    
    // 每次使用前清空
    parent.clear();
    gScore.clear();
    closedSet.clear();
    
    using PQItem = std::pair<float, RRNode*>;
    std::priority_queue<PQItem, std::vector<PQItem>, std::greater<PQItem>> openSet;
    
    parent[source] = source;
    gScore[source] = 0.0f;
    
    float f0 = calculateNodeCost(source, &net, iteration) + measureManhattan(source, sink);
    openSet.emplace(f0, source);
    
    while (!openSet.empty()) {
        RRNode *current = openSet.top().second;
        openSet.pop();
        
        if (closedSet.count(current)) continue;
        closedSet.insert(current);
        
        if (*current == *sink) {
            // 回溯路径
            std::vector<RRNode*> reversePath;
            RRNode* node = current;
            while (parent[node] != node) {
                reversePath.push_back(node);
                node = parent[node];
            }
            reversePath.push_back(source);
            
            // 反转并添加到结果
            resultPath.insert(resultPath.end(), reversePath.rbegin(), reversePath.rend());
            return true;
        }
        
        for (RRNode *neighbor : current->getConnections()) {
            if (closedSet.count(neighbor)) continue;
            
            float edgeCost = calculateNodeCost(neighbor, &net, iteration);
            float tentativeG = gScore[current] + edgeCost;
            
            auto it = gScore.find(neighbor);
            if (it == gScore.end() || tentativeG < it->second) {
                parent[neighbor] = current;
                gScore[neighbor] = tentativeG;
                float f = tentativeG + measureManhattan(neighbor, sink);
                openSet.emplace(f, neighbor);
            }
        }
    }
    
    return false;
}

// ============================================================================
// 迭代控制
// ============================================================================

void NegotiatedRouter::ripupAllNets(std::vector<Net*> &nets) {
    for (Net* net : nets) {
        // 清除该net使用的所有RRNode的net指针
        for (RRNode* node : net->getPath()) {
            node->clearNet();
        }
        net->clearPath();
    }
}

void NegotiatedRouter::resetAllOccupancy(FPGA &fpga) {
    for (auto* tile : fpga.getTiles()) {
        for (RRNode* node : tile->getRRNodes()) {
            node->resetOccupancy();
        }
    }
}



void NegotiatedRouter::resetAllHistoricalCosts(FPGA &fpga) {
    for (auto* tile : fpga.getTiles()) {
        for (RRNode* node : tile->getRRNodes()) {
            node->resetHistoricalCost();
        }
    }
}

void NegotiatedRouter::updateHistoricalCosts(FPGA &fpga) {
    for (auto* tile : fpga.getTiles()) {
        for (RRNode* node : tile->getRRNodes()) {
            // 只要节点在当前迭代中被使用过，历史代价就增加
            if (node->getOccupancy() > CAPACITY) {
                node->incrementHistoricalCost(node->getOccupancy()-1);
            }
        }
    }
}

bool NegotiatedRouter::checkCongestion(FPGA &fpga) const {
    for (auto* tile : fpga.getTiles()) {
        for (RRNode* node : tile->getRRNodes()) {
            // 在当前简化模型中，每个节点容量为1
            // 超过1个net占用即为拥塞
            if (node->getOccupancy() > CAPACITY) {
                return true;  // 存在拥塞
            }
        }
    }
    return false;  // 无拥塞
}

std::pair<int, int> NegotiatedRouter::getCongestionStats(FPGA &fpga) const {
    int congestedNodes = 0;  // 拥塞节点数
    int totalOverflow = 0;   // 总溢出量
    
    for (auto* tile : fpga.getTiles()) {
        for (RRNode* node : tile->getRRNodes()) {
            int occ = node->getOccupancy();
            if (occ > CAPACITY) {
                ++congestedNodes;
                totalOverflow += (occ - CAPACITY);
            }
        }
    }
    
    return {congestedNodes, totalOverflow};
}

void NegotiatedRouter::recordOccupancy(Design &design) {
    // 遍历所有net的路径，统计每个节点的占用次数
    for (Net* net : design.getNets()) {
        for (RRNode* node : net->getPath()) {
            node->incrementOccupancy();
        }
    }
}

// ============================================================================
// 调试输出
// ============================================================================

void NegotiatedRouter::printIterationStats(int iteration, FPGA &fpga) const {
    std::cout << "\n--- Iteration " << std::setw(2) << iteration + 1 << "/" 
                  << maxIterations_ << " ---" << std::endl;
        
    auto [congestedNodes, totalOverflow] = getCongestionStats(fpga);
    int segmentsUsed = fpga.getNumSegmentsUsed();
    
    std::cout //<< "  Routes: " << std::setw(4) << totalSuccessfulRoutes_ << " succeeded, "
              //<< std::setw(3) << totalFailedRoutes_ << " failed | "
              << "Segments: " << std::setw(5) << segmentsUsed << " | "
              << "Congestion: " << std::setw(4) << congestedNodes << " nodes, "
              << std::setw(4) << totalOverflow << " overflow";
    
    if (congestedNodes == 0) {
        std::cout << " [OK]";
    }
    std::cout << std::endl;
}

void NegotiatedRouter::printFinalStats() const {
    std::cout << "\n" << std::string(70, '-') << std::endl;
    std::cout << "Final Statistics:" << std::endl;
    std::cout << "  Total successful routes: " << totalSuccessfulRoutes_ << std::endl;
    std::cout << "  Total failed routes: " << totalFailedRoutes_ << std::endl;
    if (convergenceIteration_ > 0) {
        std::cout << "  Converged at iteration: " << convergenceIteration_ << std::endl;
    } else {
        std::cout << "  Did not converge within max iterations" << std::endl;
    }
    std::cout << std::string(70, '-') << std::endl;
}
