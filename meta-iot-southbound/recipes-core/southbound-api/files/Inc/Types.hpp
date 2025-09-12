#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <variant>
#include <vector>

namespace southbound {

/**
 * @brief 状态码
 * 
 */
enum class StatusCode {
	OK,
	Error,
	Timeout,
	BadConfig,
	NotConnected,
	AlreadyConnected,
	NotInitialized,
	InvalidParam,
	NotSupported
};

/**
 * @brief 描述设备点位（如设备地址、寄存器、通道、数据类型等） 可以作为std.map、std.set的key
 * 
 * @param attributes 描述设备的各种属性，如设备地址、寄存器、通道、数据类型等
 * @param operator< 用于比较两个 DeviceTag 对象，根据 attributes 的键值对进行比较
 */
struct DeviceTag {
	std::map<std::string, std::string> attributes;

	// Strict-weak ordering for use as a std::map key 严格弱序 重载 operator<
	bool operator<(const DeviceTag &other) const {
		if (attributes.size() != other.attributes.size()) {
			return attributes.size() < other.attributes.size();
		}
		auto it1 = attributes.begin();
		auto it2 = other.attributes.begin();
		for (; it1 != attributes.end() && it2 != other.attributes.end(); ++it1, ++it2) {
			if (it1->first < it2->first) return true;
			if (it1->first > it2->first) return false;
			if (it1->second < it2->second) return true;
			if (it1->second > it2->second) return false;
		}
		return false;
	}
};

/**
 * @brief 数据类型，约定的统一数据格式
 * 
 */
struct DataValue {
	std::variant<bool, int32_t, uint32_t, float, double, std::string> value;
	uint64_t timestamp_ms { 0 }; // Unix epoch in milliseconds
	uint8_t quality { 0 };      // 0=Bad, 1=Good, etc.
};

// 统一配置键值格式, 约定的 key 需由具体适配器文档说明
using AdapterConfig = std::map<std::string, std::string>;

// 异步订阅回调，一次可回传多个点位的最新值
using OnDataReceivedCallback = std::function<void(const std::map<DeviceTag, DataValue> &)>;

} // namespace southbound 