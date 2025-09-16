#pragma once

#include <southbound/IAdapter.hpp>
#include <southbound/Factory.hpp>
#include <southbound/Types.hpp>
#include <string>
#include <map>
#include <memory>
#include <vector>
#include <dlfcn.h>

namespace southbound {

/**
 * @brief 插件管理器，负责动态加载和管理适配器插件
 */
class PluginManager {
public:
    PluginManager();
    ~PluginManager();

    /**
     * @brief 从指定目录加载所有插件
     * @param plugin_dir 插件目录路径
     * @return 加载成功的插件数量
     */
    int load_plugins(const std::string& plugin_dir);

    /**
     * @brief 加载单个插件
     * @param plugin_path 插件文件路径
     * @return 是否加载成功
     */
    bool load_plugin(const std::string& plugin_path);

    /**
     * @brief 卸载所有插件
     */
    void unload_all_plugins();

    /**
     * @brief 卸载指定插件
     * @param plugin_name 插件名称（规范化名：不含lib前缀，去除扩展与版本）
     */
    void unload_plugin(const std::string& plugin_name);

    /**
     * @brief 创建一个新的适配器实例（调用插件工厂）
     * @param plugin_name 插件名称（例如 "modbus-adapter"）
     * @return 适配器实例指针；失败返回nullptr
     */
    IAdapter* create_adapter_instance(const std::string& plugin_name);

    /**
     * @brief 销毁通过 create_adapter_instance 创建的实例
     */
    void destroy_adapter_instance(const std::string& plugin_name, IAdapter* instance);

    /**
     * @brief 获取所有已加载的插件名称
     * @return 插件名称列表
     */
    std::vector<std::string> get_loaded_plugins() const;

    /**
     * @brief 检查插件是否已加载
     * @param plugin_name 插件名称
     * @return 是否已加载
     */
    bool is_plugin_loaded(const std::string& plugin_name) const;

private:
    using create_adapter_func_t = IAdapter*(*)();
    using destroy_adapter_func_t = void(*)(IAdapter*);

    struct PluginInfo {
        void* handle;                           // dlopen句柄
        std::string path;                       // 插件文件路径
        create_adapter_func_t create_func;      // 创建函数
        destroy_adapter_func_t destroy_func;    // 销毁函数
    };

    std::map<std::string, PluginInfo> m_plugins;  // 插件映射表（键为规范化插件名）

    /**
     * @brief 从插件文件路径提取规范化插件名称
     * 规则：去掉前缀"lib"，截断到".so"之前，去除后续版本号
     * 例如：/usr/lib/southbound/plugins/libmodbus-adapter.so.1.0.0 -> modbus-adapter
     */
    std::string extract_plugin_name(const std::string& plugin_path) const;

    /**
     * @brief 验证插件是否包含必需的符号
     * @param handle 插件句柄
     * @return 是否验证通过
     */
    bool validate_plugin(void* handle) const;
};

} // namespace southbound

