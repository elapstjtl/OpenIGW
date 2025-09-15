# Southbound Service

南向服务主程序，负责加载和管理各种协议适配器插件。

## 功能特性

- **插件化架构**: 支持动态加载各种协议适配器插件
- **多设备管理**: 同时管理多个不同协议的设备
- **配置驱动**: 通过配置文件管理设备和插件
- **异步通信**: 支持同步和异步数据读写
- **守护进程**: 支持守护进程模式运行
- **信号处理**: 支持优雅关闭和配置重载

## 架构组件

### 核心类

- **SouthboundService**: 主服务类，负责整体服务管理
- **PluginManager**: 插件管理器，负责动态加载和管理插件
- **ConfigManager**: 配置管理器，负责解析配置文件

### 插件接口

服务通过 `southbound-api` 接口与插件通信：
- `IAdapter`: 适配器接口
- `Factory`: 插件工厂函数
- `Types`: 通用数据类型定义

## 配置文件

配置文件采用INI格式，支持以下配置项：

### 全局配置
- `plugin_dir`: 插件目录路径
- `log_level`: 日志级别 (0=ERROR, 1=INFO, 2=DEBUG)
- `daemon_mode`: 是否守护进程模式

### 设备配置
每个设备用 `[设备名称]` 段配置：
- `adapter_type`: 适配器类型（插件名称）
- 适配器特定配置参数
- `tag`: 设备标签定义

## 使用方法

### 命令行选项

```bash
southbound-service [OPTIONS]

选项:
  -c, --config FILE    指定配置文件路径
  -d, --daemon         以守护进程模式运行
  -v, --verbose        详细输出
  -h, --help           显示帮助信息
  -V, --version        显示版本信息
```

### 示例

```bash
# 使用默认配置文件启动
southbound-service

# 指定配置文件启动
southbound-service -c /etc/southbound/southbound.conf

# 守护进程模式启动
southbound-service -d -v

# 显示帮助
southbound-service -h
```

### 信号处理

- `SIGINT/SIGTERM`: 优雅关闭服务
- `SIGHUP`: 重新加载配置文件

## 插件开发

要开发新的协议适配器插件，需要：

1. 实现 `IAdapter` 接口
2. 提供 `create_adapter()` 和 `destroy_adapter()` 工厂函数
3. 编译为动态库 (.so 文件)
4. 放置在插件目录中

## 构建

使用CMake构建：

```bash
mkdir build
cd build
cmake ..
make
```

## 安装

```bash
make install
```

安装后：
- 可执行文件: `/usr/bin/southbound-service`
- 插件目录: `/usr/lib/southbound/plugins/`
- 配置目录: `/etc/southbound/`
- 示例配置: `/etc/southbound/southbound.conf`
