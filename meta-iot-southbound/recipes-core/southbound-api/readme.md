好的，这是一份为您准备的 `southbound-api` 需求文档。

这份文档定义了南向适配器插件必须满足的**最小核心接口**和**数据结构**，同时在设计上为未来的功能扩展留出了空间。任何协议的插件开发者，都必须严格遵守这份文档中的规范。

-----

### **南向通用适配器 API 需求文档 (Southbound Adapter API Requirements)**

| **文档版本** | 1.0 |
| :--- | :--- |
| **发布日期** | 2025年9月12日 |
| **状态** | 草案 |
| **编写目的** | 定义南向插件与插件管理器之间的标准交互接口，实现协议无关的设备接入。 |

### 1\. 概述 (Overview)

`southbound-api` 是南向协议适配器框架的核心。它是一套纯 C++ 接口（抽象基类），定义了插件的生命周期、设备连接、数据交互和状态管理等标准行为。所有具体的协议插件（如 Modbus, OPC UA）都必须继承并完整实现该接口。

### 2\. 设计原则 (Design Principles)

  * **协议无关性**: API 的设计不应包含任何特定协议的细节。所有参数和数据结构都应是通用的。
  * **接口稳定性**: 核心接口 (`IAdapter`) 一旦发布，应保持稳定。新增功能应通过新的可选接口或扩展参数实现。
  * **清晰的生命周期**: 插件的创建、初始化、销毁流程必须明确。
  * **同步与异步分离**: 提供基础的同步 I/O 操作，并为高效的异步数据订阅提供独立接口。
  * **标准化的数据模型**: 使用统一的数据结构来描述设备点位和数据值。

### 3\. 核心接口定义: `IAdapter`

这是一个 C++ 纯虚基类，是所有插件**必须**实现的接口。

```cpp
// 前置声明所需的数据结构
class DataValue;
struct DeviceTag;
enum class StatusCode;
using AdapterConfig = std::map<std::string, std::string>;
using OnDataReceivedCallback = std::function<void(const std::map<DeviceTag, DataValue>&)>;

class IAdapter {
public:
    virtual ~IAdapter() = default;

    /**
     * @brief 初始化适配器实例
     * @param config 配置参数 (如 IP, 端口, 从站ID等)
     * @return StatusCode 初始化结果状态码
     */
    virtual StatusCode init(const AdapterConfig& config) = 0;

    /**
     * @brief 连接到物理设备
     * @return StatusCode 连接结果状态码
     */
    virtual StatusCode connect() = 0;

    /**
     * @brief 断开与物理设备的连接
     * @return StatusCode 断开结果状态码
     */
    virtual StatusCode disconnect() = 0;

    /**
     * @brief [同步] 读取设备数据
     * @param tags 需要读取的点位列表
     * @param values [输出参数] 读取到的数据结果
     * @return StatusCode 操作结果状态码
     */
    virtual StatusCode read(const std::vector<DeviceTag>& tags, std::vector<DataValue>& values) = 0;

    /**
     * @brief [同步] 向设备写入数据
     * @param tags_and_values 需要写入的点位及对应的数据值
     * @return StatusCode 操作结果状态码
     */
    virtual StatusCode write(const std::map<DeviceTag, DataValue>& tags_and_values) = 0;

    /**
     * @brief [异步] 订阅数据变化
     * @param tags 需要订阅的点位列表
     * @param callback 当数据到达时被调用的回调函数
     * @return StatusCode 订阅请求是否成功提交的状态码
     */
    virtual StatusCode subscribe(const std::vector<DeviceTag>& tags, OnDataReceivedCallback callback) = 0;

    /**
     * @brief 获取当前适配器的健康状态
     * @return StatusCode 返回代表连接状态或健康状况的状态码
     */
    virtual StatusCode get_status() = 0;
};
```

### 4\. 核心数据结构定义

#### 4.1. `AdapterConfig` (适配器配置)

  * **定义**: `using AdapterConfig = std::map<std::string, std::string>;`
  * **描述**: 一个键值对集合，用于向 `init` 函数传递协议相关的配置信息。这种结构具有良好的扩展性。
  * **示例 (Modbus TCP)**:
      * `{"ip_address": "192.168.1.10"}`
      * `{"port": "502"}`
      * `{"slave_id": "1"}`

#### 4.2. `DeviceTag` (设备点位)

  * **定义**: `struct DeviceTag { std::map<std::string, std::string> attributes; };` (为了可比较，通常会实现 `<` 操作符)
  * **描述**: 一个通用的、用于描述设备上一个具体数据点（变量）的结构。内部同样使用键值对，以适应不同协议的寻址方式。
  * **示例**:
      * **Modbus**: `{"type": "holding_register", "address": "40001", "count": "1"}`
      * **OPC UA**: `{"type": "node_id", "namespace": "2", "identifier": "s=MyVariable"}`

#### 4.3. `DataValue` (数据值)

  * **定义**: `using DataValue = std::variant<bool, int32_t, uint32_t, float, double, std::string>;`
  * **描述**: 使用 C++17 的 `std::variant` 来表示一个可变类型的数据值，可以容纳常见的工业数据类型。同时应包含时间戳和质量戳信息。
  * **完整结构示例**:
    ```cpp
    struct DataValue {
        std::variant<bool, int32_t, ...> value;
        uint64_t timestamp_ms; // Unix apoch in milliseconds
        uint8_t quality;       // 0=Bad, 1=Good, etc.
    };
    ```

#### 4.4. `StatusCode` (状态码)

  * **定义**: `enum class StatusCode { OK, Error, Timeout, BadConfig, NotConnected, ... };`
  * **描述**: 一个枚举类，用于标准化所有 API 调用的返回结果，方便上层应用进行统一的错误处理。

### 5\. 插件的实例化与生命周期 (Plugin Instantiation)

插件管理器不会直接`new`一个插件类。为了实现动态加载（`.so`/.`dll` 文件），每个插件**必须**以 C 语言符号导出以下两个工厂函数：

```c
// C-style functions to be exported from the shared library
extern "C" {
    /**
     * @brief 工厂函数，创建适配器实例
     * @return 指向 IAdapter 接口的指针
     */
    IAdapter* create_adapter();

    /**
     * @brief 工厂函数，销毁适配器实例
     * @param adapter create_adapter 创建的实例指针
     */
    void destroy_adapter(IAdapter* adapter);
}
```

### 6\. 未来扩展性考虑 (Extensibility)

1.  **新增可选接口**: 未来可以定义新的接口，如 `IBrowsable`（用于支持节点浏览的协议如 OPC UA）。插件管理器可以使用 `dynamic_cast` 来检查一个插件实例是否实现了这个扩展接口，从而实现渐进式的功能增强，而不会破坏现有插件的兼容性。
2.  **扩展配置参数**: 由于 `AdapterConfig` 和 `DeviceTag` 都是 `map` 结构，未来可以向其中添加新的键值对来支持新功能，而无需修改 API 的函数签名。
3.  **API 版本控制**: `southbound-api` 本身应进行版本管理。可以在 API 中加入一个 `getVersion()` 方法，以便插件管理器了解插件是基于哪个版本的 API 构建的。