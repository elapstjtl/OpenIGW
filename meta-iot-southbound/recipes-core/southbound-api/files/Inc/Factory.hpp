#pragma once

namespace southbound { class IAdapter; } // 前向声明

extern "C" {
	/** 工厂函数，创建适配器实例 */
	southbound::IAdapter *create_adapter();
	/** 工厂函数，销毁适配器实例 */
	void destroy_adapter(southbound::IAdapter *adapter);
} 