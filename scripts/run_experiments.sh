#!/bin/bash
# =============================================================================
# run_experiments.sh
# 权威实验矩阵：详细布线方法运行时选择 + 协商布线
#
# 用法: bash scripts/run_experiments.sh
# - 逐条顺序执行，保证计时可信
# - visual 一律 false（不生成逐步可视化 JSON）
# - 每条命令 stdout 同时 tee 到 test_logs/<name>.log
# - 输出目录固定为 /tmp/vlsi_exp_out
# =============================================================================

set -u

MAIN="./build/main"
BENCH_DIR="./benchmark"
OUT_DIR="/tmp/vlsi_exp_out"
LOG_DIR="test_logs"
mkdir -p "$LOG_DIR"
rm -rf "$OUT_DIR"

run_exp() {
    # args: name bench W router maxiter threads
    local name="$1" bench="$2" W="$3" router="$4" maxiter="$5" threads="$6"
    local log="$LOG_DIR/${name}.log"
    echo "================================================================"
    echo "[RUN] $name"
    echo "      cmd: $MAIN $BENCH_DIR/$bench $W $OUT_DIR $router $maxiter $threads false"
    echo "================================================================"
    local start end
    start=$(date +%s)
    "$MAIN" "$BENCH_DIR/$bench" "$W" "$OUT_DIR" "$router" "$maxiter" "$threads" false 2>&1 | tee "$log"
    end=$(date +%s)
    echo "ELAPSED=$((end - start))" | tee -a "$log"
    echo ""
}

# -----------------------------------------------------------------------------
# A) 详细布线对比, W=30
# -----------------------------------------------------------------------------
for b in lg_sparse large_dense huge; do
    for m in bfs astar mikami; do
        run_exp "detail_${b}_W30_${m}" "$b" 30 "$m" 30 16
    done
done

# -----------------------------------------------------------------------------
# B) 详细布线压力, large_dense W=25
# -----------------------------------------------------------------------------
for m in bfs astar mikami; do
    run_exp "detail_large_dense_W25_${m}" large_dense 25 "$m" 30 16
done

# -----------------------------------------------------------------------------
# C) 协商布线 med_dense, 16 线程, maxiter=50  (W=30/25/20)
# -----------------------------------------------------------------------------
for W in 30 25 20; do
    run_exp "neg_med_dense_W${W}_t16" med_dense "$W" negotiated 50 16
done

# -----------------------------------------------------------------------------
# D) 并行加速比 med_dense W=25, maxiter=50, threads=4/8/16
# -----------------------------------------------------------------------------
for t in 4 8 16; do
    run_exp "neg_med_dense_W25_t${t}" med_dense 25 negotiated 50 "$t"
done

# -----------------------------------------------------------------------------
# E) 大规模协商布线, 18 线程, maxiter=50
# -----------------------------------------------------------------------------
for W in 30 25; do
    run_exp "neg_lg_sparse_W${W}_t18" lg_sparse "$W" negotiated 50 18
done
for W in 30 25; do
    run_exp "neg_large_dense_W${W}_t18" large_dense "$W" negotiated 50 18
done
for W in 30 25; do
    run_exp "neg_huge_W${W}_t18" huge "$W" negotiated 50 18
done

echo "================================================================"
echo "ALL EXPERIMENTS DONE"
echo "================================================================"
