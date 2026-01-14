#include "BusCtrl.h"
#include "Log.h"
#include <sstream>
#include <iomanip>
#include <algorithm>

BusCtrl::BusCtrl(sc_module_name name) : 
    sc_module(name),
    inst_socket("inst_socket"),
    data_socket("data_socket")
{
    // Bind target sockets
    inst_socket.register_b_transport(this, &BusCtrl::b_transport);
    inst_socket.register_get_direct_mem_ptr(this, &BusCtrl::get_direct_mem_ptr);
    inst_socket.register_transport_dbg(this, &BusCtrl::transport_dbg);
    
    data_socket.register_b_transport(this, &BusCtrl::b_transport);
    data_socket.register_get_direct_mem_ptr(this, &BusCtrl::get_direct_mem_ptr);
    data_socket.register_transport_dbg(this, &BusCtrl::transport_dbg);
    
    LOG_INFO("Flexible Bus Controller initialized");
}

void BusCtrl::add_device(const std::string& name, uint32_t base_address, uint32_t size, bool address_translation)
{
    // Check for address conflicts
    for (const auto& device : m_devices) {
        uint32_t dev_end = device->base_address + device->size - 1;
        uint32_t new_end = base_address + size - 1;
        
        if (!((base_address > dev_end) || (new_end < device->base_address))) {
            std::stringstream ss;
            ss << "Address conflict: Device '" << name << "' at 0x" << std::hex << base_address 
               << " conflicts with existing device '" << device->name << "' at 0x" << device->base_address;
            LOG_ERROR(ss.str());
            return;
        }
    }
    
    // Create new device mapping
    auto device = std::make_unique<DeviceMapping>(name, base_address, size, address_translation);
    
    // Create socket for this device
    std::string socket_name = name + "_socket";
    auto socket = std::make_unique<tlm_utils::simple_initiator_socket<BusCtrl>>(socket_name.c_str());
    
    device->socket = socket.get();
    
    // Store device and socket
    size_t index = m_devices.size();
    m_device_index[name] = index;
    m_devices.push_back(std::move(device));
    m_sockets.push_back(std::move(socket));
    
    std::stringstream ss;
    ss << "Added device '" << name << "' at address range 0x" << std::hex << std::setfill('0') 
       << std::setw(8) << base_address << " - 0x" << std::setw(8) << (base_address + size - 1)
       << " (size: 0x" << std::setw(4) << size << ")";
    if (!address_translation) {
        ss << " [no address translation]";
    }
    LOG_INFO(ss.str());
}

tlm_utils::simple_initiator_socket<BusCtrl>* BusCtrl::get_device_socket(const std::string& name)
{
    auto it = m_device_index.find(name);
    if (it != m_device_index.end()) {
        return m_devices[it->second]->socket;
    }
    LOG_WARNING("Device '" + name + "' not found");
    return nullptr;
}

void BusCtrl::print_memory_map() const
{
    LOG_INFO("=== Memory Map ===");
    
    // Sort devices by base address for display
    std::vector<const DeviceMapping*> sorted_devices;
    for (const auto& device : m_devices) {
        sorted_devices.push_back(device.get());
    }
    
    std::sort(sorted_devices.begin(), sorted_devices.end(), 
              [](const DeviceMapping* a, const DeviceMapping* b) {
                  return a->base_address < b->base_address;
              });
    
    for (const auto* device : sorted_devices) {
        std::stringstream ss;
        ss << "0x" << std::hex << std::setfill('0') << std::setw(8) << device->base_address 
           << " - 0x" << std::setw(8) << (device->base_address + device->size - 1)
           << " : " << device->name << " (size: 0x" << std::setw(4) << device->size << ")";
        if (!device->address_translation) {
            ss << " [no translation]";
        }
        LOG_INFO(ss.str());
    }
    LOG_INFO("==================");
}

// Pre-defined device helpers
void BusCtrl::add_memory(uint32_t base, uint32_t size) 
{
    add_device("memory", base, size, false);  // Memory typically doesn't need address translation
}

void BusCtrl::add_trace_peripheral(uint32_t base, uint32_t size)
{
    add_device("trace", base, size, true);
}

void BusCtrl::add_nvic(uint32_t base, uint32_t size)
{
    add_device("nvic", base, size, false);  // NVIC uses absolute ARM addresses
}

void BusCtrl::add_uart(const std::string& name, uint32_t base, uint32_t size)
{
    add_device(name, base, size, true);
}

void BusCtrl::add_gpio(const std::string& name, uint32_t base, uint32_t size)
{
    add_device(name, base, size, true);
}

void BusCtrl::add_timer(const std::string& name, uint32_t base, uint32_t size)
{
    add_device(name, base, size, true);
}

void BusCtrl::add_spi(const std::string& name, uint32_t base, uint32_t size)
{
    add_device(name, base, size, true);
}

void BusCtrl::add_i2c(const std::string& name, uint32_t base, uint32_t size)
{
    add_device(name, base, size, true);
}

void BusCtrl::b_transport(tlm_generic_payload& trans, sc_time& delay)
{
    uint32_t address = trans.get_address();
    DeviceMapping* device = decode_address(address);
    
    route_transaction(trans, delay, device);
}

tlm_sync_enum BusCtrl::nb_transport_fw(tlm_generic_payload& trans, tlm_phase& phase, sc_time& delay)
{
    // Simple implementation - convert to blocking transport
    b_transport(trans, delay);
    return TLM_COMPLETED;
}

BusCtrl::DeviceMapping* BusCtrl::decode_address(uint32_t address)
{
    for (auto& device : m_devices) {
        if (address >= device->base_address && 
            address < device->base_address + device->size) {
            return device.get();
        }
    }
    
    return nullptr;  // Invalid address
}

void BusCtrl::route_transaction(tlm_generic_payload& trans, sc_time& delay, DeviceMapping* device)
{
    if (device == nullptr) {
        std::stringstream ss;
        ss << "Invalid address access: 0x" << std::hex << trans.get_address();
        LOG_WARNING(ss.str());
        trans.set_response_status(TLM_ADDRESS_ERROR_RESPONSE);
        return;
    }
    
    // Store original address for restoration
    uint32_t original_address = trans.get_address();
    
    // Apply address translation if needed
    if (device->address_translation) {
        trans.set_address(original_address - device->base_address);
    }
    
    // Forward transaction to appropriate device
    (*device->socket)->b_transport(trans, delay);
    
    // Restore original address
    trans.set_address(original_address);
}

bool BusCtrl::get_direct_mem_ptr(tlm_generic_payload& trans, tlm_dmi& dmi_data)
{
    uint32_t address = trans.get_address();
    DeviceMapping* device = decode_address(address);
    
    if (device != nullptr && device->name == "memory") {
        // Only allow DMI for memory
        return (*device->socket)->get_direct_mem_ptr(trans, dmi_data);
    }
    
    return false;  // No DMI for peripherals
}

unsigned int BusCtrl::transport_dbg(tlm_generic_payload& trans)
{
    uint32_t address = trans.get_address();
    DeviceMapping* device = decode_address(address);
    
    if (device == nullptr) {
        return 0;
    }
    
    // Store original address for restoration
    uint32_t original_address = trans.get_address();
    
    // Apply address translation if needed
    if (device->address_translation) {
        trans.set_address(original_address - device->base_address);
    }
    
    // Forward debug transaction
    unsigned int result = (*device->socket)->transport_dbg(trans);
    
    // Restore original address
    trans.set_address(original_address);
    
    return result;
}