/**
 * @file VisualizationDump.h
 * @brief 可视化数据导出器：将每一步布线状态写为 JSON。
 *
 * 职责：
 *  - dumpStep() 导出一个 step_XXXX.json（每个 tile 的占用/容量/拥塞比例），
 *    并维护 manifest.json（grid 尺寸、tracks、步骤列表、
 *    拥塞下降曲线与资源占用上升曲线）；
 *  - 构造时清空旧结果目录中的 manifest.json 与 step_*.json。
 *
 * 注意：该功能由 router 的 visual 开关控制，本次实验保持关闭，
 * 不会生成新的逐步可视化 JSON。
 */
#ifndef VISUALIZATION_DUMP_H
#define VISUALIZATION_DUMP_H

#include <string>
#include <vector>

class FPGA;
class Design;

class RoutingStateDumper {
public:
    explicit RoutingStateDumper(std::string outputDir);
    void dumpStep(FPGA &fpga, Design &design, const std::string &label);

private:
    struct StepMeta {
        std::string file;
        std::string label;
    };

    std::string outputDir;
    int nextStep;
    std::vector<StepMeta> steps;
    std::vector<int> congestionHistory;       // 拥塞下降曲线数据
    std::vector<double> resourceUsageHistory; // 资源占用上升曲线数据

    void writeManifest(FPGA &fpga) const;
};

#endif
