#include "ModbusAdapter.hpp"
#include <iostream>
#include <chrono>
#include <cstring>
#include <vector>

namespace southbound {

/**
 * 构造函数
 * 初始化默认通信参数与资源句柄。
 */
ModbusAdapter::ModbusAdapter() 
    : m_modbus_ctx(nullptr, modbus_free)
    , m_port(502)
    , m_slave_id(1)
    , m_baudrate(9600)
    , m_parity('N')
    , m_data_bits(8)
    , m_stop_bits(1) {
}

/**
 * 析构函数
 * 关闭连接并安全停止订阅线程。
 */
ModbusAdapter::~ModbusAdapter() {
    disconnect();
    if (m_subscription_thread.joinable()) {
        m_subscription_active = false;
        m_subscription_thread.join();
    }
}

/**
 * 初始化适配器
 * @param config 适配器配置（连接类型、地址/串口、从站ID等）
 * @return StatusCode::OK 初始化成功；其他为错误码
 */
StatusCode ModbusAdapter::init(const AdapterConfig& config) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_initialized) { // 已经初始化过
        return StatusCode::AlreadyConnected;
    }
    
    StatusCode result = parse_config(config);
    if (result != StatusCode::OK) {
        return result;
    }
    
    result = create_modbus_context();
    if (result != StatusCode::OK) {
        return result;
    }
    
    m_initialized = true;
    return StatusCode::OK;
}

/**
 * 建立与设备的连接
 * 需在 init 成功后调用。
 * @return StatusCode::OK 连接成功；AlreadyConnected/NotInitialized/Error 等
 */
StatusCode ModbusAdapter::connect() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (!m_initialized) {
        return StatusCode::NotInitialized;
    }
    
    if (m_connected) {
        return StatusCode::AlreadyConnected;
    }
    
    if (!m_modbus_ctx) {
        return StatusCode::Error;
    }
    
    int result = -1;
    if (m_connection_type == "tcp") {
        result = modbus_connect(m_modbus_ctx.get());
    } else if (m_connection_type == "rtu") {
        result = modbus_connect(m_modbus_ctx.get());
    }
    
    if (result == -1) {
        return StatusCode::Error;
    }
    
    m_connected = true;
    return StatusCode::OK;
}

/**
 * 断开连接并释放上下文
 * @return StatusCode::OK 总是返回成功
 */
StatusCode ModbusAdapter::disconnect() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_modbus_ctx) {
        modbus_close(m_modbus_ctx.get());
        m_modbus_ctx.reset();
    }
    
    m_connected = false;
    return StatusCode::OK;
}

/**
 * 批量读取设备标签数据
 * @param tags 待读取的设备标签列表
 * @param values 输出读取到的数据值列表，与 tags 一一对应
 * @return StatusCode::OK 成功；NotConnected/Error/InvalidParam 等
 */
StatusCode ModbusAdapter::read(const std::vector<DeviceTag>& tags, std::vector<DataValue>& values) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (!m_connected) {
        return StatusCode::NotConnected;
    }
    
    values.clear();
    values.reserve(tags.size());
    
    for (const auto& tag : tags) {
        DataValue value;
        StatusCode result = read_register(tag, value);
        if (result != StatusCode::OK) {
            return result;
        }
        values.push_back(value);
    }
    
    return StatusCode::OK;
}

/**
 * 批量写入设备标签数据
 * @param tags_and_values 待写入的标签与数值映射
 * @return StatusCode::OK 成功；NotConnected/Error/NotSupported/InvalidParam 等
 */
StatusCode ModbusAdapter::write(const std::map<DeviceTag, DataValue>& tags_and_values) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (!m_connected) {
        return StatusCode::NotConnected;
    }
    
    for (const auto& pair : tags_and_values) {
        StatusCode result = write_register(pair.first, pair.second);
        if (result != StatusCode::OK) {
            return result;
        }
    }
    
    return StatusCode::OK;
}

