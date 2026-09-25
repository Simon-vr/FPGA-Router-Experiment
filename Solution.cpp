/**
 * @file Solution.cpp
 * @brief MyRouter（顺序详细布线器）实现。
 *
 * 流程：
 *  1. routeDesign(): 按 net 顺序处理，每个 net 先 disassemble_mst() 拆边，
 *     再根据 method_ 选 singleroute_bfs / _Astar / _mikami 逐对引脚布线，
 *     最后 finalizeRouting() 将路径标记为已占用；
 *  2. sortNets(): 全局布线排序启发（按 bounding box 内全局引脚数升序），
 *     当前调用已被注释，保留供实验参考；
 *  3. disassemble_mst(): Kruskal 最小生成树拆线，边权为 Manhattan 距离；
 *  4. singleroute_*(): 三种双端详细布线算法；
 *  5. ripup_net(): 拆除占用节点的 net（辅助接口，当前未被调用）。
 */
#include "Solution.h"
#include "FPGA.h"
#include "Design.h"
#include "Net.h"
#include "RRNode.h"
#include "vistual/VisualizationDump.h"
#include <unordered_map>
#include <iostream>
#include <queue>
#include <functional>
#include <vector>
#include <algorithm>
#include <stdexcept>
#include <string>

void MyRouter::routeDesign(FPGA &fpga, Design &design) {
    int numNets = design.getNumNets();
    std::vector<Net*>& nets = design.getNets();
    // 预保留所有终端引脚：把 source/sink 提前标记为各自 net 所有，
    // 这样其它 net 不能把本 net 的引脚当作中转节点穿过去。
    // 否则后处理的 net 会在 finalizeRouting() 时覆盖该节点归属，
    // 使先处理 net 的 verifyRouting() 断链（A* 在 huge 上曾因此失败）。
    for (Net *net : nets) {
        net->getSource().setNet(*net);
        for (RRNode *sink : net->getSinks()) {
            sink->setNet(*net);
        }
    }
    RoutingStateDumper* stateDumper = nullptr;
    if (visual_) {
        stateDumper = new RoutingStateDumper("vistual/" + outputDir);
        stateDumper->dumpStep(fpga, design, "initial");
    }
    // 顺序全局布线：按照net的bounding box面积从小到大排序，优先布线面积小的net
    // sortNets(nets);
    long successfulRoutes = 0;
    long totalRoutes = 0;
    for(int i = 0; i < numNets; i++) {
        Net &net = *nets[i];
        /* version 1: single route
        RRNode &source = net.getSource();
        std::set<RRNode *> &sinks = net.getSinks();
        for(RRNode *sink : sinks) {
            //if(singleroute_bfs(net, &source,sink)) {
            if(singleroute_Astar(net, &source,sink)) {
            net.finalizeRouting();
                std::cout << "Successfully routed net " << net.getIdx() << std::endl;
            } else {
                std::cout << "No path found for net " << net.getIdx() << std::endl;
            }
        }
        */
        std::set<NodePair> pinpairs = disassemble_mst(net);
        for (const NodePair &pair : pinpairs) {
            RRNode *source = pair.first;
            RRNode *sink = pair.second;
            bool routed = false;
            if (method_ == "astar") {
                routed = singleroute_Astar(net, source, sink);
            } else if (method_ == "mikami") {
                routed = singleroute_mikami(net, source, sink);
            } else {
                routed = singleroute_bfs(net, source, sink);
            }
            if(routed) {
                successfulRoutes++;
                totalRoutes++;
            } else {
                // std::cout << "No path found for net " << net.getIdx()
                //           << " from (" << source->getX() << "," << source->getY()
                //           << ") to (" << sink->getX() << "," << sink->getY() << ")\n";
                totalRoutes++;
            }
        }
        net.finalizeRouting();
        if (visual_ && stateDumper) {
            stateDumper->dumpStep(fpga, design, "after_net_" + std::to_string(net.getIdx()));
        }
    }
    std::cout << "Successful routes: " << successfulRoutes << "/" << totalRoutes << std::endl;
    if (stateDumper) {
        delete stateDumper;
        stateDumper = nullptr;
    }
}

