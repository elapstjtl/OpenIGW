# Modbus 适配器

这是一个实现了 southbound API 接口的 Modbus 协议适配器，支持 Modbus RTU 和 Modbus TCP 两种连接方式。

## 功能特性

- 支持 Modbus RTU (串口) 和 Modbus TCP (以太网) 连接
- 实现完整的 southbound API 接口
- 支持同步读写操作
- 支持异步数据订阅
- 支持多种数据类型：线圈、离散输入、保持寄存器、输入寄存器

## 配置参数

### Modbus TCP 配置
```json
{
    "connection_type": "tcp",
    "ip_address": "192.168.1.100",
    "port": "502",
    "slave_id": "1"
}
```

### Modbus RTU 配置
```json
{
    "connection_type": "rtu",
    "device_path": "/dev/ttyUSB0",
    "baudrate": "9600",
    "parity": "N",
    "data_bits": "8",
    "stop_bits": "1",
    "slave_id": "1"
}
```

## 设备标签配置

设备标签 (DeviceTag) 需要包含以下属性：

- `register_address`: 寄存器地址 (必需)
- `function_code`: 功能码 (可选，默认为3)
  - 1: 读线圈
  - 2: 读离散输入
  - 3: 读保持寄存器
  - 4: 读输入寄存器
  - 5: 写单个线圈
  - 6: 写单个寄存器
  - 15: 写多个线圈
  - 16: 写多个寄存器
- `data_type`: 数据类型 (可选，默认为int16)
  - `int16`: 16位有符号整数
  - `uint16`: 16位无符号整数
  - `int32`: 32位有符号整数
  - `uint32`: 32位无符号整数
  - `float32`: 32位浮点数
- `register_count`: 寄存器数量 (可选，默认为1)

## 使用示例

```cpp
#include <southbound/IAdapter.hpp>
#include <southbound/Types.hpp>
#include <iostream>

// 创建适配器实例
auto adapter = std::make_unique<ModbusAdapter>();

// 配置连接参数
AdapterConfig config;
config["connection_type"] = "tcp";
config["ip_address"] = "192.168.1.100";
config["port"] = "502";
config["slave_id"] = "1";

// 初始化并连接
if (adapter->init(config) == StatusCode::OK) {
    if (adapter->connect() == StatusCode::OK) {
        // 读取数据
        DeviceTag tag;
        tag.attributes["register_address"] = "100";
        tag.attributes["function_code"] = "3";
        tag.attributes["data_type"] = "int16";
        
        std::vector<DeviceTag> tags = {tag};
        std::vector<DataValue> values;
        
        if (adapter->read(tags, values) == StatusCode::OK) {
            std::cout << "读取成功，值: " << std::get<int32_t>(values[0].value) << std::endl;
        }
        
        // 断开连接
        adapter->disconnect();
    }
}
```

## 构建说明

该适配器使用 CMake 构建系统，依赖以下库：
- southbound-api (头文件)
- libmodbus (Modbus 协议库)

构建完成后会生成 `libmodbus-adapter.so` 动态链接库文件。
