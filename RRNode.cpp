/**
 * @file RRNode.cpp
 * @brief RRNode 实现：邻接表双向连接与 net 标记。
 *
 * connect() 使用断言保证不重复连接，双向写入双方 connections，
 * 从而在 RR 图上形成无向边。
 */
#include <algorithm>
#include <assert.h>
#include <ostream>

#include "RRNode.h"

using namespace std;

RRNode::RRNode(rrType type, int x, int y, int idx): type(type), x(x), y(y), idx(idx), net(nullptr) {

}

RRNode::~RRNode(){}

void RRNode::setNet(Net &net) {
    this->net = &net;
}

bool RRNode::isConnected(RRNode &node) {
    return find(connections.begin(), connections.end(), &node) != connections.end();
}

void RRNode::connect(RRNode &node) {
    assert(!isConnected(node));
    assert(!node.isConnected(*this));

    connections.push_back(&node);
    node.connections.push_back(this);
}