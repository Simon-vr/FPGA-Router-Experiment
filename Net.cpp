/**
 * @file Net.cpp
 * @brief Net 实现：路径记录、net 标记与布通性校验。
 *
 * verifyRouting() 在 RR 图上做 BFS，只沿“归属于本 net”的节点扩展，
 * 以此判断 source 与每个 sink 是否真正处于同一连通分量。
 */
#include <algorithm>
#include <assert.h>
#include <iostream>
#include <list>
#include <set>

#include "Net.h"
#include "RRNode.h"

using namespace std;

Net::Net(RRNode &source, int idx): source(source), idx(idx) {}

Net::~Net() {}

void Net::addSink(RRNode &dest) {
    assert(find(sinks.begin(), sinks.end(), &dest) == sinks.end());
    sinks.insert(&dest);
}

void Net::finalizeRouting() {
    for(auto node : usedRRs) {
        node->setNet(*this);
    }
}

bool Net::verifyRouting() {
    set<RRNode *> nodesReached;
    list<RRNode *> nodesToVisit;

    if(source.getNet() != this) {
        cout << "**Net source " << source << " is not marked as belonging to net " << idx << endl;
        return false;
    }
    else {
        nodesToVisit.push_back(&source);
    }

    while(nodesToVisit.size()) {
        RRNode *node = nodesToVisit.front();
        nodesToVisit.pop_front();
        nodesReached.insert(node);

        for(auto connection : node->getConnections()) {
            if(connection->getNet() == this && (nodesReached.find(connection) == nodesReached.end())) {
                nodesToVisit.push_back(connection);
            }
        }
    }

    for(auto sink : sinks) {
        if(nodesReached.find(sink) == nodesReached.end()) {
            cout << "***Net " << idx << " with source " << source << " did not reach sink: " << *sink << endl;
            return false;
        }
    }

    return true;
}

void Net::setPath(const std::vector<RRNode*>& nodes) {
    usedRRs.clear();
    for (RRNode* node : nodes) {
        usedRRs.insert(node);
    }
}