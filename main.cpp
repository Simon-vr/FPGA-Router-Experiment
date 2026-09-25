/**
 * @file main.cpp
 * @brief 程序入口：读取电路文件，构建 FPGA/Design，按命令行选择路由器并启动。
 *
 * 命令行格式：
 *   ./build/main <circuit_file> <W> [out_dir] [router_type]
 *                [maxiter] [num_threads] [visual]
 *
 * router_type 运行时选择（本次实验新增/扩展）：
 *   - bfs    : BFS 详细布线（旧别名 "my"）
 *   - astar  : A* 详细布线（别名 "a*"）
 *   - mikami : Mikami-Tabuchi 详细布线
 *   - negotiated : PathFinder 协商布线
 * 仅 negotiated 使用 maxiter / num_threads / visual 参数；
 * visual 参数控制是否导出逐步 JSON，默认 false（本次实验禁止开启）。
 */
#include <assert.h>
#include <cctype>
#include <fstream>
#include <iostream>
#include <string>
#include <regex>
#include <sstream>

#include "Design.h"
#include "FPGA.h"
#include "FpgaTile.h"
#include "Net.h"
#include "RRNode.h"
#include "Solution.h"

using namespace std;

/*
    Usage: ./main.exe <circuit_file> <W> [output_dir] [router_type] [maxiter] [num_threads] [vistual_enable]
     - circuit_file: 输入电路文件路径
     - W: 布线通道宽度
     - output_dir: 可选，输出目录，默认为"annual_results"
     - router_type: 可选，路由器类型，默认为"bfs"，可选"bfs"(别名"my")/"astar"(别名"a*")/"mikami"/"negotiated"
     - maxiter: 可选，协商布线的最大迭代次数，仅在router_type为"negotiated"时有效，默认为30
     - num_threads: 可选，并行路由的线程数量，仅在router_type为"negotiated"时有效，默认为4
     - visual_enable: 可选，是否启用可视化输出，仅在router_type为"negotiated"时有效，默认为false
*/

int main(int argc, char *argv[]) {
    if (argc < 3 || argc > 8) {
        cerr << "Usage: ./main.exe <circuit_file> <W> [output_dir] [router_type] [maxiter] [num_threads] [vistual_enable]" << endl;
        cerr << "  - circuit_file: 输入电路文件路径" << endl;
        cerr << "  - W: 布线通道宽度" << endl;
        cerr << "  - output_dir: 可选，输出目录，默认为\"annual_results\"" << endl;
        cerr << "  - router_type: 可选，路由器类型，默认为\"bfs\"，可选\"bfs\"(\"my\")/\"astar\"/\"mikami\"/\"negotiated\"" << endl;
        cerr << "  - maxiter: 可选，协商布线的最大迭代次数，仅在router_type为\"negotiated\"时有效，默认为30" << endl;
        cerr << "  - num_threads: 可选，并行路由的线程数量，仅在router_type为\"negotiated\"时有效，默认为4" << endl;
        cerr << "  - visual_enable: 可选，是否启用可视化输出，仅在router_type为\"negotiated\"时有效，默认为false" << endl;
        return 1;
    }
    bool printNets = false;

    string circuit_file = argv[1];
    int W = stoi(argv[2]);
    string output_dir = (argc >= 4) ? argv[3] : "annual_results";
    string router_type = (argc >= 5) ? argv[4] : "bfs";
    for (char &c : router_type) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    int maxiter = (argc >= 6) ? stoi(argv[5]) : 30;
    int num_threads = (argc >= 7) ? stoi(argv[6]) : 4;
    bool visual_enable = (argc >= 8) ? (string(argv[7]) == "true" || string(argv[7]) == "1") : false;

    ifstream fp(circuit_file);
    string line;

    // Get grid size
    getline(fp, line);
    int gridSize = stoi(line) + 1;
    cout << "Grid size: " << gridSize << "x" << gridSize << endl;

    // Get # tracks
    cout << "Number of tracks: " << W << endl;

    // Initialize FPGA and design objects
    FPGA fpga(gridSize, W);
    Design design;

    // Get nets fom circuit file
    int netIdx = 0;
    while(true) {
        getline(fp, line);
        istringstream iss(line);

        // Read source
        string xStr, yStr, pStr;
        iss >> xStr >> yStr >> pStr;
        int x = stoi(xStr), y = stoi(yStr), p = stoi(pStr);

        if(x < 0)   break;
        if(printNets) {
            std::cout << "(" << x << ", " << y << ")." << p << " to ";
        }
        Net *net = new Net(fpga.getTile(x, y).getLogicPin(p), netIdx++);

        // Read list of sinks
        while(iss >> xStr) {
            iss >> yStr;
            iss >> pStr;
            x = stoi(xStr);
            y = stoi(yStr);
            p = stoi(pStr);
            if (printNets){
                std::cout << "(" << x << ", " << y << ")." << p << " ";
            }
            RRNode &sink = fpga.getTile(x, y).getLogicPin(p);
            net->addSink(sink);
        }
        if(printNets) {
            cout << endl;
        }
        design.addNet(*net);
    }

    cout << "Starting Routing" << endl;
    cout << "Router type: " << router_type << endl;

    // Initialize router based on type
    Router *router = nullptr;
    if (router_type == "negotiated") {
        cout << "Max iterations: " << maxiter << endl;
        cout << "Num threads: " << num_threads << endl;
        cout << "Visual enable: " << (visual_enable ? "true" : "false") << endl;
        router = new NegotiatedRouter(output_dir, maxiter, visual_enable, num_threads);
    } else {
        // 运行时选择详细布线方法：bfs（含 my 兼容别名）/ astar（含 a*）/ mikami
        string method = "bfs";
        if (router_type == "astar" || router_type == "a*") {
            method = "astar";
        } else if (router_type == "mikami") {
            method = "mikami";
        } else {
            method = "bfs";
        }
        cout << "Detail routing method: " << method << endl;
        router = new MyRouter(output_dir, method, visual_enable);
    }
    router->routeDesign(fpga, design);

    bool success = design.verifyRouting();
    if(!success) {
        cout << "Error: Routing not complete" << endl;
    } else {
        cout << "Routing check passed" << endl;
        cout << "Segments used: " << fpga.getNumSegmentsUsed() << endl;
    }

    // delete router;

    return 0;
}