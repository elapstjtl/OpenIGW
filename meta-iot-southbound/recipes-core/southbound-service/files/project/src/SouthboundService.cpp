#include "../Inc/SouthboundService.hpp"
#include <iostream>
#include <chrono>
#include <signal.h>

namespace southbound {

/**
 * @brief 构造函数
 * @details 初始化南向服务，创建插件管理器和配置管理器实例
 */
SouthboundService::SouthboundService() 
    : m_running(false), m_initialized(false) {
    m_plugin_manager = std::make_unique<PluginManager>();
    m_config_manager = std::make_unique<ConfigManager>();
}

/**
 * @brief 析构函数
 * @details 停止服务并清理资源
 */
SouthboundService::~SouthboundService() {
    stop();
}

/**
 * @brief 初始化服务
 * @param config_file 配置文件路径
 * @return true 初始化成功，false 初始化失败
 * @details 加载配置、验证配置、加载插件、初始化设备适配器
 */
bool SouthboundService::initialize(const std::string& config_file) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_initialized) {
        log(1, "Service already initialized");
        return true;
    }
    
    // 加载配置
    if (!m_config_manager->load_config(config_file)) {
        log(0, "Failed to load config file: " + config_file);
        return false;
    }
    
    // 验证配置
    if (!m_config_manager->validate_config()) {
        log(0, "Invalid configuration");
        return false;
    }
    
    // 加载插件
    const ServiceConfig& service_config = m_config_manager->get_service_config();
    int loaded_count = m_plugin_manager->load_plugins(service_config.plugin_dir);
    log(1, "Loaded " + std::to_string(loaded_count) + " plugins");
    
    // 初始化设备适配器
    if (!initialize_device_adapters()) {
        log(0, "Failed to initialize device adapters");
        return false;
    }
    
    m_initialized = true;
    log(1, "Service initialized successfully");
    return true;
}

/**
 * @brief 启动服务
 * @return true 启动成功，false 启动失败
 * @details 连接所有设备，启动工作线程
 */
bool SouthboundService::start() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (!m_initialized) {
        log(0, "Service not initialized");
        return false;
    }
    
    if (m_running) {
        log(1, "Service already running");
        return true;
    }
    
    // 连接所有设备
    if (!connect_all_devices()) {
        log(0, "Failed to connect to all devices");
        return false;
    }
    
    // 启动工作线程
    m_running = true;
    m_worker_thread = std::thread(&SouthboundService::worker_thread_func, this);
    
    log(1, "Service started successfully");
    return true;
}

/**
 * @brief 停止服务
 * @details 停止工作线程，断开所有设备连接，清理资源
 */
void SouthboundService::stop() {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        if (!m_running) {
            return;
        }
        
        m_running = false;
    }
    
    m_cv.notify_all();
    
    if (m_worker_thread.joinable()) {
        m_worker_thread.join();
    }
    
    // 断开所有设备连接，并销毁实例
    disconnect_all_devices();
    
    log(1, "Service stopped");
}

/**
 * @brief 检查服务是否运行
 * @return true 正在运行，false 已停止
 * @details 返回服务的运行状态
 */
bool SouthboundService::is_running() const {
    return m_running;
}

/**
 * @brief 读取设备数据
 * @param device_name 设备名称
 * @param tags 要读取的标签列表
 * @param values 输出的数据值列表
 * @return 操作状态码
 * @details 从指定设备读取指定标签的数据值
 */
StatusCode SouthboundService::read_device_data(const std::string& device_name, 
                                             const std::vector<DeviceTag>& tags, 
                                             std::vector<DataValue>& values) {
    IAdapter* adapter = get_device_adapter(device_name);
    if (!adapter) {
        log(0, "Device not found: " + device_name);
        return StatusCode::NotConnected;
    }
    
    return adapter->read(tags, values);
}

/**
 * @brief 写入设备数据
 * @param device_name 设备名称
 * @param tags_and_values 标签和值的映射
 * @return 操作状态码
 * @details 向指定设备写入标签和对应的数据值
 */
StatusCode SouthboundService::write_device_data(const std::string& device_name, 
                                              const std::map<DeviceTag, DataValue>& tags_and_values) {
    IAdapter* adapter = get_device_adapter(device_name);
    if (!adapter) {
        log(0, "Device not found: " + device_name);
        return StatusCode::NotConnected;
    }
    
    return adapter->write(tags_and_values);
}

/**
 * @brief 订阅设备数据
 * @param device_name 设备名称
 * @param tags 要订阅的标签列表
 * @param callback 数据接收回调函数
 * @return 操作状态码
 * @details 订阅指定设备的数据变化，通过回调函数接收数据
 */
StatusCode SouthboundService::subscribe_device_data(const std::string& device_name, 
                                                  const std::vector<DeviceTag>& tags, 
                                                  OnDataReceivedCallback callback) {
    IAdapter* adapter = get_device_adapter(device_name);
    if (!adapter) {
        log(0, "Device not found: " + device_name);
        return StatusCode::NotConnected;
    }
    
    return adapter->subscribe(tags, callback);
}

