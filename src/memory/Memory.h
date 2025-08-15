#ifndef MEMORY_H
#define MEMORY_H

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>
#include <vector>
#include <string>

using namespace sc_core;
using namespace tlm;

class Memory : public sc_module, public tlm_fw_transport_if<>
{
public:
    // TLM target socket
    tlm_utils::simple_target_socket<Memory> socket;

    // Constructor
    SC_HAS_PROCESS(Memory);
    Memory(sc_module_name name, uint32_t size = 0x100000); // Default 1MB
    
    // Destructor
    ~Memory();

    // Load Intel HEX file
    bool load_hex_file(const std::string& filename);
    
    // Direct memory access (for debugging)
    uint32_t read_word(uint32_t address);
    void write_word(uint32_t address, uint32_t data);
    
    // TLM-2 interface methods
    virtual void b_transport(tlm_generic_payload& trans, sc_time& delay);
    virtual tlm_sync_enum nb_transport_fw(tlm_generic_payload& trans, tlm_phase& phase, sc_time& delay);
    virtual bool get_direct_mem_ptr(tlm_generic_payload& trans, tlm_dmi& dmi_data);
    virtual unsigned int transport_dbg(tlm_generic_payload& trans);
    
private:
    uint8_t* m_memory;
    uint32_t m_size;
    
    // Helper methods
    bool is_valid_address(uint32_t address, uint32_t length);
    void handle_read(tlm_generic_payload& trans);
    void handle_write(tlm_generic_payload& trans);
    
    // Intel HEX parsing
    struct HexRecord {
        uint8_t byte_count;
        uint16_t address;
        uint8_t record_type;
        std::vector<uint8_t> data;
        uint8_t checksum;
    };
    
    bool parse_hex_line(const std::string& line, HexRecord& record);
    uint8_t hex_char_to_byte(char c);
};

#endif // MEMORY_H