/**
 * 订阅一组标签并按周期回调
 * 内部启动轮询线程，每秒读取一次已订阅标签并通过回调返回。
 * @param tags 订阅的设备标签列表
 * @param callback 数据到达时回调，参数为标签与数值映射
 * @return StatusCode::OK 成功；NotConnected 等
 */
StatusCode ModbusAdapter::subscribe(const std::vector<DeviceTag>& tags, OnDataReceivedCallback callback) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (!m_connected) {
        return StatusCode::NotConnected;
    }
    
    m_subscribed_tags = tags;
    m_callback = callback;
    
    if (m_subscription_thread.joinable()) {
        m_subscription_active = false;
        m_subscription_thread.join();
    }
    
    m_subscription_active = true;
    m_subscription_thread = std::thread(&ModbusAdapter::subscription_worker, this);
    
    return StatusCode::OK;
}

/**
 * 手动停止订阅线程
 * 停止当前运行的订阅线程并清理相关资源
 * @return StatusCode::OK 成功停止
 */
StatusCode ModbusAdapter::unsubscribe() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_subscription_thread.joinable()) {
        m_subscription_active = false;
        m_subscription_thread.join();
    }
    
    m_subscribed_tags.clear();
    m_callback = nullptr;
    
    return StatusCode::OK;
}


/**
 * 获取当前连接状态
 * @return NotInitialized/NotConnected/OK
 */
StatusCode ModbusAdapter::get_status() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (!m_initialized) {
        return StatusCode::NotInitialized;
    }
    
    if (!m_connected) {
        return StatusCode::NotConnected;
    }
    
    return StatusCode::OK;
}

/**
 * 解析适配器配置
 * @param config 输入配置
 * @return StatusCode::OK 成功；BadConfig 等
 */
StatusCode ModbusAdapter::parse_config(const AdapterConfig& config) {
    // 1. 获取必需参数：连接类型
    if (!try_get_config_value(config, "connection_type", m_connection_type)) {
        return StatusCode::BadConfig; // 如果没有 "connection_type"，配置无效
    }

    // 2. 根据连接类型，获取各自的必需和可选参数
    if (m_connection_type == "tcp") {
        // TCP 必需参数
        if (!try_get_config_value(config, "ip_address", m_ip_address)) {
            return StatusCode::BadConfig;
        }
        // TCP 可选参数 (如果获取失败，则使用默认值)
        try_get_config_value(config, "port", m_port);

    } else if (m_connection_type == "rtu") {
        // RTU 必需参数
        if (!try_get_config_value(config, "device_path", m_device_path)) {
            return StatusCode::BadConfig;
        }
        // RTU 可选参数
        try_get_config_value(config, "baudrate", m_baudrate);
        try_get_config_value(config, "parity", m_parity);
        try_get_config_value(config, "data_bits", m_data_bits);
        try_get_config_value(config, "stop_bits", m_stop_bits);
        
    } else {
        return StatusCode::BadConfig; // 不支持的连接类型
    }

    // 3. 获取所有连接类型都共用的可选参数
    try_get_config_value(config, "slave_id", m_slave_id);
    
    return StatusCode::OK;
}

/**
 * 创建并配置 libmodbus 上下文
 * 根据连接类型创建 TCP/RTU 上下文，设置从站ID与超时。
 * @return StatusCode::OK 成功；BadConfig/Error 等
 */
StatusCode ModbusAdapter::create_modbus_context() {
    if (m_connection_type == "tcp") {
        m_modbus_ctx.reset(modbus_new_tcp(m_ip_address.c_str(), m_port));
    } else if (m_connection_type == "rtu") {
        m_modbus_ctx.reset(modbus_new_rtu(m_device_path.c_str(), m_baudrate, m_parity, m_data_bits, m_stop_bits));
    } else {
        return StatusCode::BadConfig;
    }
    
    if (!m_modbus_ctx) {
        return StatusCode::Error;
    }
    
    // 设置从站 ID
    modbus_set_slave(m_modbus_ctx.get(), m_slave_id);
    
    // 设置超时
    modbus_set_response_timeout(m_modbus_ctx.get(), 1, 0); // 1秒超时
    
    return StatusCode::OK;
}