int MyRouter::measure_manhattan(RRNode *a, RRNode *b)
{
    return std::abs(a->getX() - b->getX()) + std::abs(a->getY() - b->getY());
}

void MyRouter::sortNets(std::vector<Net *> &nets)
{
    std::vector<RRNode *> allPins;
    for (Net *net : nets) {
        allPins.push_back(&net->getSource());
        for (RRNode *sink : net->getSinks()) {
            allPins.push_back(sink);
        }
    };

    std::sort(allPins.begin(), allPins.end(), [](RRNode *lhs, RRNode *rhs) {
        if (lhs->getX() != rhs->getX()) {
            return lhs->getX() < rhs->getX();
        }
        return lhs->getY() < rhs->getY();
    });

    auto countPinsInBoundingBox = [&](Net *net) -> int {
        int min_x = net->getSource().getX();
        int max_x = net->getSource().getX();
        int min_y = net->getSource().getY();
        int max_y = net->getSource().getY();

        auto updateBox = [&](RRNode *pin) {
            min_x = std::min(min_x, pin->getX());
            max_x = std::max(max_x, pin->getX());
            min_y = std::min(min_y, pin->getY());
            max_y = std::max(max_y, pin->getY());
        };

        for (RRNode *sink : net->getSinks()) {
            updateBox(sink);
        }

        auto x_begin = std::lower_bound(
            allPins.begin(), allPins.end(), min_x,
            [](RRNode *pin, int x) { return pin->getX() < x; });
        auto x_end = std::upper_bound(
            allPins.begin(), allPins.end(), max_x,
            [](int x, RRNode *pin) { return x < pin->getX(); });

        int count = 0;
        for (auto it = x_begin; it != x_end; ++it) {
            RRNode *pin = *it;
            if (pin->getY() >= min_y && pin->getY() <= max_y) {
                ++count;
            }
        }
        return count;
    };

    std::sort(nets.begin(), nets.end(), [&](Net *a, Net *b)->bool {
        int a_count = countPinsInBoundingBox(a);
        int b_count = countPinsInBoundingBox(b);
        if (a_count != b_count) {
            return a_count < b_count; // bbox中引脚数少的net优先布线
        }
        return a->getIdx() < b->getIdx();
    });
}

std::set<MyRouter::NodePair> MyRouter::disassemble_mst(Net &net)
{
    std::vector<RRNode*> pins;
    pins.push_back(&net.getSource());
    for (RRNode *sink : net.getSinks()) {
        pins.push_back(sink);
    }

    std::vector<std::pair<int, int>> edges;
    edges.reserve((pins.size() * (pins.size() - 1)) / 2);
    for (int i = 0; i < static_cast<int>(pins.size()); ++i) {
        for (int j = i + 1; j < static_cast<int>(pins.size()); ++j) {
            edges.push_back({i, j});
        }
    }

    std::sort(edges.begin(), edges.end(), [&](const std::pair<int, int> &a, const std::pair<int, int> &b) {
        int wa = measure_manhattan(pins[a.first], pins[a.second]);
        int wb = measure_manhattan(pins[b.first], pins[b.second]);
        if (wa != wb) {
            return wa < wb;
        }
        if (a.first != b.first) {
            return a.first < b.first;
        }
        return a.second < b.second;
    });

    
    std::vector<int> parent(pins.size());
    std::vector<int> rnk(pins.size(), 0);
    for (int i = 0; i < static_cast<int>(pins.size()); ++i) {
        parent[i] = i;
    }

    std::function<int(int)> find_root = [&](int x) {
        if (parent[x] != x) {
            parent[x] = find_root(parent[x]);
        }
        return parent[x];
    };

    auto unite = [&](int a, int b) {//并查集合并
        int ra = find_root(a);
        int rb = find_root(b);
        if (ra == rb) {
            return false;
        }
        if (rnk[ra] < rnk[rb]) {
            std::swap(ra, rb);
        }
        parent[rb] = ra;
        if (rnk[ra] == rnk[rb]) {
            ++rnk[ra];
        }
        return true;
    };

    std::set<NodePair> mstEdges;
    for (const auto &e : edges) {
        if (!unite(e.first, e.second)) {
            continue;
        }

        RRNode *a = pins[e.first];
        RRNode *b = pins[e.second];
        if (b < a) {
            std::swap(a, b);
        }
        mstEdges.insert({a, b});

        if (mstEdges.size() == pins.size() - 1) {
            break;
        }
    }

    if (mstEdges.size() != pins.size() - 1) {
        throw std::runtime_error("Kruskal MST edge count mismatch: duplicated or missing edges.");
    }

    return mstEdges;
}

