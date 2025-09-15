#pragma once

#include <southbound/IAdapter.hpp>
#include <southbound/Types.hpp>
#include "PluginManager.hpp"
#include "ConfigManager.hpp"
#include <string>
#include <map>
#include <memory>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <mutex>

namespace southbound {

/**
 * @brief 南向服务主类，负责管理插件和设备通信
 */
class SouthboundService {
public:
    SouthboundService();
    ~SouthboundService();

    /**
     * @brief 初始化服务
     * @param config_file 配置文件路径
     * @return 是否初始化成功
     */
    bool initialize(const std::string& config_file);

    /**
     * @brief 启动服务
     * @return 是否启动成功
     */
    bool start();

    /**
     * @brief 停止服务
     */
    void stop();

    /**
     * @brief 服务是否正在运行
     * @return 是否运行中
     */
    bool is_running() const;

    /**
     * @brief 读取设备数据
     * @param device_name 设备名称
     * @param tags 标签列表
     * @param values 输出数据值
     * @return 操作状态码
     */
    StatusCode read_device_data(const std::string& device_name, 
                               const std::vector<DeviceTag>& tags, 
                               std::vector<DataValue>& values);

    /**
     * @brief 写入设备数据
     * @param device_name 设备名称
     * @param tags_and_values 标签和值的映射
     * @return 操作状态码
     */
    StatusCode write_device_data(const std::string& device_name, 
                                const std::map<DeviceTag, DataValue>& tags_and_values);

    /**
     * @brief 订阅设备数据变化
     * @param device_name 设备名称
     * @param tags 标签列表
     * @param callback 数据变化回调
     * @return 操作状态码
     */
    StatusCode subscribe_device_data(const std::string& device_name, 
                                   const std::vector<DeviceTag>& tags, 
                                   OnDataReceivedCallback callback);

    /**
     * @brief 获取服务状态
     * @return 服务状态信息
     */
    std::string get_service_status() const;

private:
    std::unique_ptr<PluginManager> m_plugin_manager;
    std::unique_ptr<ConfigManager> m_config_manager;
    
    std::map<std::string, IAdapter*> m_device_adapters;  // 设备名称到适配器的映射
    std::map<std::string, std::string> m_device_plugin_map;  // 设备名称到插件名称的映射
    
    std::atomic<bool> m_running;
    std::atomic<bool> m_initialized;
    
    mutable std::mutex m_mutex;
    std::condition_variable m_cv;
    
    std::thread m_worker_thread;

    /**
     * @brief 工作线程函数
     */
    void worker_thread_func();

    /**
     * @brief 初始化设备适配器
     * @return 是否初始化成功
     */
    bool initialize_device_adapters();

    /**
     * @brief 连接所有设备
     * @return 是否连接成功
     */
    bool connect_all_devices();

    /**
     * @brief 断开所有设备连接
     */
    void disconnect_all_devices();

    /**
     * @brief 获取设备对应的适配器
     * @param device_name 设备名称
     * @return 适配器指针，如果不存在返回nullptr
     */
    IAdapter* get_device_adapter(const std::string& device_name) const;

    /**
     * @brief 日志输出
     * @param level 日志级别
     * @param message 日志消息
     */
    void log(int level, const std::string& message) const;
};

} // namespace southbound
