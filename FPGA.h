/**
 * @file FPGA.h
 * @brief FPGA 芯片模型：N x N 的 tile 网格及其布线资源组织。
 *
 * 职责：
 *  - 按 gridSize x gridSize 创建并持有所有 FpgaTile；
 *  - 建立相邻 tile 的四方向指针，并调用 generateContents()/populateSwitchbox()
 *    生成导线、逻辑引脚与开关块连接；
 *  - getNumSegmentsUsed() 统计当前被占用的布线资源节点数。
 *
 * 关键数据结构：tileMap[x][y] 定位 tile，tiles 保存全部 tile。
 */
#ifndef FPGA_H
#define FPGA_H

#include <map>
#include <vector>

using namespace std;

class FpgaTile;

class FPGA{
public:
    FPGA(int gridSize, int W);
    virtual ~FPGA();

private:
    int N;  // FPGA的大小
    int W;  // 布线通道宽度
    map<int, map<int, FpgaTile *>> tileMap; // FPGA块的映射
    vector<FpgaTile *> tiles;   // FPGA块数组

public:
    FpgaTile &getTile(int x, int y) { return *(tileMap[x][y]); }    // 获得坐标为(x,y)的FPGA块
    vector<FpgaTile *> &getTiles() { return tiles; }    // 获得FPGA块数组

    int getN() { return N; }    // 获得FPGA的大小
    int getW() { return W; }    // 获得布线通道宽度
    int getNumSegmentsUsed();   // 获得布线后Routing Segment的数量
};

#endif