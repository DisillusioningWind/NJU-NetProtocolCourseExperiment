# 编译器设定和编译选项
CXX = g++
CXXFLAGS = -std=c++11

# 目标文件
SERVER = server
CLIENT = client
TARGET = $(SERVER) $(CLIENT)
SERVERFILES = $(shell find server_nb -name "*.cpp")
CLIENTFILES = $(shell find client_game -name "*.cpp")

# 生成目标文件
build: $(TARGET)

$(SERVER): $(SERVERFILES)
	$(CXX) $(CXXFLAGS) -o $@ $^ -lpthread

$(CLIENT): $(CLIENTFILES)
	$(CXX) $(CXXFLAGS) -o $@ $^

# 伪目标
.PHONY: clean runServer runClient test

# 测试
runServer:
	@./$(SERVER)

runClient:
	@./$(CLIENT)

# 清除目标文件
clean:
	rm -rf $(SERVER) $(CLIENT)

# 测试
test:
	@echo $(SERVERFILES)
