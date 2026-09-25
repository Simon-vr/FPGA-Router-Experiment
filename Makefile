# FPGA 协商布线器 —— 顶层 Makefile
# 目标：
#   make all    使用 CMake 配置并编译，产物为 ./build/main
#   make clean  删除 build 目录
#   make test   用 benchmark/tiny 跑一次 BFS 冒烟测试并检查路由成功

BUILD_DIR := build
TARGET    := $(BUILD_DIR)/main

.PHONY: all clean test

all:
	cmake -S . -B $(BUILD_DIR)
	cmake --build $(BUILD_DIR) -j

clean:
	rm -rf $(BUILD_DIR)

# 冒烟测试：tiny 电路，W=4，BFS，30 次迭代，4 线程，关闭可视化
test: $(TARGET)
	@out="$$(./$(TARGET) ./benchmark/tiny 4 /tmp/vlsi_smoke bfs 30 4 false 2>&1)"; \
	echo "$$out"; \
	if echo "$$out" | grep -q "Routing check passed"; then \
		echo "[make test] PASS"; \
	else \
		echo "[make test] FAIL: 未检测到 'Routing check passed'"; \
		exit 1; \
	fi

$(TARGET):
	cmake -S . -B $(BUILD_DIR)
	cmake --build $(BUILD_DIR) -j