// BFS 最短路：无权图下的最短跳数路径。
// 父指针 parent 构成 BFS 树，找到 sink 后回溯写入 net 路径。
// 可经过空闲节点，或本 net 自己的节点（终端已预保留，需允许到达自身 sink）。
bool MyRouter::singleroute_bfs(Net &net, RRNode *source, RRNode *sink)
{
    if (*source == *sink) {
        net.addRRToPath(*source);
        return true;
    }
    std::unordered_map<RRNode*, RRNode*> parent;   // 用于记录BFS树
    std::queue<RRNode*> q;    // BFS队列

    q.push(source);
    parent[source] = source; 

    while(!q.empty()) {
        RRNode *current = q.front();
        q.pop();
        if(*current == *sink) {   
            net.addRRToPath(*current);
            while(parent[current] != current) {   // 从sink回溯到source
                current = parent[current];
                net.addRRToPath(*current);
            }
            return true;
        }


        for(RRNode *neighbor : current->getConnections()) {
            bool traversable = !neighbor->isUsed() || neighbor->getNet() == &net;
            if(parent.find(neighbor) == parent.end() && traversable) {
                parent[neighbor] = current;    // 将current设置为neighbor的父节点
                q.push(neighbor);
            }
        }
    }
    return false;
}

// A* 最短路：g 为已走跳数，h 为到 sink 的 Manhattan 距离，f = g + h。
// 使用 closed 集标记已扩展节点，并允许更优 g 重新入队，得到真正的 A*。
// 可经过空闲节点，或本 net 自己的节点（终端已预保留）。
bool MyRouter::singleroute_Astar(Net &net, RRNode *source, RRNode *sink)
{
    if (*source == *sink) {
        net.addRRToPath(*source);
        return true;
    }
    std::unordered_map<RRNode*, RRNode*> parent;
    std::unordered_map<RRNode*, int> gScore;
    std::set<RRNode*> closed;   // 已扩展节点；配合 g 松弛可忽略过期条目

    using PQItem = std::pair<int, RRNode*>; // (f_score, node)
    std::priority_queue<PQItem, std::vector<PQItem>, std::greater<PQItem>> openSet;

    parent[source] = source;
    gScore[source] = 0;
    int f0 = measure_manhattan(source, sink);
    openSet.emplace(f0, source);

    while(!openSet.empty()) {
        RRNode *current = openSet.top().second;
        openSet.pop();

        if(closed.find(current) != closed.end()) {
            continue;   // 该节点已有更优 g 被扩展过，忽略过期条目
        }
        closed.insert(current);

        if(*current == *sink) {
            net.addRRToPath(*current);
            while(parent[current] != current) {
                current = parent[current];
                net.addRRToPath(*current);
            }
            return true;
        }

        for(RRNode *neighbor : current->getConnections()) {
            // 占用资源属于别的 net 时不可走；空闲或本 net 自己的节点可走
            if(neighbor->isUsed() && neighbor->getNet() != &net) {
                continue;
            }
            int tentative_g = gScore[current] + 1;
            auto it = gScore.find(neighbor);
            if(it == gScore.end() || tentative_g < it->second) {
                parent[neighbor] = current;
                gScore[neighbor] = tentative_g;
                int f = tentative_g + measure_manhattan(neighbor, sink);
                openSet.emplace(f, neighbor);
            }
        }
    }
    return false;
}

