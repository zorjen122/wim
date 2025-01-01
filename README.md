# wim (dev......)
An CPP-IM

## 前提
  - Linux (最好Ubuntu20.04)
  - cmake(minnum 3.1)
  - mysqlconnect/cpp
  - hiredis
  - boost & asio
  - jsoncpp
  - grpc-cpp
  - protobuf

## 开始
共四个服务
- gate
- chat
- verify
- status

配置好mysql、redis，以及各服务器目录中的config.ini后：
```
[setp-1]
cd server
./requireStart

[setp-2]
cd [service-dir]
mkdir build

[setp-3]
cd build
cmake ..
make -j4
./your_service

[for-verify]
cd verify
node run serv
```

## 测试-cli
```
cd chat/test/
mkdir build
cmake ..
make -j4
./bin/test_exampleServic
```