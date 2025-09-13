#include "ModbusAdapter.hpp"
#include <southbound/Factory.hpp>
#include <memory>

extern "C" {

southbound::IAdapter *create_adapter() {
    return new southbound::ModbusAdapter();
}

void destroy_adapter(southbound::IAdapter *adapter) {
    delete adapter;
}

} // extern "C"
