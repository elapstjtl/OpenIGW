#pragma once

#include <southbound/IAdapter.hpp>
#include <southbound/Types.hpp>
#include <modbus/modbus.h>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <vector>
#include <map>

namespace southbound {

class ModbusAdapter : public IAdapter {
public:
    ModbusAdapter();
    virtual ~ModbusAdapter();

    // IAdapter 接口实现
    virtual StatusCode init(const AdapterConfig& config) override;
    virtual StatusCode connect() override;
    virtual StatusCode disconnect() override;
    virtual StatusCode read(const std::vector<DeviceTag>& tags, std::vector<DataValue>& values) override;
    virtual StatusCode write(const std::map<DeviceTag, DataValue>& tags_and_values) override;
    virtual StatusCode subscribe(const std::vector<DeviceTag>& tags, OnDataReceivedCallback callback) override;
    virtual StatusCode get_status() override;

private:
    // Modbus 相关
    std::unique_ptr<modbus_t, void(*)(modbus_t*)> m_modbus_ctx;
    std::string m_connection_type;  // "rtu" or "tcp"
    std::string m_device_path;      // 串口设备路径 (RTU)
    std::string m_ip_address;       // IP 地址 (TCP)
    int m_port;                     // 端口号 (TCP)
    int m_slave_id;                 // 从站 ID
    int m_baudrate;                 // 波特率 (RTU)
    char m_parity;                  // 校验位 (RTU)
    int m_data_bits;                // 数据位 (RTU)
    int m_stop_bits;                // 停止位 (RTU)
    
    // 状态管理
    std::atomic<bool> m_connected{false};
    std::atomic<bool> m_initialized{false};
    std::mutex m_mutex;
    
    // 订阅相关
    std::vector<DeviceTag> m_subscribed_tags;
    OnDataReceivedCallback m_callback;
    std::thread m_subscription_thread;
    std::atomic<bool> m_subscription_active{false};
    
    // 内部方法
    StatusCode parse_config(const AdapterConfig& config);
    StatusCode create_modbus_context();
    StatusCode read_register(const DeviceTag& tag, DataValue& value);
    StatusCode write_register(const DeviceTag& tag, const DataValue& value);
    void subscription_worker();
    int get_register_address(const DeviceTag& tag);
    int get_register_count(const DeviceTag& tag);
    int get_function_code(const DeviceTag& tag);
};

} // namespace southbound
