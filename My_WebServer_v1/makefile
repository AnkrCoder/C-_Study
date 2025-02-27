# 定义编译器
CXX = g++

# 定义编译选项
CXXFLAGS = -std=c++17 -Wall -I. -pthread

# 定义链接选项
LDFLAGS = -pthread

# 定义源文件目录
SRC_DIRS = Epoll_Reactor HTTP_Connection Logger Master_Worker

# 定义源文件
SRCS = $(shell find $(SRC_DIRS) -name '*.cpp') server.cpp

# 定义目标文件
OBJS = $(SRCS:.cpp=.o)

# 定义可执行文件名
TARGET = server

# 默认目标
all: $(TARGET)

# 链接目标文件生成可执行文件
$(TARGET): $(OBJS)
	$(CXX) $(LDFLAGS) -o $@ $^

# 编译源文件生成目标文件
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# 清理生成的文件
clean:
	rm -f $(OBJS) $(TARGET)

# 伪目标，用于显示帮助信息
.PHONY: all clean