/**
 * @file RRNode.h
 * @brief 布线资源节点（Routing Resource Node）：图中一个可布线的点。
 *
 * 节点类型（rrType）：
 *  - H_WIRE：水平导线；
 *  - V_WIRE：垂直导线；
 *  - CB_WIRE：逻辑块引脚（连接盒）。
 *
 * 职责：保存坐标/编号/类型、与其它节点的 connections 邻接表、
 * 当前占用它的 net，以及协商布线所需的 historicalCost / occupancy /
 * prevIterNets_ 等状态。
 */
#ifndef RRNODE_H
#define RRNODE_H

#include <ostream>
#include <vector>
#include <cmath>

using namespace std;

class RRNode;
class Net;

class RRNode{
public:
    enum rrType { H_WIRE, V_WIRE, CB_WIRE };
    const char *rrTypeStr[3] = {"H_WIRE", "V_WIRE", "CB_WIRE"}; // 分别对应水平导线，垂直导线，逻辑块引脚

    RRNode(rrType type, int x, int y, int idx);
    virtual ~RRNode();

private:
    vector<RRNode *> connections;   // 该布线资源节点与其他布线资源节点的连接关系
    rrType type;    // 布线资源节点类型
    int x;      // 布线资源节点所属的FPGA块的x坐标
    int y;      // 布线资源节点所属的FPGA块的y坐标
    int idx;    // 布线资源节点编号
    Net *net;   // 布线资源节点所属的net
    
    // ============================================================================
    // 协商布线 (Negotiated Routing) 相关字段
    // ============================================================================
    float historicalCost = 1.0f;    // 历史代价：记录该节点在所有迭代中被使用的累积次数
    int occupancy = 0;              // 当前迭代占用计数：记录当前迭代中被多少net占用
    std::vector<Net*> prevIterNets_; // 上一轮迭代占用该节点的所有net（用于并行路由的冲突检测）
    
public:
    // ============================================================================
    // 协商布线 (Negotiated Routing) 相关方法
    // ============================================================================
    
    /**
     * @brief 获取该节点的历史代价
     * @return 历史代价值，初始值为1.0
     */
    float getHistoricalCost() const { return historicalCost; }
    
    /**
     * @brief 增加历史代价，通常在当前迭代结束后调用
     */
    void incrementHistoricalCost(int over=1) { 
        historicalCost += 1;
    }
    
    /**
     * @brief 重置历史代价（用于重新开始协商布线）
     */
    void resetHistoricalCost() { historicalCost = 1.0f; }
    
    /**
     * @brief 获取当前迭代的占用计数
     * @return 占用次数，每次迭代开始时重置为0
     */
    int getOccupancy() const { return occupancy; }
    
    /**
     * @brief 重置当前迭代的占用计数
     */
    void resetOccupancy() { occupancy = 0; }
    
    /**
     * @brief 增加当前迭代的占用计数
     */
    void incrementOccupancy() { ++occupancy; }
    
    /**
     * @brief 获取上一轮迭代占用该节点的所有net
     * @return 上一轮占用该节点的net指针数组
     */
    const std::vector<Net*>& getPrevIterNets() const { return prevIterNets_; }
    
    /**
     * @brief 清空上一轮迭代的占用记录
     */
    void clearPrevIterNets() { prevIterNets_.clear(); }
    
    /**
     * @brief 添加一个net到上一轮迭代的占用记录
     * @param net 占用该节点的net指针
     */
    void addPrevIterNet(Net* net) { prevIterNets_.push_back(net); }
    
    /**
     * @brief 检查上一轮迭代该节点是否被其他net占用
     * @param currentNet 当前正在路由的net
     * @return true 如果被其他net占用
     */
    bool isUsedByOtherInPrevIter(Net* currentNet) const {
        for (Net* n : prevIterNets_) {
            if (n != currentNet) return true;
        }
        return false;
    }
    void connect(RRNode &node);         // 将该布线资源节点与node连接
    bool isConnected(RRNode &node);     // 检查该布线资源节点是否与node连接
    rrType getType() { return type; }   // 获得布线资源节点类型
    int getX() { return x; }            // 获得布线资源节点的x坐标
    int getY() { return y; }            // 获得布线资源节点的y坐标
    int getIdx() { return idx; }        // 获得布线资源节点的编号

    bool isUsed() { return net != nullptr; }    // 检查布线资源节点是否已使用
    void setNet(Net &net);              // 设置该布线资源节点所属的net
    Net *getNet() { return net; }       // 获得该布线资源节点所属的net
    void clearNet() { net = nullptr; }  // 清除该布线资源节点所属的net（用于拆线）
    std::vector<RRNode *> &getConnections() { return connections; } // 获得该布线资源节点与其他布线资源节点的连接关系

    
    friend std::ostream &operator<<(std::ostream &out, RRNode const &node) {
        out << "RRNode (" << node.x << ", " << node.y << ")."
            << node.rrTypeStr[node.type] << "." << node.idx;
        return out;
    }
    friend bool operator==(const RRNode &a, const RRNode &b) {
        return a.x == b.x && a.y == b.y && a.type == b.type && a.idx == b.idx;
    }
    friend bool operator!=(const RRNode &a, const RRNode &b) {
        return !(a == b);
    }
};

#endif