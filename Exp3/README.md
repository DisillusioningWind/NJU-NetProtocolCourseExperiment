# 程序介绍

该程序是一个基于TCP协议的多人在线猜拳游戏，实现了客户端和服务器的基本功能，包括：

- 客户端
  - 连接服务器
  - 登录
  - 查看在线用户
  - 开始游戏
  - 退出游戏
- 服务器
  - 监听客户端连接
  - 查看所有用户
  - 查看所有对局信息

# 程序环境

Ubuntu 20.04

g++ 9.4.0

# 目录结构
```
.
├── client_game                // 客户端源码目录
│   ├── client_game.cpp        // 客户端游戏逻辑实现
│   ├── client_game.h          // 客户端游戏逻辑头文件
│   └── client_main.cpp        // 客户端主函数
├── server_nb                  // 服务器源码目录
│   ├── server_main.cpp        // 服务器主函数
│   ├── server_nb.cpp          // 服务器逻辑实现
│   └── server_nb.h            // 服务器逻辑头文件
├── 201220064_lab03_report.pdf // 实验报告
├── client                     // 客户端程序可执行文件
├── common.h                   // 公共头文件
├── makefile                   // makefile
├── README.md                  // 说明文档
└── server                     // 服务器程序可执行文件
```
# 运行说明
## 编译
    make
    或
    make build
## 运行
### 服务器
    ./server
    或
    make runServer
### 客户端
    ./client [ip]   // ip为服务器ip地址, 默认为127.0.0.1，即本机地址，可选
    或
    make runClient
## 清理
    make clean