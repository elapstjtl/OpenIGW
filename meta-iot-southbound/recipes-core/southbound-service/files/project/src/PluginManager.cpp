#include "PluginManager.hpp"
#include <iostream>
#include <filesystem>
#include <algorithm>

namespace southbound {

PluginManager::PluginManager() {
}

PluginManager::~PluginManager() {
	unload_all_plugins();
}

int PluginManager::load_plugins(const std::string& plugin_dir) {
	int loaded_count = 0;
	try {
		for (const auto& entry : std::filesystem::directory_iterator(plugin_dir)) {
			if (!entry.is_regular_file()) continue;
			const std::string file_path = entry.path().string();
			// 接受 .so 以及版本化 .so.*
			const std::string filename = entry.path().filename().string();
			const bool is_so = filename.find(".so") != std::string::npos; // 包含 .so 即认为是候选
			if (!is_so) continue;
			if (load_plugin(file_path)) loaded_count++;
		}
	} catch (const std::filesystem::filesystem_error& e) {
		std::cerr << "Error scanning plugin directory: " << e.what() << std::endl;
	}
	return loaded_count;
}

bool PluginManager::load_plugin(const std::string& plugin_path) {
	if (!std::filesystem::exists(plugin_path)) {
		std::cerr << "Plugin file does not exist: " << plugin_path << std::endl;
		return false;
	}

	std::string plugin_name = extract_plugin_name(plugin_path);
	if (plugin_name.empty()) {
		std::cerr << "Cannot extract plugin name from: " << plugin_path << std::endl;
		return false;
	}

	if (is_plugin_loaded(plugin_name)) {
		std::cout << "Plugin already loaded: " << plugin_name << std::endl;
		return true;
	}

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
	m_plugins[plugin_name] = info;

	std::cout << "Successfully loaded plugin: " << plugin_name << std::endl;
	return true;
}

void PluginManager::unload_all_plugins() {
	for (auto& kv : m_plugins) {
		if (kv.second.handle) dlclose(kv.second.handle);
	}
	m_plugins.clear();
}

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

IAdapter* PluginManager::get_adapter(const std::string& /*plugin_name*/) {
	// 兼容旧接口：不再维护单例
	return nullptr;
}

IAdapter* PluginManager::create_adapter_instance(const std::string& plugin_name) {
	auto it = m_plugins.find(plugin_name);
	if (it == m_plugins.end()) return nullptr;
	return it->second.create_func ? it->second.create_func() : nullptr;
}

void PluginManager::destroy_adapter_instance(const std::string& plugin_name, IAdapter* instance) {
	if (!instance) return;
	auto it = m_plugins.find(plugin_name);
	if (it == m_plugins.end()) return;
	if (it->second.destroy_func) it->second.destroy_func(instance);
}

std::vector<std::string> PluginManager::get_loaded_plugins() const {
	std::vector<std::string> names;
	names.reserve(m_plugins.size());
	for (const auto& kv : m_plugins) names.push_back(kv.first);
	return names;
}

bool PluginManager::is_plugin_loaded(const std::string& plugin_name) const {
	return m_plugins.find(plugin_name) != m_plugins.end();
}

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

bool PluginManager::validate_plugin(void* handle) const {
	void* create_func = dlsym(handle, "create_adapter");
	void* destroy_func = dlsym(handle, "destroy_adapter");
	return create_func != nullptr && destroy_func != nullptr;
}

} // namespace southbound

