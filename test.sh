#!/bin/bash

# FPGA路由测试脚本
# 用法: ./test.sh [benchmark_name] [W_value] [output_dir] [log_name] 或 ./test.sh all

# 可执行文件路径
EXECUTABLE="./build/main"
BENCHMARK_DIR="./benchmark"

# 颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 日志文件配置
LOG_DIR="./test_logs"
DEFAULT_VISUAL_DIR="vistual/results"
LOG_FILE="$LOG_DIR/$(basename "$DEFAULT_VISUAL_DIR")_$(date '+%Y%m%d_%H%M%S').log"

# 创建日志目录
mkdir -p "$LOG_DIR"

# 检查可执行文件是否存在
if [ ! -f "$EXECUTABLE" ]; then
    echo -e "${RED}错误: 可执行文件 $EXECUTABLE 不存在${NC}"
    echo -e "${YELLOW}请先运行: make${NC}"
    exit 1
fi

# 日志函数
log_message() {
    local level=$1
    shift
    local message="$@"
    local timestamp=$(date '+%Y-%m-%d %H:%M:%S')
    
    # 输出到终端（带颜色）
    case $level in
        "INFO")  echo -e "${GREEN}[$timestamp] INFO: $message${NC}" ;;
        "WARN")  echo -e "${YELLOW}[$timestamp] WARN: $message${NC}" ;;
        "ERROR") echo -e "${RED}[$timestamp] ERROR: $message${NC}" ;;
        "DEBUG") echo -e "${BLUE}[$timestamp] DEBUG: $message${NC}" ;;
        *)       echo -e "[$timestamp] $level: $message" ;;
    esac
    
    # 写入日志文件（无颜色）
    echo "[$timestamp] $level: $message" >> "$LOG_FILE"
}

# 记录测试开始
log_message "INFO" "测试开始 - 日志文件: $LOG_FILE"

# 运行所有benchmark的函数
run_all_benchmarks() {
    log_message "INFO" "开始运行所有benchmark测试"
    echo -e "${BLUE}========================================${NC}"
    echo -e "${BLUE}运行所有benchmark测试${NC}"
    echo -e "${BLUE}========================================${NC}"
    
    local w_values=(10 20 30 40)
    local benchmarks=(tiny small_dense med_sparse med_dense lg_sparse large_dense huge xl)
    local total_tests=0
    local successful_tests=0
    
    for benchmark in "${benchmarks[@]}"; do
        for w in "${w_values[@]}"; do
            echo -e "\n${YELLOW}测试: $benchmark (W=$w)${NC}"
            log_message "INFO" "开始测试: $benchmark (W=$w)"
            
            if [ -f "$BENCHMARK_DIR/$benchmark" ]; then
                local start_time=$(date +%s)
                
                # 运行测试并捕获输出
                local output=$(time $EXECUTABLE "$BENCHMARK_DIR/$benchmark" "$w" 2>&1)
                local exit_code=$?
                
                local end_time=$(date +%s)
                local duration=$((end_time - start_time))
                
                # 记录详细结果到日志文件
                echo "========================================" >> "$LOG_FILE"
                echo "测试: $benchmark (W=$w)" >> "$LOG_FILE"
                echo "开始时间: $(date '+%Y-%m-%d %H:%M:%S')" >> "$LOG_FILE"
                echo "持续时间: ${duration}秒" >> "$LOG_FILE"
                echo "退出码: $exit_code" >> "$LOG_FILE"
                echo "输出:" >> "$LOG_FILE"
                echo "$output" >> "$LOG_FILE"
                echo "========================================" >> "$LOG_FILE"
                
                if [ $exit_code -eq 0 ]; then
                    echo -e "${GREEN}完成: $benchmark (W=$w) - 耗时: ${duration}秒${NC}"
                    log_message "INFO" "测试成功: $benchmark (W=$w) - 耗时: ${duration}秒"
                    ((successful_tests++))
                else
                    echo -e "${RED}失败: $benchmark (W=$w) - 耗时: ${duration}秒${NC}"
                    log_message "WARN" "测试失败: $benchmark (W=$w) - 耗时: ${duration}秒"
                fi
                
                ((total_tests++))
            else
                echo -e "${RED}跳过: $benchmark 文件不存在${NC}"
                log_message "WARN" "跳过测试: $benchmark 文件不存在"
            fi
            echo "----------------------------------------"
        done
    done
    
    # 显示测试总结
    echo -e "${BLUE}========================================${NC}"
    echo -e "${BLUE}测试完成总结${NC}"
    echo -e "${BLUE}========================================${NC}"
    echo -e "总测试数: $total_tests"
    echo -e "成功测试: ${GREEN}$successful_tests${NC}"
    echo -e "失败测试: ${RED}$((total_tests - successful_tests))${NC}"
    echo -e "成功率: $(echo "scale=2; $successful_tests * 100 / $total_tests" | bc -l 2>/dev/null || echo "N/A")%"
    
    log_message "INFO" "测试完成总结 - 总测试: $total_tests, 成功: $successful_tests, 失败: $((total_tests - successful_tests))"
}

