# Makefile for LB_Server with main function
# 编译器设置
CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++17 -g

# 链接库
LIBS = -lVortex-Net -lpthread

# 源文件
SRCS = main.cc RemoteClient.cc Selector.cc CheckerTask.cc HealthChecker.cc

# 目标文件
OBJS = $(SRCS:.cc=.o)

# 目标可执行文件
TARGET = LB_Server

# 默认目标
all: $(TARGET)

# 链接目标文件生成可执行文件
$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LIBS)
	rm -f $(OBJS)

# 编译源文件生成目标文件
%.o: %.cc
	$(CXX) $(CXXFLAGS) -c $< -o $@

# h_test 目标文件
H_TEST_OBJS = h_test.o Selector.o CheckerTask.o HealthChecker.o

# h_test 可执行文件
h_test: $(H_TEST_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LIBS)
	rm -f $(H_TEST_OBJS)

# 清理目标
clean:
	rm -f $(OBJS) $(TARGET) $(H_TEST_OBJS) h_test

# 伪目标
.PHONY: all clean h_test