/**
 * 读取单个标签对应的寄存器/线圈
 * 根据 function_code 调用不同的 Modbus 读函数，并按 data_type 转换值。
 * @param tag 设备标签（包含地址、数量、功能码、数据类型等）
 * @param value 输出读取到的数据值（含时间戳、质量）
 * @return StatusCode::OK 成功；InvalidParam/NotSupported/Error 等
 */
StatusCode ModbusAdapter::read_register(const DeviceTag& tag, DataValue& value) {
    auto it = tag.attributes.find("register_address");
    if (it == tag.attributes.end()) {
        return StatusCode::InvalidParam;
    }
    
    int address = std::stoi(it->second);
    int count = get_register_count(tag);
    int function_code = get_function_code(tag);
    
    std::vector<uint16_t> data(count > 0 ? count : 1);
    int result = -1;
    
    switch (function_code) {
        case 3: // 读保持寄存器
            result = modbus_read_registers(m_modbus_ctx.get(), address, count, data.data());
            break;
        case 4: // 读输入寄存器
            result = modbus_read_input_registers(m_modbus_ctx.get(), address, count, data.data());
            break;
        case 1: // 读线圈
            {
                std::vector<uint8_t> coil_data(count > 0 ? count : 1);
                result = modbus_read_bits(m_modbus_ctx.get(), address, count, coil_data.data());
                if (result == count) {
                    value.value = static_cast<bool>(coil_data[0]);
                }
            }
            break;
        case 2: // 读离散输入
            {
                std::vector<uint8_t> input_data(count > 0 ? count : 1);
                result = modbus_read_input_bits(m_modbus_ctx.get(), address, count, input_data.data());
                if (result == count) {
                    value.value = static_cast<bool>(input_data[0]);
                }
            }
            break;
        default:
            return StatusCode::NotSupported;
    }
    
    if (result == count && (function_code == 3 || function_code == 4)) {
        // 根据数据类型转换值
        auto data_type_it = tag.attributes.find("data_type");
        if (data_type_it != tag.attributes.end()) {
            if (data_type_it->second == "int16") {
                value.value = static_cast<int32_t>(static_cast<int16_t>(data[0]));
            } else if (data_type_it->second == "uint16") {
                value.value = static_cast<uint32_t>(data[0]);
            } else if (data_type_it->second == "int32") {
                if (count >= 2) {
                    int32_t val = (static_cast<int32_t>(data[0]) << 16) | data[1];
                    value.value = val;
                } else {
                    return StatusCode::InvalidParam;
                }
            } else if (data_type_it->second == "uint32") {
                if (count >= 2) {
                    uint32_t val = (static_cast<uint32_t>(data[0]) << 16) | data[1];
                    value.value = val;
                } else {
                    return StatusCode::InvalidParam;
                }
            } else if (data_type_it->second == "float32") {
                if (count >= 2) {
                    uint32_t val = (static_cast<uint32_t>(data[0]) << 16) | data[1];
                    float fval;
                    std::memcpy(&fval, &val, sizeof(float));
                    value.value = fval;
                } else {
                    return StatusCode::InvalidParam;
                }
            } else {
                value.value = static_cast<int32_t>(data[0]);
            }
        } else {
            value.value = static_cast<int32_t>(data[0]);
        }
    }
    
    if (result != count) {
        return StatusCode::Error;
    }
    
    // 设置时间戳和质量
    value.timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    value.quality = 1; // Good
    
    return StatusCode::OK;
}