# 运行单个测试的函数
run_single_test() {
    local benchmark_name=$1
    local w_value=$2
    local output_dir=${3:-$DEFAULT_VISUAL_DIR}
    local log_name=${4:-}
    
    local benchmark_file="$BENCHMARK_DIR/$benchmark_name"
    local log_base

    if [ -z "$log_name" ]; then
        log_base=$(basename "$output_dir")
        LOG_FILE="$LOG_DIR/${log_base}_$(date '+%Y%m%d_%H%M%S').log"
    else
        LOG_FILE="$LOG_DIR/${log_name}.log"
    fi
    
    log_message "INFO" "开始单个测试: $benchmark_name (W=$w_value)"
    
    # 检查benchmark文件是否存在
    if [ ! -f "$benchmark_file" ]; then
        echo -e "${RED}错误: Benchmark文件 $benchmark_file 不存在${NC}"
        log_message "ERROR" "Benchmark文件不存在: $benchmark_file"
        echo -e "${YELLOW}可用的benchmark文件:${NC}"
        ls -1 "$BENCHMARK_DIR" | sed 's/^/  /'
        exit 1
    fi
    
    # 显示测试信息
    echo -e "${BLUE}========================================${NC}"
    echo -e "${BLUE}FPGA路由测试${NC}"
    echo -e "${BLUE}========================================${NC}"
    echo -e "Benchmark文件: ${GREEN}$benchmark_name${NC}"
    echo -e "W参数: ${GREEN}$w_value${NC}"
    echo -e "输出目录: ${GREEN}$output_dir${NC}"
    echo -e "日志文件: ${GREEN}$LOG_FILE${NC}"
    echo -e "命令: ${YELLOW}$EXECUTABLE $benchmark_file $w_value $output_dir${NC}"
    echo -e "${BLUE}========================================${NC}"
    
    # 记录测试信息到日志
    echo "========================================" >> "$LOG_FILE"
    echo "单个测试: $benchmark_name (W=$w_value)" >> "$LOG_FILE"
    echo "开始时间: $(date '+%Y-%m-%d %H:%M:%S')" >> "$LOG_FILE"
    echo "命令: $EXECUTABLE $benchmark_file $w_value $output_dir" >> "$LOG_FILE"
    echo "========================================" >> "$LOG_FILE"
    
    # 运行测试
    echo -e "${YELLOW}开始测试...${NC}"
    
    # 记录开始时间
    local start_time=$(date +%s)
    
    # 运行测试并同时输出到终端和日志文件
    $EXECUTABLE "$benchmark_file" "$w_value" "$output_dir" 2>&1 | tee -a "$LOG_FILE"
    local exit_code=${PIPESTATUS[0]}
    
    # 记录结束时间并计算运行时间
    local end_time=$(date +%s)
    local duration=$((end_time - start_time))
    
    # 显示运行时间
    echo -e "${BLUE}运行时间: ${duration}秒${NC}"
    echo "运行时间: ${duration}秒" >> "$LOG_FILE"
    
    # 检查退出状态
    if [ $exit_code -eq 0 ]; then
        echo -e "${GREEN}测试完成成功!${NC}"
        log_message "INFO" "单个测试成功: $benchmark_name (W=$w_value), 运行时间: ${duration}秒"
    else
        echo -e "${RED}测试失败!${NC}"
        log_message "ERROR" "单个测试失败: $benchmark_name (W=$w_value) - 退出码: $exit_code, 运行时间: ${duration}秒"
        exit 1
    fi
}

# 显示帮助信息
show_help() {
    echo -e "${YELLOW}用法: $0 [benchmark_name] [W_value] [VISUAL_DIR]${NC}"
    echo -e "${YELLOW}       $0 all${NC}"
    echo -e "${YELLOW}示例: $0 lg_sparse 20${NC}"
    echo -e "${YELLOW}      $0 tiny 10${NC}"
    echo -e "${YELLOW}      $0 all${NC}"
    echo -e "\n${BLUE}可用的benchmark文件:${NC}"
    ls -1 "$BENCHMARK_DIR" | sed 's/^/  /'
}

# 主逻辑
if [ $# -eq 0 ]; then
    # 没有参数，显示帮助
    show_help
elif [ "$1" == "all" ]; then
    # 运行所有测试
    run_all_benchmarks
elif [ $# -eq 1 ]; then
    # 只有一个参数，使用默认值
    run_single_test "$1" "20"
elif [ $# -eq 2 ]; then
    # 两个参数
    run_single_test "$1" "$2"
elif [ $# -eq 3 ]; then
    # 三个参数：指定输出目录
    run_single_test "$1" "$2" "$3"
elif [ $# -eq 4 ]; then
    # 四个参数：指定输出目录和日志文件名
    run_single_test "$1" "$2" "$3" "$4"
else
    echo -e "${RED}参数过多!${NC}"
    show_help
    exit 1
fi