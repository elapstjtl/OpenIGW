#pragma once

#include <southbound/Types.hpp>
#include <string>
#include <map>
#include <vector>

namespace southbound {

/**
 * @brief 设备配置信息
 */
struct DeviceConfig {
    std::string name;                    // 设备名称
    std::string adapter_type;            // 适配器类型（如modbus-adapter）
    AdapterConfig adapter_config;        // 适配器特定配置
    std::vector<DeviceTag> tags;         // 设备标签列表
};

/**
 * @brief 服务配置信息
 */
struct ServiceConfig {
    std::string plugin_dir;              // 插件目录
    std::vector<DeviceConfig> devices;   // 设备配置列表
    int log_level;                       // 日志级别
    bool daemon_mode;                    // 是否守护进程模式
};

/**
 * @brief 配置管理器，负责解析和管理配置文件
 */
class ConfigManager {
public:
    ConfigManager();
    ~ConfigManager();

    /**
     * @brief 从文件加载配置
     * @param config_file 配置文件路径
     * @return 是否加载成功
     */
    bool load_config(const std::string& config_file);

    /**
     * @brief 获取服务配置
     * @return 服务配置引用
     */
    const ServiceConfig& get_service_config() const;

    /**
     * @brief 获取指定设备的配置
     * @param device_name 设备名称
     * @return 设备配置指针，如果不存在返回nullptr
     */
    const DeviceConfig* get_device_config(const std::string& device_name) const;

    /**
     * @brief 获取所有设备配置
     * @return 设备配置列表
     */
    const std::vector<DeviceConfig>& get_all_devices() const;

    /**
     * @brief 验证配置是否有效
     * @return 是否有效
     */
    bool validate_config() const;

    /**
     * @brief 重新加载配置
     * @return 是否重新加载成功
     */
    bool reload_config();

private:
    ServiceConfig m_config;
    std::string m_config_file;

    /**
     * @brief 解析配置文件内容
     * @param content 配置文件内容
     * @return 是否解析成功
     */
    bool parse_config_content(const std::string& content);

    /**
     * @brief 解析设备配置段
     * @param lines 配置行列表
     * @param start_line 开始行号
     * @param end_line 结束行号
     * @param device 设备配置对象
     * @return 是否解析成功
     */
    bool parse_device_section(const std::vector<std::string>& lines, 
                             size_t start_line, size_t end_line, 
                             DeviceConfig& device);

    /**
     * @brief 解析键值对
     * @param line 配置行
     * @param key 输出键
     * @param value 输出值
     * @return 是否解析成功
     */
    bool parse_key_value(const std::string& line, std::string& key, std::string& value);

    /**
     * @brief 去除字符串前后空白
     * @param str 输入字符串
     * @return 处理后的字符串
     */
    std::string trim(const std::string& str) const;

    /**
     * @brief 设置默认配置
     */
    void set_default_config();
};

} // namespace southbound
