/**
 * @file VisualizationDump.cpp
 * @brief RoutingStateDumper 实现：逐步 JSON 与 manifest 的写出。
 *
 * 拥塞定义：某 RRNode 的 occupancy > 1（容量为 1）。
 * 每个 tile 输出 occupancy（已用节点数）、capacity（节点总数）与
 * congestion_ratio（拥塞节点数 / 节点总数），并累计到历史曲线。
 */
#include "VisualizationDump.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <set>
#include <sstream>
#include <unordered_set>
#include <utility>

#include "../Design.h"
#include "../FPGA.h"
#include "../FpgaTile.h"
#include "../Net.h"
#include "../RRNode.h"

namespace {
std::string rrTypeToString(RRNode::rrType type) {
    switch (type) {
        case RRNode::H_WIRE: return "H_WIRE";
        case RRNode::V_WIRE: return "V_WIRE";
        case RRNode::CB_WIRE: return "CB_WIRE";
    }
    return "UNKNOWN";
}
} // namespace

RoutingStateDumper::RoutingStateDumper(std::string outputDir)
    : outputDir(std::move(outputDir)), nextStep(0) {
    std::filesystem::create_directories(this->outputDir);
    for (const auto &entry : std::filesystem::directory_iterator(this->outputDir)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        const std::string name = entry.path().filename().string();
        if (name == "manifest.json" || name.rfind("step_", 0) == 0) {
            std::filesystem::remove(entry.path());
        }
    }
}

void RoutingStateDumper::writeManifest(FPGA &fpga) const {
    std::ofstream out(outputDir + "/manifest.json");
    out << "{\n";
    out << "  \"grid_width\": " << fpga.getN() << ",\n";
    out << "  \"grid_height\": " << fpga.getN() << ",\n";
    out << "  \"tracks\": " << fpga.getW() << ",\n";
    out << "  \"steps\": [\n";
    for (size_t i = 0; i < steps.size(); ++i) {
        out << "    {\"file\": \"" << steps[i].file << "\", \"label\": \"" << steps[i].label << "\"}";
        if (i + 1 != steps.size()) out << ",";
        out << "\n";
    }
    out << "  ],\n";
    
    // 输出拥塞下降曲线
    out << "  \"congestion_history\": [";
    for (size_t i = 0; i < congestionHistory.size(); ++i) {
        out << congestionHistory[i];
        if (i + 1 != congestionHistory.size()) out << ", ";
    }
    out << "],\n";
    
    // 输出资源占用上升曲线
    out << "  \"resource_usage_history\": [";
    for (size_t i = 0; i < resourceUsageHistory.size(); ++i) {
        out << std::fixed << std::setprecision(2) << resourceUsageHistory[i];
        if (i + 1 != resourceUsageHistory.size()) out << ", ";
    }
    out << "]\n";
    
    out << "}\n";
}

void RoutingStateDumper::dumpStep(FPGA &fpga, Design &design, const std::string &label) {
    std::ostringstream name;
    name << "step_" << std::setw(4) << std::setfill('0') << nextStep++ << ".json";
    const std::string fileName = name.str();
    const std::string path = outputDir + "/" + fileName;

    std::ofstream out(path);
    out << "{\n";
    out << "  \"grid_width\": " << fpga.getN() << ",\n";
    out << "  \"grid_height\": " << fpga.getN() << ",\n";
    out << "  \"tracks\": " << fpga.getW() << ",\n";
    out << "  \"label\": \"" << label << "\",\n";
    out << "  \"tiles\": [\n";

    auto &tiles = fpga.getTiles();
    int totalCongested = 0;
    int totalUsed = 0;
    int totalCapacity = 0;
    
    for (size_t i = 0; i < tiles.size(); ++i) {
        FpgaTile *tile = tiles[i];
        int used = 0;
        int capacity = static_cast<int>(tile->getRRNodes().size());
        int congestedNodes = 0;
        for (RRNode *node : tile->getRRNodes()) {
            if (node->isUsed()) {
                ++used;
            }
            // 节点拥塞: occupancy > 1 表示多个net占用该节点
            if (node->getOccupancy() > 1) {
                ++congestedNodes;
            }
        }
        // 计算tile的拥塞比例: 拥塞节点数 / 总节点数
        double congestionRatio = (capacity > 0) ? static_cast<double>(congestedNodes) / capacity : 0.0;
        bool hasCongestion = congestedNodes > 0;
        if (hasCongestion) ++totalCongested;
        totalUsed += used;
        totalCapacity += capacity;
        
        out << "    {\"x\": " << tile->getX()
            << ", \"y\": " << tile->getY()
            << ", \"occupancy\": " << used
            << ", \"capacity\": " << capacity
            << ", \"congestion_ratio\": " << congestionRatio << "}";
        if (i + 1 != tiles.size()) out << ",";
        out << "\n";
    }
    out << "  ]\n";

    out << "}\n";

    steps.push_back({fileName, label});
    
    // 记录历史数据
    congestionHistory.push_back(totalCongested);
    double usagePercent = totalCapacity > 0 ? (totalUsed * 100.0) / totalCapacity : 0.0;
    resourceUsageHistory.push_back(usagePercent);
    
    writeManifest(fpga);
}