/**
 * 写入单个标签对应的寄存器/线圈
 * 根据 function_code 调用不同的 Modbus 写函数（简化实现）。
 * @param tag 设备标签（包含地址与功能码等）
 * @param value 待写入的值
 * @return StatusCode::OK 成功；NotSupported/Error/InvalidParam 等
 */
StatusCode ModbusAdapter::write_register(const DeviceTag& tag, const DataValue& value) {
    auto it = tag.attributes.find("register_address");
    if (it == tag.attributes.end()) {
        return StatusCode::InvalidParam;
    }
    
    int address = std::stoi(it->second);
    int function_code = get_function_code(tag);
    
    int result = -1;
    
    switch (function_code) {
        case 5: // 写单个线圈
            {
                bool coil_value = std::get<bool>(value.value);
                result = modbus_write_bit(m_modbus_ctx.get(), address, coil_value ? 1 : 0);
            }
            break;
        case 6: // 写单个寄存器
            {
                int32_t reg_value32 = std::get<int32_t>(value.value);
                uint16_t reg_value = static_cast<uint16_t>(reg_value32 & 0xFFFF);
                result = modbus_write_register(m_modbus_ctx.get(), address, reg_value);
            }
            break;
        case 15: // 写多个线圈（简化：写单个）
            {
                bool coil_value = std::get<bool>(value.value);
                result = modbus_write_bit(m_modbus_ctx.get(), address, coil_value ? 1 : 0);
            }
            break;
        case 16: // 写多个寄存器（简化：写单个低16位）
            {
                int32_t reg_value32 = std::get<int32_t>(value.value);
                uint16_t reg_value = static_cast<uint16_t>(reg_value32 & 0xFFFF);
                result = modbus_write_register(m_modbus_ctx.get(), address, reg_value);
            }
            break;
        default:
            return StatusCode::NotSupported;
    }
    
    if (result == -1) {
        return StatusCode::Error;
    }
    
    return StatusCode::OK;
}

/**
 * 订阅线程工作函数
 * 每秒轮询一次已订阅标签，读取成功则触发回调。
 */
void ModbusAdapter::subscription_worker() {
    while (m_subscription_active) {
        std::this_thread::sleep_for(m_poll_interval); // 设置轮询时间
        
        // 已连接且回调函数已设置
        if (!m_connected || !m_callback) {
            continue;
        }
        
        std::map<DeviceTag, DataValue> values;
        bool has_data = false;
        
        for (const auto& tag : m_subscribed_tags) {
            DataValue value;
            if (read_register(tag, value) == StatusCode::OK) {
                values[tag] = value;
                has_data = true;
            }
        }
        
        if (has_data) {
            m_callback(values);
        }
    }
}

/**
 * 设置订阅线程的轮询时间
 * @param interval 轮询时间 1ms为单位
 */
void ModbusAdapter::set_poll_interval(std::chrono::milliseconds interval) {
    m_poll_interval = interval;
}

/**
 * 获取标签的寄存器地址
 * @param tag 设备标签
 * @return 地址，未配置则返回 0
 */
int ModbusAdapter::get_register_address(const DeviceTag& tag) {
    auto it = tag.attributes.find("register_address");
    if (it != tag.attributes.end()) {
        return std::stoi(it->second);
    }
    return 0;
}

/**
 * 获取标签需要读取的寄存器数量
 * @param tag 设备标签
 * @return 数量，未配置则默认 1
 */
int ModbusAdapter::get_register_count(const DeviceTag& tag) {
    auto it = tag.attributes.find("register_count");
    if (it != tag.attributes.end()) {
        return std::stoi(it->second);
    }
    return 1;
}

/**
 * 获取标签对应的功能码
 * @param tag 设备标签
 * @return 功能码，未配置则默认 3（读保持寄存器）
 */
int ModbusAdapter::get_function_code(const DeviceTag& tag) {
    auto it = tag.attributes.find("function_code");
    if (it != tag.attributes.end()) {
        return std::stoi(it->second);
    }
    return 3; // 默认读保持寄存器
}

} // namespace southbound