/**
 * @brief 获取服务状态
 * @return 服务状态字符串
 * @details 返回服务的详细状态信息，包括运行状态、插件数量、设备数量等
 */
std::string SouthboundService::get_service_status() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::string status = "Service Status:\n";
    status += "  Running: " + std::string(m_running ? "Yes" : "No") + "\n";
    status += "  Initialized: " + std::string(m_initialized ? "Yes" : "No") + "\n";
    status += "  Loaded Plugins: " + std::to_string(m_plugin_manager->get_loaded_plugins().size()) + "\n";
    status += "  Connected Devices: " + std::to_string(m_device_adapters.size()) + "\n";
    
    return status;
}

/**
 * @brief 工作线程函数
 * @details 后台工作线程，定期检查设备状态，处理后台任务
 */
void SouthboundService::worker_thread_func() {
    log(1, "Worker thread started");
    
    while (m_running) {
        // 这里可以添加定期任务，如健康检查、数据采集等
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        // 检查设备连接状态
        for (const auto& pair : m_device_adapters) {
            const std::string& device_name = pair.first;
            IAdapter* adapter = pair.second;
            
            StatusCode status = adapter->get_status();
            if (status != StatusCode::OK) {
                log(0, "Device " + device_name + " status error: " + std::to_string(static_cast<int>(status)));
            }
        }
    }
    
    log(1, "Worker thread stopped");
}

/**
 * @brief 初始化设备适配器
 * @return true 初始化成功，false 初始化失败
 * @details 为每个配置的设备创建对应的适配器实例并初始化
 */
bool SouthboundService::initialize_device_adapters() {
    const std::vector<DeviceConfig>& devices = m_config_manager->get_all_devices();
    
    for (const auto& device_config : devices) {
        // 为每个设备创建独立的适配器实例
        IAdapter* adapter = m_plugin_manager->create_adapter_instance(device_config.adapter_type);
        if (!adapter) {
            log(0, "Plugin not found for device " + device_config.name + ": " + device_config.adapter_type);
            return false;
        }
        
        // 初始化适配器
        StatusCode status = adapter->init(device_config.adapter_config);
        if (status != StatusCode::OK) {
            log(0, "Failed to initialize adapter for device " + device_config.name);
            m_plugin_manager->destroy_adapter_instance(device_config.adapter_type, adapter);
            return false;
        }
        
        // 保存适配器引用
        m_device_adapters[device_config.name] = adapter;
        m_device_plugin_map[device_config.name] = device_config.adapter_type;
        
        log(1, "Initialized adapter for device: " + device_config.name);
    }
    
    return true;
}

/**
 * @brief 连接所有设备
 * @return true 连接成功，false 连接失败
 * @details 尝试连接所有已初始化的设备适配器
 */
bool SouthboundService::connect_all_devices() {
    for (const auto& pair : m_device_adapters) {
        const std::string& device_name = pair.first;
        IAdapter* adapter = pair.second;
        
        StatusCode status = adapter->connect();
        if (status != StatusCode::OK) {
            log(0, "Failed to connect device " + device_name);
            return false;
        }
        
        log(1, "Connected device: " + device_name);
    }
    
    return true;
}

/**
 * @brief 断开所有设备连接
 * @details 断开所有设备连接并销毁适配器实例
 */
void SouthboundService::disconnect_all_devices() {
    for (auto it = m_device_adapters.begin(); it != m_device_adapters.end(); ++it) {
        const std::string& device_name = it->first;
        IAdapter* adapter = it->second;
        
        StatusCode status = adapter->disconnect();
        if (status != StatusCode::OK) {
            log(0, "Failed to disconnect device " + device_name);
        } else {
            log(1, "Disconnected device: " + device_name);
        }
        // 销毁实例
        auto pm_it = m_device_plugin_map.find(device_name);
        if (pm_it != m_device_plugin_map.end()) {
            m_plugin_manager->destroy_adapter_instance(pm_it->second, adapter);
        }
    }
    m_device_adapters.clear();
    m_device_plugin_map.clear();
}

/**
 * @brief 获取设备适配器
 * @param device_name 设备名称
 * @return 设备适配器指针，未找到返回nullptr
 * @details 根据设备名称查找对应的适配器实例
 */
IAdapter* SouthboundService::get_device_adapter(const std::string& device_name) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_device_adapters.find(device_name);
    if (it == m_device_adapters.end()) {
        return nullptr;
    }
    
    return it->second;
}

/**
 * @brief 输出日志信息
 * @param level 日志级别（0=ERROR, 1=INFO, 2=DEBUG）
 * @param message 日志消息
 * @details 根据配置的日志级别输出日志信息
 */
void SouthboundService::log(int level, const std::string& message) const {
    const ServiceConfig& config = m_config_manager->get_service_config();
    
    if (level <= config.log_level) {
        const char* level_str = (level == 0) ? "ERROR" : (level == 1) ? "INFO" : "DEBUG";
        std::cout << "[" << level_str << "] " << message << std::endl;
    }
}

} // namespace southbound
