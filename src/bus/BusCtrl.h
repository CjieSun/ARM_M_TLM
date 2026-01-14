#ifndef BUSCTRL_H
#define BUSCTRL_H

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>
#include <tlm_utils/simple_initiator_socket.h>
#include <map>
#include <vector>
#include <memory>
#include <string>

using namespace sc_core;
using namespace tlm;

class BusCtrl : public sc_module, public tlm_fw_transport_if<>
{
public:
    // Target sockets (from CPU)
    tlm_utils::simple_target_socket<BusCtrl> inst_socket; // Instruction bus
    tlm_utils::simple_target_socket<BusCtrl> data_socket; // Data bus
    
    // Device mapping structure
    struct DeviceMapping {
        std::string name;
        uint32_t base_address;
        uint32_t size;
        tlm_utils::simple_initiator_socket<BusCtrl>* socket;
        bool address_translation;  // true if address should be adjusted to 0-based
        
        DeviceMapping(const std::string& n, uint32_t base, uint32_t sz, bool addr_trans = true) 
            : name(n), base_address(base), size(sz), socket(nullptr), address_translation(addr_trans) {}
    };

    // Constructor
    SC_HAS_PROCESS(BusCtrl);
    BusCtrl(sc_module_name name);
    
    // Device management methods
    void add_device(const std::string& name, uint32_t base_address, uint32_t size, bool address_translation = true);
    tlm_utils::simple_initiator_socket<BusCtrl>* get_device_socket(const std::string& name);
    void print_memory_map() const;
    
    // Pre-defined device helpers
    void add_memory(uint32_t base = 0x00000000, uint32_t size = 0x40000000);
    void add_trace_peripheral(uint32_t base = 0x40000000, uint32_t size = 0x00004000);
    void add_nvic(uint32_t base = 0xE000E000, uint32_t size = 0x00001000);
    void add_uart(const std::string& name, uint32_t base, uint32_t size = 0x1000);
    void add_gpio(const std::string& name, uint32_t base, uint32_t size = 0x1000);
    void add_timer(const std::string& name, uint32_t base, uint32_t size = 0x1000);
    void add_spi(const std::string& name, uint32_t base, uint32_t size = 0x1000);
    void add_i2c(const std::string& name, uint32_t base, uint32_t size = 0x1000);

    // TLM-2 interface methods
    virtual void b_transport(tlm_generic_payload& trans, sc_time& delay);
    virtual tlm_sync_enum nb_transport_fw(tlm_generic_payload& trans, tlm_phase& phase, sc_time& delay);
    virtual bool get_direct_mem_ptr(tlm_generic_payload& trans, tlm_dmi& dmi_data);
    virtual unsigned int transport_dbg(tlm_generic_payload& trans);

private:
    // Device storage
    std::vector<std::unique_ptr<DeviceMapping>> m_devices;
    std::map<std::string, size_t> m_device_index;  // name -> index in m_devices
    std::vector<std::unique_ptr<tlm_utils::simple_initiator_socket<BusCtrl>>> m_sockets;
    
    // Address decoding
    DeviceMapping* decode_address(uint32_t address);
    void route_transaction(tlm_generic_payload& trans, sc_time& delay, DeviceMapping* device);
};

#endif // BUSCTRL_H