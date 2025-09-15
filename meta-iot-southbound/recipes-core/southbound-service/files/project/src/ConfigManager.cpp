#include "ConfigManager.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>

namespace southbound {

ConfigManager::ConfigManager() {
    set_default_config();
}

ConfigManager::~ConfigManager() {
}

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

const ServiceConfig& ConfigManager::get_service_config() const {
    return m_config;
}

const DeviceConfig* ConfigManager::get_device_config(const std::string& device_name) const {
    for (const auto& device : m_config.devices) {
        if (device.name == device_name) {
            return &device;
        }
    }
    return nullptr;
}

const std::vector<DeviceConfig>& ConfigManager::get_all_devices() const {
    return m_config.devices;
}

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

bool ConfigManager::reload_config() {
    if (m_config_file.empty()) {
        std::cerr << "No config file specified for reload" << std::endl;
        return false;
    }
    
    return load_config(m_config_file);
}

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
    bool in_device_section = false;
    size_t device_start_line = 0;
    
    for (size_t i = 0; i < lines.size(); ++i) {
        const std::string& current_line = lines[i];
        std::string trimmed_line = trim(current_line);
        
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

bool ConfigManager::parse_key_value(const std::string& line, std::string& key, std::string& value) {
    size_t equal_pos = line.find('=');
    if (equal_pos == std::string::npos) {
        return false;
    }
    
    key = trim(line.substr(0, equal_pos));
    value = trim(line.substr(equal_pos + 1));
    
    return !key.empty();
}

std::string ConfigManager::trim(const std::string& str) const {
    size_t first = str.find_first_not_of(' \t');
    if (first == std::string::npos) {
        return "";
    }
    
    size_t last = str.find_last_not_of(' \t');
    return str.substr(first, (last - first + 1));
}

void ConfigManager::set_default_config() {
    m_config.plugin_dir = "/usr/lib/southbound/plugins";
    m_config.log_level = 1;  // INFO level
    m_config.daemon_mode = false;
}

} // namespace southbound
