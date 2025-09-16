#pragma once

#include "southbound/Types.hpp"

namespace southbound {

class IAdapter {
public:
	virtual ~IAdapter() = default;

	/**
	 * 初始化适配器实例
	 */
	virtual StatusCode init(const AdapterConfig &config) = 0;

	/**
	 * 连接到物理设备
	 */
	virtual StatusCode connect() = 0;

	/**
	 * 断开与物理设备的连接
	 */
	virtual StatusCode disconnect() = 0;

	/**
	 * [同步] 读取设备数据
	 */
	virtual StatusCode read(const std::vector<DeviceTag> &tags, std::vector<DataValue> &values) = 0;

	/**
	 * [同步] 向设备写入数据
	 */
	virtual StatusCode write(const std::map<DeviceTag, DataValue> &tags_and_values) = 0;

	/**
	 * [异步] 订阅数据变化
	 */
	virtual StatusCode subscribe(const std::vector<DeviceTag> &tags, OnDataReceivedCallback callback) = 0;

	/**
	 * [异步] 取消订阅数据变化
	 */
	virtual StatusCode unsubscribe() = 0;

	/**
	 * 获取当前适配器的健康状态
	 */
	virtual StatusCode get_status() = 0;
};

} // namespace southbound 