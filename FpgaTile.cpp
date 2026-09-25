/**
 * @file FpgaTile.cpp
 * @brief FpgaTile 实现：资源生成与开关块连接。
 *
 * 算法要点：
 *  - generateContents(): 仅在有下/右侧邻居时生成对应方向的 W 条导线
 *    （避免边界重叠），并在内部 tile 生成 4 个逻辑引脚，
 *    用 V_WIRE 连引脚1/2、H_WIRE 连引脚3/4 构成连接盒；
 *  - populateSwitchbox(): 按 6 种相邻方向组合建立通道间导线连接，
 *    交叉处采用取模轮转实现可布线的开关模式。
 */
#include "RRNode.h"
#include "FpgaTile.h"

using namespace std;

FpgaTile::FpgaTile(int x, int y, int W): x(x), y(y), W(W), left(nullptr), right(nullptr), up(nullptr), down(nullptr) {}

FpgaTile::~FpgaTile() {
    for(auto node: rrNodes) {
        delete node;
    }
}

void FpgaTile::generateContents() { // 生成水平和垂直的导线，以及logic block的引脚的导线
    // Add routing tracks
    if(down) {
        for(int i = 0; i < W; i++) {
            RRNode *node = new RRNode(RRNode::V_WIRE, x, y, i);
            rrNodes.push_back(node);
            vWires.push_back(node);
        }
    }
    if(right) {
        for(int i = 0; i < W; i++) {
            RRNode *node = new RRNode(RRNode::H_WIRE, x, y, i);
            rrNodes.push_back(node);
            hWires.push_back(node);
        }
    }

    if(down && right) {
        // Add logic pins
        logicPin[1] = new RRNode(RRNode::CB_WIRE, x, y, 1);
        logicPin[2] = new RRNode(RRNode::CB_WIRE, x, y, 2);
        logicPin[3] = new RRNode(RRNode::CB_WIRE, x, y, 3);
        logicPin[4] = new RRNode(RRNode::CB_WIRE, x, y, 4);

        rrNodes.push_back(logicPin[1]);
        rrNodes.push_back(logicPin[2]);
        rrNodes.push_back(logicPin[3]);
        rrNodes.push_back(logicPin[4]);

        // Add connection box
        for(int i = 0; i < W; i++) {
            vWires[i]->connect(*(logicPin[1]));
            vWires[i]->connect(*(logicPin[2]));
        }
        for(int i = 0; i < W; i++) {
            hWires[i]->connect(*(logicPin[3]));
            hWires[i]->connect(*(logicPin[4]));
        }
    }
}

void FpgaTile::populateSwitchbox() {    // 建立开关块导线的连接关系
    if(up && down) {
        for(int i = 0; i < W; i++) {
            vWires[i]->connect(up->getVWire(i));
        }
    }

    if(left && right) {
        for(int i = 0; i < W; i++) {
            hWires[i]->connect(left->getHWire(i));
        }
    }

    if(left && up) {
        for(int i = 0; i < W; i++) {
            left->getHWire(i).connect(up->getVWire((W - i) % W));
        }
    }

    if(up && right) {
        for(int i = 0; i < W; i++) {
            up->getVWire(i).connect(this->getHWire((i + 1) % W));
        }
    }

    if(right && down) {
        for(int i = 0; i < W; i++) {
            this->getHWire(i).connect(this->getVWire((2 * W - 2 - i) % W));
        }
    }

    if(down && left) {
        for(int i = 0; i < W; i++) {
            this->getVWire(i).connect(left->getHWire((i + 1) % W));
        }
    }
}