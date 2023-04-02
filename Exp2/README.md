# 程序介绍
  该程序为通过逆向分析重新实现的天气查询系统，可以连接服务器查询未来一天或几天的天气状况。
# 程序环境
  Ubuntu 20.04
  
  g++ 9.4.0

  cmake 3.16  
# 目录结构
```
.
├── 201220064_lab02_report.pdf //实验报告
├── build
│   ├── ...                    //其他
│   ├── Makefile               //cmake生成的makefile文件
│   └── MyWeatherClient        //生成的可执行文件，程序本身
├── CMakeLists.txt             //cmakelist文件
├── README.md                  //readme文档
├── trace.pcapng               //进行协议逆向分析使用的Wireshark dump文件
├── wea_cli.cpp                //程序源文件
└── wea_cli.h                  //程序头文件
```
# 运行说明
1. 在build目录下使用cmake生成makefile文件
   
    cd build

    cmake ..
2. make              //在build目录下生成可执行文件
3. ./MyWeatherClient //运行程序
