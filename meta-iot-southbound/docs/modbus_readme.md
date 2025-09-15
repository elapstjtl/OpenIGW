### Modbus 适配器核心类型文档

本文件说明项目中与 Modbus 读写直接相关的三类核心类型：`DataValue`、`DeviceTag`、`StatusCode`。

### DataValue
- **用途**: 承载一次读/写操作中的数据值、时间戳与质量标记。
- **字段**:
  - **value**: 变体类型，依据功能码与 `data_type` 取不同形态。
    - 线圈/离散输入（FC1/FC2）: `bool`
    - 保持/输入寄存器（FC3/FC4）: 根据 `data_type` 为下列之一
      - `int16` -> `int32_t`（以 16 位有符号数解释并提升）
      - `uint16` -> `uint32_t`（以 16 位无符号数解释并提升）
      - `int32` -> `int32_t`（需要 2 个寄存器）
      - `uint32` -> `uint32_t`（需要 2 个寄存器）
      - `float32` -> `float`（需要 2 个寄存器）
    - 若未配置 `data_type`，默认按 16 位有符号数处理并存入 `int32_t`。
  - **timestamp_ms**: `int64`，Unix epoch 毫秒时间戳（读取成功时设置）。
  - **quality**: `int`，数据质量标记；当前实现中读取成功时置为 `1`（Good）。
- **组合规则（32 位类型）**:
  - 高 16 位在 `data[0]`，低 16 位在 `data[1]`，组合为 `(data[0] << 16) | data[1]`。
- **写入约定（当前实现）**:
  - FC5/FC15 写线圈: 使用 `bool` 值
  - FC6/FC16 写寄存器: 使用 `int32_t` 值；当前对 FC16 的实现简化为仅写入“低 16 位”到单个寄存器

### DeviceTag
- **用途**: 描述一个可读/可写的数据点（线圈、离散输入、输入/保持寄存器）。
- **结构**: `attributes` 为 `map<string,string>`，关键键值如下：
  - **register_address**: 必填，寄存器/线圈地址（十进制整数字符串）
  - **register_count**: 可选，寄存器数量（默认 `1`）。当 `data_type` 为 32 位类型（`int32`/`uint32`/`float32`）时建议设为 `2`。
  - **function_code**: 可选，功能码（默认 `3`）。常用值：
    - `1`: 读线圈（Read Coils）
    - `2`: 读离散输入（Read Discrete Inputs）
    - `3`: 读保持寄存器（Read Holding Registers）
    - `4`: 读输入寄存器（Read Input Registers）
    - `5`: 写单线圈（Write Single Coil）
    - `6`: 写单寄存器（Write Single Register）
    - `15`: 写多个线圈（当前实现简化为写单点）
    - `16`: 写多个寄存器（当前实现简化为写入单个寄存器的低 16 位）
  - **data_type**: 可选，寄存器数据类型解释（影响读时的组包与类型转换）。支持：`int16`、`uint16`、`int32`、`uint32`、`float32`。未设置时按 16 位有符号数处理。
- **示例**（读取保持寄存器中的 32 位浮点，地址 100，2 个寄存器）：
```json
{
  "attributes": {
    "register_address": "100",
    "register_count": "2",
    "function_code": "3",
    "data_type": "float32"
  }
}
```

### StatusCode
- **用途**: 标识适配器操作结果状态。
- **取值及含义**:
  - **OK**: 操作成功
  - **AlreadyConnected**: 已处于连接/初始化状态（上下文相关）
  - **NotInitialized**: 未初始化即发起需要初始化前置条件的操作
  - **NotConnected**: 未连接即发起需要连接前置条件的操作
  - **BadConfig**: 配置缺失或非法（如缺少 `connection_type`、`ip_address`/`device_path` 等）
  - **InvalidParam**: 标签或参数无效（如缺少 `register_address`，或 `register_count` 不满足数据类型要求）
  - **NotSupported**: 不支持的功能/操作（如未实现的功能码处理）
  - **Error**: 运行时错误（如底层 libmodbus 调用失败）

### 备注
- 32 位类型的字序采用“高字在前（高 16 位）”，与代码中 `(data[0] << 16) | data[1]` 一致。
- 订阅接口会以 1 秒周期轮询已订阅 `DeviceTag` 并在成功时回调返回 `map<DeviceTag, DataValue>`。
- 实际底层通讯由 libmodbus 完成，当前文件仅对其进行参数解析、上下文管理、线程与数据类型转换的封装。
