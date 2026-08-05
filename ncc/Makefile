# ============================================================
# LEGACY 构建入口（已弃用）
#
# 本项目官方构建系统为 xmake（xmake.lua）：
#   xmake [-r]                 # 构建 ncc（自动探测 tcc/NIHAO_TCC_DIR）
#   xmake test [--all] [-b c|native|ir-c|ir-native]   # 回归测试
#
# 本 Makefile 仅为历史兼容保留，存在已知缺陷：
#   - 不解析 libtcc.h include 路径（native.c 编译失败）
#   - make test 内的用例是旧语法
# 请不要在本项目中使用 `make`，改用 xmake。
# ============================================================

# 编译器和编译选项
CC := tcc
CFLAGS := -Wall -Wextra -O2 -std=c99

# 目标可执行文件名称
TARGET := ncc.exe

# 目录设置
BUILD_DIR := build
SRC_DIR := .

# 自动获取所有.c源文件
SRCS := $(wildcard $(SRC_DIR)/*.c)
OBJS := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRCS))
DEPS := $(OBJS:.o=.d)  # 依赖文件

# 默认目标
all: $(BUILD_DIR)/$(TARGET)

# 链接生成可执行文件
$(BUILD_DIR)/$(TARGET): $(OBJS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $^

# 编译每个.c文件为.o文件，并生成依赖信息
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# 自动创建build目录
$(BUILD_DIR):
	mkdir -p $@

# 包含自动生成的依赖文件
-include $(DEPS)

test: $(BUILD_DIR)/$(TARGET)
	@echo "Running tests..."
	@echo 'module main use stdio const main() { puts("Hello, NihaoC!") }' > test.nh
	./$(BUILD_DIR)/$(TARGET) test.nh -o test
	@echo "Test complete."

# Development targets
debug: CFLAGS += -DDEBUG -O0
debug: all

# 清理生成的文件
.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)

.PHONY: info
info:
	@echo "源文件: $(SRCS)"
	@echo "目标文件: $(OBJS)"