// Mikami-Tabuchi 双端线搜索：从 source/sink 同时扩展前沿。
// 扩展规则：CB_WIRE 只走一步；H_WIRE/V_WIRE 沿同类型导线直线延伸，
// 遇到类型变化才产生新的扩散点。当两棵搜索树相遇即找到路径。
bool MyRouter::singleroute_mikami(Net &net, RRNode *source, RRNode *sink)
{
    if (*source == *sink) {
        net.addRRToPath(*source);
        return true;
    }

    auto isTraversable = [&](RRNode *node) {
        return !node->isUsed() || node->getNet() == &net || *node == *source || *node == *sink;
    };

    std::unordered_map<RRNode*, RRNode*> parentS;
    std::unordered_map<RRNode*, RRNode*> parentT;
    std::vector<RRNode*> frontierS{source};
    std::vector<RRNode*> frontierT{sink};
    parentS[source] = source;
    parentT[sink] = sink;

    RRNode *meet = nullptr;

    auto extendLine = [&](RRNode *first,
                          RRNode *prev,
                          std::vector<RRNode*> &nextFrontier,
                          std::unordered_map<RRNode*, RRNode*> &parentThis,
                          std::unordered_map<RRNode*, RRNode*> &parentOther) -> bool {
        RRNode *current = first;
        RRNode *last = prev;

        while (current != nullptr) {
            if (parentThis.find(current) != parentThis.end()) {
                return false;
            }

            parentThis[current] = last;
            nextFrontier.push_back(current);

            if (parentOther.find(current) != parentOther.end()) {
                meet = current;
                return true;
            }

            RRNode *next = nullptr;
            for (RRNode *neighbor : current->getConnections()) {
                if (neighbor == last || !isTraversable(neighbor)) {
                    continue;
                }
                if (neighbor->getType() != current->getType()) {
                    continue;
                }
                next = neighbor;
                break;
            }

            if (next == nullptr) {
                break;
            }

            last = current;
            current = next;
        }

        return false;
    };

    auto expandOnePoint = [&](RRNode *node,
                              std::vector<RRNode*> &nextFrontier,
                              std::unordered_map<RRNode*, RRNode*> &parentThis,
                              std::unordered_map<RRNode*, RRNode*> &parentOther) -> bool {
        if (parentOther.find(node) != parentOther.end()) {
            meet = node;
            return true;
        }

        for (RRNode *neighbor : node->getConnections()) {
            if (!isTraversable(neighbor) || parentThis.find(neighbor) != parentThis.end()) {
                continue;
            }

            if (parentOther.find(neighbor) != parentOther.end()) {
                parentThis[neighbor] = node;
                nextFrontier.push_back(neighbor);
                meet = neighbor;
                return true;
            }

            if (neighbor->getType() == RRNode::CB_WIRE) {
                parentThis[neighbor] = node;
                nextFrontier.push_back(neighbor);
            } else if (neighbor->getType() == RRNode::H_WIRE || neighbor->getType() == RRNode::V_WIRE) {
                parentThis[neighbor] = node;
                nextFrontier.push_back(neighbor);
                if (extendLine(neighbor, node, nextFrontier, parentThis, parentOther)) {
                    return true;
                }
            }
        }

        return false;
    };

    auto expandOneSide = [&](const std::vector<RRNode*> &frontier,
                             std::unordered_map<RRNode*, RRNode*> &parentThis,
                             std::unordered_map<RRNode*, RRNode*> &parentOther) -> std::vector<RRNode*> {
        std::vector<RRNode*> nextFrontier;
        for (RRNode *start : frontier) {
            if (meet != nullptr) {
                break;
            }
            expandOnePoint(start, nextFrontier, parentThis, parentOther);
        }
        return nextFrontier;
    };

    while (!frontierS.empty() && !frontierT.empty() && meet == nullptr) {
        frontierS = expandOneSide(frontierS, parentS, parentT);
        if (meet != nullptr) {
            break;
        }
        frontierT = expandOneSide(frontierT, parentT, parentS);
    }

    if (meet == nullptr) {
        return false;
    }

    RRNode *curr = meet;
    net.addRRToPath(*curr);
    while (parentS[curr] != curr) {
        curr = parentS[curr];
        net.addRRToPath(*curr);
    }

    curr = meet;
    while (parentT[curr] != curr) {
        curr = parentT[curr];
        net.addRRToPath(*curr);
    }

    return true;
}

Net *MyRouter::ripup_net(RRNode *obstacle)
{
    if(obstacle->isUsed()) {
        Net *net = obstacle->getNet();
        net->clearPath();
        return net;
    }
    else {
        return nullptr;
    }
}
