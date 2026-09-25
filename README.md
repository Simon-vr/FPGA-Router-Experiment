# FPGA Negotiated Router

**📖 Project docs & live demo: [https://simon-vr.github.io/FPGA-Router-Experiment/](https://simon-vr.github.io/FPGA-Router-Experiment/)**

A teaching-oriented FPGA router that supports BFS / A* / Mikami–Tabuchi detailed routing and PathFinder-based negotiated routing with OpenMP parallelism.

[中文文档](README.zh-CN.md)

**Course site:** [customized-computing.github.io/VLSI-FPGA](https://customized-computing.github.io/VLSI-FPGA/#/)

![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)
![CMake](https://img.shields.io/badge/build-CMake-064F8C.svg)
![OpenMP](https://img.shields.io/badge/parallel-OpenMP-orange.svg)
![GitHub Pages](https://img.shields.io/badge/docs-GitHub%20Pages-2ea44f.svg)

---

## Highlights

- **Teaching-grade FPGA router** — clean, readable implementation of the full global → detailed → negotiated routing flow.
- **RR graph modeling** — an N×N tile grid with H_WIRE / V_WIRE / CB_WIRE resource nodes; `RRNode.net` records resource occupancy.
- **Multiple detailed routers** — BFS, A* (Manhattan heuristic), and Mikami–Tabuchi (node-type-driven two-terminal line search).
- **PathFinder negotiated routing** — congestion-aware rip-up & reroute with history + present cost, parallel route computation and serial commit.
- **OpenMP parallelism** — multi-threaded routing with deterministic results (parallelism does not change the solution).
- **Visualization replay** — step-by-step JSON dumps rendered by a lightweight in-browser viewer (`vistual/index.html`).

## Algorithm Overview

1. **Global routing (`sortNets`)** — nets are ordered ascending by the number of global pins inside their bounding box.
2. **Net decomposition (`disassemble_mst`)** — each net is split into two-terminal connections using a Kruskal MST weighted by Manhattan distance.
3. **Detailed routing (`singleroute_bfs` / `singleroute_astar` / `singleroute_mikami`)** — routes each two-terminal connection on the RR graph.
4. **Negotiated routing (`NegotiatedRouter`)** — iteratively rips up and reroutes congested nets using the PathFinder cost:

   ```text
   cost(v, t) = (1 + h(v)) · p(v, t) · (1 + 0.1 · t)
   ```

   where `h(v)` is the accumulated history congestion, `p(v, t)` is the present congestion, and `t` is the iteration index. Routes are computed in parallel with OpenMP and committed serially.

## Build & Run

```bash
# Configure and build with CMake
cmake -S . -B build
cmake --build build -j

# Equivalent once the build tree exists:
cd build && make all
```

The executable is produced at `./build/main`.

### Command-line usage

```text
./build/main <circuit> <W> [out_dir] [router_type] [maxiter] [threads] [visual]
```

| Argument | Description | Default |
|---|---|---|
| `circuit` | Circuit file path, e.g. `./benchmark/tiny` | — |
| `W` | Routing channel width (tracks) | — |
| `out_dir` | Output directory | `annual_results` |
| `router_type` | `bfs` \| `astar` \| `mikami` \| `negotiated` | `bfs` |
| `maxiter` | Max iterations (negotiated only) | `30` |
| `threads` | OpenMP threads (negotiated only) | `4` |
| `visual` | Emit step-by-step visualization JSON (negotiated only) | `false` |

Backward-compatible aliases: `my` → `bfs`, `a*` → `astar`.

> **Note:** `visual` defaults to `false`. The experiments below were run with `visual=false`, so no step-by-step JSON was generated.

### Tiny smoke test

```bash
./build/main ./benchmark/tiny 30 smoke_out bfs
```

Another detailed-routing example:

```bash
./build/main ./benchmark/lg_sparse 30 detail_out mikami
```

Negotiated-routing example:

```bash
./build/main ./benchmark/med_dense 25 results_negotiated \
             negotiated 50 16 false
```

## Results

All results were measured sequentially on the provided benchmarks (`lg_sparse`, `large_dense`, `huge`; negotiated routing on `med_dense`, `large_dense`, `huge`).

### A) Detailed-routing comparison (W = 30)

| Benchmark | Method | Connected / Total | Segments | Verify | Time (s) |
|---|---|---:|---:|:---:|---:|
| lg_sparse | bfs | 338/338 | 4056 | ✅ passed | 1 |
| lg_sparse | astar | 338/338 | 4067 | ✅ passed | 1 |
| lg_sparse | mikami | 338/338 | 4062 | ✅ passed | 1 |
| large_dense | bfs | 1028/1028 | 11939 | ✅ passed | 3 |
| large_dense | astar | 1028/1028 | 11968 | ✅ passed | 2 |
| large_dense | mikami | 1028/1028 | 11945 | ✅ passed | 2 |
| huge | bfs | 2307/2307 | 46819 | ✅ passed | 37 |
| huge | astar | 2307/2307 | 46928 | ✅ passed | 23 |
| huge | mikami | 2307/2307 | 46769 | ✅ passed | 22 |

**Takeaway:** All three detail routers complete and verify on the three benchmarks, with wire-length differences < 1.5%. On `huge`, `mikami` is the fastest and yields the shortest wire length.

### B) Detailed-routing stress (large_dense, W = 25)

| Method | Connected / Total | Verify | Time (s) |
|---|---:|---:|---:|
| bfs | 1028/1028 | ✅ passed | 6 |
| astar | 1028/1028 | ✅ passed | 1 |
| mikami | 1028/1028 | ✅ passed | 1 |

**Takeaway:** All three detail routers complete and verify at the tighter W = 25 (`Segments` 12043 / 12180 / 12105). The separate negotiated engine still does not converge on `large_dense` at W = 25 (table E).

### C) Negotiated-routing convergence (med_dense, 16 threads, maxiter = 50)

| W | Converged | Iterations | Segments | Verify | Time (s) |
|---:|:---:|---:|---:|:---:|---:|
| 30 | ✅ | 39 | 2960 | ✅ passed | 3 |
| 25 | ✅ | 35 | 2970 | ✅ passed | 3 |
| 20 | ❌ | 50 | 2940 | ❌ not complete | 3 |

**Takeaway:** The critical channel width lies between W = 20 and W = 25; W = 25 converges faster.

### D) Parallel speedup (med_dense, W = 25, maxiter = 50)

| Threads | Iterations | Time (s) | Speedup vs. 4 threads | Segments | Verify |
|---:|---:|---:|---:|---:|:---:|
| 4 | 35 | 7 | 1.00× | 2970 | ✅ passed |
| 8 | 35 | 4 | 1.75× | 2970 | ✅ passed |
| 16 | 35 | 3 | 2.33× | 2970 | ✅ passed |

**Takeaway:** Iteration count and solution quality are identical across thread counts — parallelism does not change the result. Speedup is sub-linear due to serial commit (Amdahl); 16 threads reach ~2.33× over 4 threads.

### E) Large-scale negotiated routing (18 threads, maxiter = 50)

| Benchmark | W | Converged | Iterations | Final congested nodes | Segments | Verify | Time (s) |
|---|---:|:---:|---:|---:|---:|:---:|---:|
| lg_sparse | 30 | ✅ | 20 | 0 | 4176 | ✅ passed | 4 |
| lg_sparse | 25 | ✅ | 29 | 0 | 4181 | ✅ passed | 6 |
| large_dense | 30 | ❌ | 50 | 170 | 12200 | ❌ not complete | 35 |
| large_dense | 25 | ❌ | 50 | 420 | 11949 | ❌ not complete | 31 |
| huge | 30 | ❌ | 50 | 431 | 48370 | ❌ not complete | 423 |
| huge | 25 | ❌ | 50 | 1202 | 47771 | ❌ not complete | 477 |

**Takeaway:** Only `lg_sparse` converges. For `large_dense`, W = 30 clearly improves congestion over W = 25 (170 vs. 420 nodes) but is still insufficient. `huge` takes ~7–8 minutes per run and does not converge in either case.

### Reproduce

```bash
cmake -S . -B build && cmake --build build -j
bash scripts/run_experiments.sh      # -> test_logs/*.log
python3 scripts/parse_results.py     # -> experiment_results.json
```

## Memory Analysis

A memory profile of the negotiated router (Valgrind Massif, med_dense, W = 25, 16 threads):

| Metric | Value |
|---|---:|
| Peak memory | 13.26 MB |
| Final memory | 10.67 MB |
| Useful heap | 8.72 MB |
| Extra overhead | 1.94 MB |

![Massif memory profile](docs/assets/massif_memory.png)

## Visualization & Online Docs

Documentation assets and rendered figures live under [`docs/`](docs/) (GitHub Pages). Example figures:

| | |
|---|---|
| ![A* huge congestion heatmap](docs/assets/astar_huge_net165_heatmap.png) | ![BFS large_dense congestion heatmap](docs/assets/bfs_large_dense_net349_heatmap.png) |
| ![Mikami large_dense failure heatmap](docs/assets/mikami_large_dense_net226_heatmap.png) | ![Mikami large_dense failure edges](docs/assets/mikami_large_dense_net226_edges.png) |
| ![Negotiated congestion heatmap](docs/assets/negotiated_congestion_heatmap.png) | ![Negotiated congestion curve](docs/assets/negotiated_congestion_curve.png) |

Pre-rendered data for the online viewer is available under [`docs/data/`](docs/data/):

- [`docs/data/huge.json`](docs/data/huge.json)
- [`docs/data/large_sparse.json`](docs/data/large_sparse.json)
- [`docs/data/large_dense.json`](docs/data/large_dense.json)

To replay routing states locally, start a static server in the repository root and open the viewer:

```bash
python3 -m http.server 8000
# then open http://localhost:8000/vistual/index.html in a browser
```

The viewer reads the sampled step data under `docs/data/*.json` and provides an iteration slider, heatmap / congestion toggle, play/pause (800 ms/step), and congestion / resource-usage line charts. (The server must run from the repository root, since the data lives outside `vistual/`.)

## Acknowledgements & License

- Course project for **Introduction to VLSI Design**; developed for teaching and study of FPGA routing algorithms.
- The negotiated router follows the classic **PathFinder** formulation; the detailed routers use BFS, A* (Manhattan heuristic), and the **Mikami–Tabuchi** line-search method.
- Released for educational use under the MIT License.
