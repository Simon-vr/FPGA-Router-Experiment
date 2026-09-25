#!/bin/bash

# 自动化实验脚本
# 输出日志文件
LOG_FILE="experiment_results.log"
MAIN_EXEC="./build/main"
BENCHMARK_DIR="benchmark"

# 清空或创建日志文件
> "$LOG_FILE"

echo "========================================" >> "$LOG_FILE"
echo "自动化实验开始 - $(date)" >> "$LOG_FILE"
echo "========================================" >> "$LOG_FILE"
echo "" >> "$LOG_FILE"

# ========================================
# 实验1: 压力测试 - 16线程, 50迭代, med_dense, W=30/25/20
# ========================================
echo "========================================" >> "$LOG_FILE"
echo "实验1: 压力测试" >> "$LOG_FILE"
echo "参数: 16线程, 50迭代, med_dense, W=30/25/20" >> "$LOG_FILE"
echo "========================================" >> "$LOG_FILE"
echo "" >> "$LOG_FILE"

for W in 30 25 20; do
    echo "--- med_dense, W=$W, 16线程, 50迭代 ---" >> "$LOG_FILE"
    echo "运行时间: $(date)" >> "$LOG_FILE"
    echo "命令: $MAIN_EXEC $BENCHMARK_DIR/med_dense $W results_pressure negotiated 50 16 false" >> "$LOG_FILE"
    echo "" >> "$LOG_FILE"
    
    $MAIN_EXEC "$BENCHMARK_DIR/med_dense" "$W" "results_pressure" "negotiated" 50 16 false >> "$LOG_FILE" 2>&1
    
    echo "" >> "$LOG_FILE"
    echo "退出码: $?" >> "$LOG_FILE"
    echo "----------------------------------------" >> "$LOG_FILE"
    echo "" >> "$LOG_FILE"
done

echo "" >> "$LOG_FILE"
echo "实验1 完成" >> "$LOG_FILE"
echo "" >> "$LOG_FILE"
echo "" >> "$LOG_FILE"

# ========================================
# 实验2: 并行提速测试 - med_dense, 50迭代, W=25, 线程数=4/8/16
# ========================================
echo "========================================" >> "$LOG_FILE"
echo "实验2: 并行提速测试" >> "$LOG_FILE"
echo "参数: med_dense, 50迭代, W=25, 线程数=4/8/16" >> "$LOG_FILE"
echo "========================================" >> "$LOG_FILE"
echo "" >> "$LOG_FILE"

W=25
for THREADS in 4 8 16; do
    echo "--- med_dense, W=$W, ${THREADS}线程, 50迭代 ---" >> "$LOG_FILE"
    echo "运行时间: $(date)" >> "$LOG_FILE"
    echo "命令: $MAIN_EXEC $BENCHMARK_DIR/med_dense $W results_thread${THREADS} negotiated 50 $THREADS false" >> "$LOG_FILE"
    echo "" >> "$LOG_FILE"
    
    $MAIN_EXEC "$BENCHMARK_DIR/med_dense" "$W" "results_thread${THREADS}" "negotiated" 50 "$THREADS" false >> "$LOG_FILE" 2>&1
    
    echo "" >> "$LOG_FILE"
    echo "退出码: $?" >> "$LOG_FILE"
    echo "----------------------------------------" >> "$LOG_FILE"
    echo "" >> "$LOG_FILE"
done

echo "" >> "$LOG_FILE"
echo "实验2 完成" >> "$LOG_FILE"
echo "" >> "$LOG_FILE"
echo "" >> "$LOG_FILE"

# ========================================
# 实验3: 大规模线网收敛测试 - W=30/25, 50迭代, 18线程, large_sparse, large_dense, huge (开可视化)
# ========================================
echo "========================================" >> "$LOG_FILE"
echo "实验3: 大规模线网收敛测试" >> "$LOG_FILE"
echo "参数: W=30, 50迭代, 18线程, large_sparse/large_dense/huge (开可视化)" >> "$LOG_FILE"
echo "========================================" >> "$LOG_FILE"
echo "" >> "$LOG_FILE"

THREADS=18

# large_sparse
for W in 30; do
    echo "--- large_sparse, W=$W, ${THREADS}线程, 50迭代, 开可视化 ---" >> "$LOG_FILE"
    echo "运行时间: $(date)" >> "$LOG_FILE"
    echo "命令: $MAIN_EXEC $BENCHMARK_DIR/lg_sparse $W results_large_sparse negotiated 50 $THREADS true" >> "$LOG_FILE"
    echo "" >> "$LOG_FILE"
    
    $MAIN_EXEC "$BENCHMARK_DIR/lg_sparse" "$W" "results_large_sparse" "negotiated" 50 "$THREADS" true >> "$LOG_FILE" 2>&1
    
    echo "" >> "$LOG_FILE"
    echo "退出码: $?" >> "$LOG_FILE"
    echo "----------------------------------------" >> "$LOG_FILE"
    echo "" >> "$LOG_FILE"
done

# large_dense
for W in 30; do
    echo "--- large_dense, W=$W, ${THREADS}线程, 50迭代, 开可视化 ---" >> "$LOG_FILE"
    echo "运行时间: $(date)" >> "$LOG_FILE"
    echo "命令: $MAIN_EXEC $BENCHMARK_DIR/large_dense $W results_large_dense negotiated 50 $THREADS true" >> "$LOG_FILE"
    echo "" >> "$LOG_FILE"
    
    $MAIN_EXEC "$BENCHMARK_DIR/large_dense" "$W" "results_large_dense" "negotiated" 50 "$THREADS" true >> "$LOG_FILE" 2>&1
    
    echo "" >> "$LOG_FILE"
    echo "退出码: $?" >> "$LOG_FILE"
    echo "----------------------------------------" >> "$LOG_FILE"
    echo "" >> "$LOG_FILE"
done

# huge (注意: benchmark目录中是 'huge' 不是 'huge_dense')
echo "--- huge, W=30, ${THREADS}线程, 50迭代, 开可视化 ---" >> "$LOG_FILE"
echo "运行时间: $(date)" >> "$LOG_FILE"
echo "命令: $MAIN_EXEC $BENCHMARK_DIR/huge 30 results_huge negotiated 50 $THREADS true" >> "$LOG_FILE"
echo "" >> "$LOG_FILE"

$MAIN_EXEC "$BENCHMARK_DIR/huge" "30" "results_huge" "negotiated" 50 "$THREADS" true >> "$LOG_FILE" 2>&1

echo "" >> "$LOG_FILE"
echo "退出码: $?" >> "$LOG_FILE"
echo "----------------------------------------" >> "$LOG_FILE"
echo "" >> "$LOG_FILE"

echo "" >> "$LOG_FILE"
echo "实验3 完成" >> "$LOG_FILE"
echo "" >> "$LOG_FILE"
echo "" >> "$LOG_FILE"

# ========================================
echo "========================================" >> "$LOG_FILE"
echo "所有实验完成 - $(date)" >> "$LOG_FILE"
echo "========================================" >> "$LOG_FILE"

echo "实验完成! 查看日志文件: $LOG_FILE"