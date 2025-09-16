#include "../Inc/ConfigManager.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>

namespace southbound {

/**
 * @brief 构造函数，初始化配置管理器
 * @details 创建配置管理器实例并设置默认配置值
 */
ConfigManager::ConfigManager() {
    set_default_config();
}

/**
 * @brief 析构函数
 * @details 清理配置管理器资源
 */
ConfigManager::~ConfigManager() {
}

/**
 * @brief 从指定文件加载配置
 * @param config_file 配置文件路径
 * @return true 加载成功，false 加载失败
 * @details 打开配置文件，读取内容并解析配置项
 */
bool ConfigManager::load_config(const std::string& config_file) {
    m_config_file = config_file;
    
    std::ifstream file(config_file);
    if (!file.is_open()) {
        std::cerr << "Failed to open config file: " << config_file << std::endl;
        return false;
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    file.close();
    
    return parse_config_content(buffer.str());
}

/**
 * @brief 获取服务配置
 * @return 服务配置的常量引用
 * @details 返回当前加载的服务配置信息 返回类成员变量 m_config 的 只读引用
 */
const ServiceConfig& ConfigManager::get_service_config() const {
    return m_config;
}

/**
 * @brief 根据设备名称获取设备配置
 * @param device_name 设备名称
 * @return 设备配置指针，如果未找到返回nullptr
 * @details 在已加载的设备配置中查找指定名称的设备
 */
const DeviceConfig* ConfigManager::get_device_config(const std::string& device_name) const {
    for (const auto& device : m_config.devices) {
        if (device.name == device_name) {
            return &device;
        }
    }
    return nullptr;
}

/**
 * @brief 获取所有设备配置
 * @return 所有设备配置的常量引用
 * @details 返回当前加载的所有设备配置列表
 */
const std::vector<DeviceConfig>& ConfigManager::get_all_devices() const {
    return m_config.devices;
}

/**
 * @brief 验证配置的有效性
 * @return true 配置有效，false 配置无效
 * @details 检查插件目录、设备名称、适配器类型等必需配置项
 */
bool ConfigManager::validate_config() const {
    // 检查插件目录
    if (m_config.plugin_dir.empty()) {
        std::cerr << "Plugin directory not specified" << std::endl;
        return false;
    }
    
    // 检查设备配置
    for (const auto& device : m_config.devices) {
        if (device.name.empty()) {
            std::cerr << "Device name cannot be empty" << std::endl;
            return false;
        }
        
        if (device.adapter_type.empty()) {
            std::cerr << "Adapter type not specified for device: " << device.name << std::endl;
            return false;
        }
    }
    
    return true;
}

/**
 * @brief 重新加载配置文件
 * @return true 重载成功，false 重载失败
 * @details 使用当前保存的配置文件路径重新加载配置
 */
bool ConfigManager::reload_config() {
    if (m_config_file.empty()) {
        std::cerr << "No config file specified for reload" << std::endl;
        return false;
    }
    
    return load_config(m_config_file);
}

/**
 * @brief 解析配置文件内容
 * @param content 配置文件内容字符串
 * @return true 解析成功，false 解析失败
 * @details 解析INI格式的配置文件，支持全局配置和设备配置段
 */
bool ConfigManager::parse_config_content(const std::string& content) {
    std::vector<std::string> lines;
    std::istringstream stream(content);
    std::string line;
    
    while (std::getline(stream, line)) {
        lines.push_back(line);
    }
    
    // 重置配置
    m_config.devices.clear();
    
    DeviceConfig current_device;
    bool in_device_section = false; // 是否在设备段
    size_t device_start_line = 0; // 设备段开始行号
    
    for (size_t i = 0; i < lines.size(); ++i) {
        const std::string& current_line = lines[i];
        std::string trimmed_line = trim(current_line); // 去除行首尾空格
        
        // 跳过空行和注释
        if (trimmed_line.empty() || trimmed_line[0] == '#') {
            continue;
        }
        
        // 检查是否是设备段开始
        if (trimmed_line[0] == '[' && trimmed_line.back() == ']') {
            // 如果之前有设备段，先解析它
            if (in_device_section) {
                if (!parse_device_section(lines, device_start_line, i - 1, current_device)) {
                    return false;
                }
                m_config.devices.push_back(current_device);
            }
            
            // 开始新的设备段
            current_device = DeviceConfig();
            current_device.name = trimmed_line.substr(1, trimmed_line.length() - 2);
            in_device_section = true;
            device_start_line = i + 1;
            continue;
        }
        
        // 解析全局配置
        if (!in_device_section) {
            std::string key, value;
            if (parse_key_value(current_line, key, value)) {
                if (key == "plugin_dir") {
                    m_config.plugin_dir = value;
                } else if (key == "log_level") {
                    m_config.log_level = std::stoi(value);
                } else if (key == "daemon_mode") {
                    m_config.daemon_mode = (value == "true" || value == "1");
                }
            }
        }
    }
    
    // 处理最后一个设备段
    if (in_device_section) {
        if (!parse_device_section(lines, device_start_line, lines.size() - 1, current_device)) {
            return false;
        }
        m_config.devices.push_back(current_device);
    }
    
    return true;
}

/**
 * @brief 解析设备配置段
 * @param lines 配置文件行数组
 * @param start_line 开始行号
 * @param end_line 结束行号
 * @param device 输出的设备配置对象
 * @return true 解析成功，false 解析失败
 * @details 解析指定行范围内的设备配置，包括适配器类型、连接参数和标签定义
 */
bool ConfigManager::parse_device_section(const std::vector<std::string>& lines, 
                                       size_t start_line, size_t end_line, 
                                       DeviceConfig& device) {
    for (size_t i = start_line; i <= end_line && i < lines.size(); ++i) {
        const std::string& line = lines[i];
        std::string trimmed_line = trim(line);
        
        // 跳过空行和注释
        if (trimmed_line.empty() || trimmed_line[0] == '#') {
            continue;
        }
        
        std::string key, value;
        if (parse_key_value(line, key, value)) {
            if (key == "adapter_type") {
                device.adapter_type = value;
            } else if (key == "tag") {
                // 解析标签，格式: tag=address:1,type:holding,slave:1
                DeviceTag tag;
                std::istringstream tag_stream(value);
                std::string pair;
                
                while (std::getline(tag_stream, pair, ',')) {
                    size_t colon_pos = pair.find(':');
                    if (colon_pos != std::string::npos) {
                        std::string tag_key = trim(pair.substr(0, colon_pos));
                        std::string tag_value = trim(pair.substr(colon_pos + 1));
                        tag.attributes[tag_key] = tag_value;
                    }
                }
                
                if (!tag.attributes.empty()) {
                    device.tags.push_back(tag);
                }
            } else {
                // 其他配置项作为适配器配置
                device.adapter_config[key] = value;
            }
        }
    }
    
    return true;
}

/**
 * @brief 解析键值对
 * @param line 配置行字符串
 * @param key 输出的键
 * @param value 输出的值
 * @return true 解析成功，false 解析失败
 * @details 从配置行中解析出键值对，格式为 key=value
 */
bool ConfigManager::parse_key_value(const std::string& line, std::string& key, std::string& value) {
    size_t equal_pos = line.find('=');
    if (equal_pos == std::string::npos) {
        return false;
    }
    
    key = trim(line.substr(0, equal_pos));
    value = trim(line.substr(equal_pos + 1));
    
    return !key.empty();
}

/**
 * @brief 去除字符串首尾空白字符
 * @param str 输入字符串
 * @return 去除空白字符后的字符串
 * @details 去除字符串开头和结尾的空格、制表符等空白字符
 */
std::string ConfigManager::trim(const std::string& str) const {
    size_t first = str.find_first_not_of(' \t');
    if (first == std::string::npos) {
        return "";
    }
    
    size_t last = str.find_last_not_of(' \t');
    return str.substr(first, (last - first + 1));
}

/**
 * @brief 设置默认配置值
 * @details 初始化配置管理器的默认配置，包括插件目录、日志级别等
 */
void ConfigManager::set_default_config() {
    m_config.plugin_dir = "/usr/lib/southbound/plugins";
    m_config.log_level = 1;  // INFO level
    m_config.daemon_mode = false;
}

} // namespace southbound
