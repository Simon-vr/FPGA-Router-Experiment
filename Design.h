/**
 * @file Design.h
 * @brief 设计网表容器：管理一个电路中的所有 net（线网）。
 *
 * 职责：
 *  - 持有并按加入顺序保存所有 Net 对象；
 *  - 提供 net 数量、按序号取 net、取全部 net 的接口；
 *  - verifyRouting() 汇总校验所有 net 是否真正连通。
 *
 * 关键点：addNet 会检查源/汇引脚是否与其他 net 冲突，冲突直接报错退出。
 */
#ifndef DESIGN_H
#define DESIGN_H

#include <map>
#include <string>
#include <vector>

using namespace std;

class Net;
class RRNode;

class Design {
public:
    Design();
    virtual ~Design();

private:
    vector<Net *> nets; // 用于存放net的数组

public:
    void addNet(Net &net);  // 添加net
    int getNumNets() { return nets.size(); }    // 获得net的数量
    Net &getNet(int idx) { return *(nets[idx]); }   // 获得指定序号的net
    vector<Net*> &getNets() { return nets; }    // 获得net数组
    bool verifyRouting();   // 检查是否布线成功
};

#endif