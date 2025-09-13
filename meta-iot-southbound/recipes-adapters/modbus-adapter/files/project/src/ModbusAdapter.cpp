#include "ModbusAdapter.hpp"
#include <iostream>
#include <chrono>
#include <cstring>

namespace southbound {

ModbusAdapter::ModbusAdapter() 
    : m_modbus_ctx(nullptr, modbus_free)
    , m_port(502)
    , m_slave_id(1)
    , m_baudrate(9600)
    , m_parity('N')
    , m_data_bits(8)
    , m_stop_bits(1) {
}

ModbusAdapter::~ModbusAdapter() {
    disconnect();
    if (m_subscription_thread.joinable()) {
        m_subscription_active = false;
        m_subscription_thread.join();
    }
}

StatusCode ModbusAdapter::init(const AdapterConfig& config) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_initialized) {
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

StatusCode ModbusAdapter::disconnect() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_modbus_ctx) {
        modbus_close(m_modbus_ctx.get());
        m_modbus_ctx.reset();
    }
    
    m_connected = false;
    return StatusCode::OK;
}

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

StatusCode ModbusAdapter::parse_config(const AdapterConfig& config) {
    // 解析连接类型
    auto it = config.find("connection_type");
    if (it == config.end()) {
        return StatusCode::BadConfig;
    }
    m_connection_type = it->second;
    
    if (m_connection_type == "tcp") {
        // TCP 配置
        it = config.find("ip_address");
        if (it == config.end()) {
            return StatusCode::BadConfig;
        }
        m_ip_address = it->second;
        
        it = config.find("port");
        if (it != config.end()) {
            m_port = std::stoi(it->second);
        }
    } else if (m_connection_type == "rtu") {
        // RTU 配置
        it = config.find("device_path");
        if (it == config.end()) {
            return StatusCode::BadConfig;
        }
        m_device_path = it->second;
        
        it = config.find("baudrate");
        if (it != config.end()) {
            m_baudrate = std::stoi(it->second);
        }
        
        it = config.find("parity");
        if (it != config.end()) {
            m_parity = it->second[0];
        }
        
        it = config.find("data_bits");
        if (it != config.end()) {
            m_data_bits = std::stoi(it->second);
        }
        
        it = config.find("stop_bits");
        if (it != config.end()) {
            m_stop_bits = std::stoi(it->second);
        }
    } else {
        return StatusCode::BadConfig;
    }
    
    // 从站 ID
    it = config.find("slave_id");
    if (it != config.end()) {
        m_slave_id = std::stoi(it->second);
    }
    
    return StatusCode::OK;
}

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

StatusCode ModbusAdapter::read_register(const DeviceTag& tag, DataValue& value) {
    auto it = tag.attributes.find("register_address");
    if (it == tag.attributes.end()) {
        return StatusCode::InvalidParam;
    }
    
    int address = std::stoi(it->second);
    int count = get_register_count(tag);
    int function_code = get_function_code(tag);
    
    uint16_t* data = new uint16_t[count > 0 ? count : 1];
    int result = -1;
    
    switch (function_code) {
        case 3: // 读保持寄存器
            result = modbus_read_registers(m_modbus_ctx.get(), address, count, data);
            break;
        case 4: // 读输入寄存器
            result = modbus_read_input_registers(m_modbus_ctx.get(), address, count, data);
            break;
        case 1: // 读线圈
            {
                uint8_t* coil_data = new uint8_t[count > 0 ? count : 1];
                result = modbus_read_bits(m_modbus_ctx.get(), address, count, coil_data);
                if (result == count) {
                    value.value = static_cast<bool>(coil_data[0]);
                }
                delete[] coil_data;
            }
            break;
        case 2: // 读离散输入
            {
                uint8_t* input_data = new uint8_t[count > 0 ? count : 1];
                result = modbus_read_input_bits(m_modbus_ctx.get(), address, count, input_data);
                if (result == count) {
                    value.value = static_cast<bool>(input_data[0]);
                }
                delete[] input_data;
            }
            break;
        default:
            delete[] data;
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
                    delete[] data;
                    return StatusCode::InvalidParam;
                }
            } else if (data_type_it->second == "uint32") {
                if (count >= 2) {
                    uint32_t val = (static_cast<uint32_t>(data[0]) << 16) | data[1];
                    value.value = val;
                } else {
                    delete[] data;
                    return StatusCode::InvalidParam;
                }
            } else if (data_type_it->second == "float32") {
                if (count >= 2) {
                    uint32_t val = (static_cast<uint32_t>(data[0]) << 16) | data[1];
                    float fval;
                    std::memcpy(&fval, &val, sizeof(float));
                    value.value = fval;
                } else {
                    delete[] data;
                    return StatusCode::InvalidParam;
                }
            } else {
                value.value = static_cast<int32_t>(data[0]);
            }
        } else {
            value.value = static_cast<int32_t>(data[0]);
        }
    }
    
    delete[] data;
    
    if (result != count) {
        return StatusCode::Error;
    }
    
    // 设置时间戳和质量
    value.timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    value.quality = 1; // Good
    
    return StatusCode::OK;
}

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

void ModbusAdapter::subscription_worker() {
    while (m_subscription_active) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000)); // 1秒轮询
        
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

int ModbusAdapter::get_register_address(const DeviceTag& tag) {
    auto it = tag.attributes.find("register_address");
    if (it != tag.attributes.end()) {
        return std::stoi(it->second);
    }
    return 0;
}

int ModbusAdapter::get_register_count(const DeviceTag& tag) {
    auto it = tag.attributes.find("register_count");
    if (it != tag.attributes.end()) {
        return std::stoi(it->second);
    }
    return 1;
}

int ModbusAdapter::get_function_code(const DeviceTag& tag) {
    auto it = tag.attributes.find("function_code");
    if (it != tag.attributes.end()) {
        return std::stoi(it->second);
    }
    return 3; // 默认读保持寄存器
}

} // namespace southbound
