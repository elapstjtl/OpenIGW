#include "../Inc/PluginManager.hpp"
#include <iostream>
#include <filesystem>
#include <algorithm>

namespace southbound {

/**
 * @brief 构造函数
 * @details 初始化插件管理器，创建空的插件映射表
 */
PluginManager::PluginManager() {
}

/**
 * @brief 析构函数
 * @details 清理所有已加载的插件，释放动态库句柄
 */
PluginManager::~PluginManager() {
	unload_all_plugins();
}

/**
 * @brief 从指定目录加载所有插件
 * @param plugin_dir 插件目录路径
 * @return 成功加载的插件数量
 * @details 扫描插件目录，加载所有.so文件，验证插件有效性
 */
int PluginManager::load_plugins(const std::string& plugin_dir) {
	int loaded_count = 0;
	try {
		for (const auto& entry : std::filesystem::directory_iterator(plugin_dir)) {
			if (!entry.is_regular_file()) continue; // 如果不是常规文件，则跳过
			const std::string file_path = entry.path().string();
			// 接受 .so 以及版本化 .so.*
			const std::string filename = entry.path().filename().string();
			const bool is_so = filename.find(".so") != std::string::npos; // 包含 .so 即认为是候选
			if (!is_so) continue; // 如果不是 .so 文件，则跳过
			if (load_plugin(file_path)) loaded_count++; 
		}
	} catch (const std::filesystem::filesystem_error& e) {
		std::cerr << "Error scanning plugin directory: " << e.what() << std::endl;
	}
	return loaded_count;
}

/**
 * @brief 加载单个插件
 * @param plugin_path 插件文件路径
 * @return true 加载成功，false 加载失败
 * @details 使用dlopen加载动态库，验证必需符号，注册插件信息
 */
bool PluginManager::load_plugin(const std::string& plugin_path) {
	// 检查插件文件是否存在
	if (!std::filesystem::exists(plugin_path)) {
		std::cerr << "Plugin file does not exist: " << plugin_path << std::endl;
		return false;
	}

	// 提取插件名称
	std::string plugin_name = extract_plugin_name(plugin_path);
	if (plugin_name.empty()) {
		std::cerr << "Cannot extract plugin name from: " << plugin_path << std::endl;
		return false;
	}

	// 检查插件是否已加载
	if (is_plugin_loaded(plugin_name)) {
		std::cout << "Plugin already loaded: " << plugin_name << std::endl;
		return true;
	}

	// 加载插件
	void* handle = dlopen(plugin_path.c_str(), RTLD_LAZY);
	if (!handle) {
		std::cerr << "Failed to load plugin " << plugin_path << ": " << dlerror() << std::endl;
		return false;
	}

	// 验证插件符号
	if (!validate_plugin(handle)) {
		dlclose(handle);
		std::cerr << "Invalid plugin: " << plugin_path << std::endl;
		return false;
	}

	// 验证工厂函数是否存在
	auto create_func = (create_adapter_func_t)dlsym(handle, "create_adapter");
	auto destroy_func = (destroy_adapter_func_t)dlsym(handle, "destroy_adapter");
	if (!create_func || !destroy_func) {
		std::cerr << "Failed to get factory functions from plugin: " << plugin_path << std::endl;
		dlclose(handle);
		return false;
	}

	PluginInfo info{};
	info.handle = handle;
	info.path = plugin_path;
	info.create_func = create_func;
	info.destroy_func = destroy_func;
	m_plugins[plugin_name] = info; // 放入插件映射表

	std::cout << "Successfully loaded plugin: " << plugin_name << std::endl;
	return true;
}

/**
 * @brief 卸载所有插件
 * @details 关闭所有已加载的插件动态库，清空插件映射表
 */
void PluginManager::unload_all_plugins() {
	for (auto& kv : m_plugins) {
		if (kv.second.handle) dlclose(kv.second.handle);
	}
	m_plugins.clear();
}

/**
 * @brief 卸载指定插件
 * @param plugin_name 插件名称
 * @details 关闭指定插件的动态库，从映射表中移除
 */
void PluginManager::unload_plugin(const std::string& plugin_name) {
	auto it = m_plugins.find(plugin_name);
	if (it == m_plugins.end()) {
		std::cerr << "Plugin not found: " << plugin_name << std::endl;
		return;
	}
	if (it->second.handle) dlclose(it->second.handle);
	m_plugins.erase(it);
	std::cout << "Unloaded plugin: " << plugin_name << std::endl;
}

/**
 * @brief 创建适配器实例
 * @param plugin_name 插件名称
 * @return 适配器实例指针，失败返回nullptr
 * @details 使用插件的工厂函数创建新的适配器实例
 */
IAdapter* PluginManager::create_adapter_instance(const std::string& plugin_name) {
	auto it = m_plugins.find(plugin_name);
	if (it == m_plugins.end()) return nullptr;
	return it->second.create_func ? it->second.create_func() : nullptr;
}

/**
 * @brief 销毁适配器实例
 * @param plugin_name 插件名称
 * @param instance 要销毁的适配器实例
 * @details 使用插件的销毁函数释放适配器实例
 */
void PluginManager::destroy_adapter_instance(const std::string& plugin_name, IAdapter* instance) {
	if (!instance) return;
	auto it = m_plugins.find(plugin_name);
	if (it == m_plugins.end()) return;
	if (it->second.destroy_func) it->second.destroy_func(instance);
}

/**
 * @brief 获取已加载的插件列表
 * @return 插件名称列表
 * @details 返回当前已加载的所有插件名称
 */
std::vector<std::string> PluginManager::get_loaded_plugins() const {
	std::vector<std::string> names;
	names.reserve(m_plugins.size());
	for (const auto& kv : m_plugins) names.push_back(kv.first);
	return names;
}

/**
 * @brief 检查插件是否已加载
 * @param plugin_name 插件名称
 * @return true 已加载，false 未加载
 * @details 检查指定名称的插件是否在已加载列表中
 */
bool PluginManager::is_plugin_loaded(const std::string& plugin_name) const {
	return m_plugins.find(plugin_name) != m_plugins.end();
}

/**
 * @brief 从文件路径提取插件名称
 * @param plugin_path 插件文件路径
 * @return 插件名称
 * @details 从完整路径中提取插件名称，去除lib前缀、.so后缀和版本号
 */
std::string PluginManager::extract_plugin_name(const std::string& plugin_path) const {
	std::filesystem::path path(plugin_path);
	std::string filename = path.filename().string();
	// 1) 去掉版本号：截断到 ".so" 之前
	size_t so_pos = filename.find(".so");
	if (so_pos != std::string::npos) {
		filename = filename.substr(0, so_pos + 3); // 保留到 .so 末尾
	}
	// 2) 去掉后缀 ".so"
	if (filename.size() >= 3 && filename.compare(filename.size() - 3, 3, ".so") == 0) {
		filename = filename.substr(0, filename.size() - 3);
	}
	// 3) 去掉前缀 "lib"
	const std::string lib_prefix = "lib";
	if (filename.rfind(lib_prefix, 0) == 0) {
		filename = filename.substr(lib_prefix.size());
	}
	return filename;
}

/**
 * @brief 验证插件有效性
 * @param handle 动态库句柄
 * @return true 插件有效，false 插件无效
 * @details 检查插件是否包含必需的工厂函数符号
 */
bool PluginManager::validate_plugin(void* handle) const {
	void* create_func = dlsym(handle, "create_adapter");
	void* destroy_func = dlsym(handle, "destroy_adapter");
	return create_func != nullptr && destroy_func != nullptr;
}

} // namespace southbound
