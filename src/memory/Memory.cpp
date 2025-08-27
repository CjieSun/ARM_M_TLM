#include "Memory.h"
#include "Log.h"
#include <fstream>
#include <sstream>
#include <cstring>

// Simple address map for flat backing store
static constexpr uint32_t FLASH_BASE   = 0x00000000;
static constexpr uint32_t FLASH_SIZE   = 0x00080000; // 512KB (per linker)
static constexpr uint32_t SRAM_BASE    = 0x20000000;
static constexpr uint32_t SRAM_SIZE    = 0x00010000; // 64KB (per linker)
// Internal offsets within the flat memory buffer
static constexpr uint32_t FLASH_OFFSET = 0x00000000;
static constexpr uint32_t SRAM_OFFSET  = FLASH_OFFSET + FLASH_SIZE; // place SRAM after flash

Memory::Memory(sc_module_name name, uint32_t size) : 
    sc_module(name), socket("socket"), m_size(size)
{
    // Allocate memory
    m_memory = new uint8_t[m_size];
    std::memset(m_memory, 0, m_size);
    
    // Bind socket
    socket.register_b_transport(this, &Memory::b_transport);
    socket.register_get_direct_mem_ptr(this, &Memory::get_direct_mem_ptr);
    socket.register_transport_dbg(this, &Memory::transport_dbg);
    
    LOG_INFO("Memory initialized: " + std::to_string(size) + " bytes");
    LOG_INFO("Memory map: FLASH[0x" + std::to_string(FLASH_BASE) + 
             ":0x" + std::to_string(FLASH_BASE + FLASH_SIZE - 1) + 
             "] -> off 0x" + std::to_string(FLASH_OFFSET) + 
             ", SRAM[0x" + std::to_string(SRAM_BASE) + 
             ":0x" + std::to_string(SRAM_BASE + SRAM_SIZE - 1) + 
             "] -> off 0x" + std::to_string(SRAM_OFFSET) + 
             ", backing size=0x" + std::to_string(m_size));
}

Memory::~Memory()
{
    delete[] m_memory;
}

bool Memory::load_hex_file(const std::string& filename)
{
    std::ifstream file(filename);
    if (!file.is_open()) {
        LOG_ERROR("Cannot open HEX file: " + filename);
        return false;
    }
    
    std::string line;
    uint32_t extended_address = 0;
    
    while (std::getline(file, line)) {
        if (line.empty() || line[0] != ':') {
            continue;
        }
        
        HexRecord record;
        if (!parse_hex_line(line, record)) {
            LOG_ERROR("Invalid HEX line: " + line);
            continue;
        }
        
        switch (record.record_type) {
            case 0x00: // Data record
            {
                uint32_t abs_addr = extended_address + record.address;
                // Translate absolute address into backing store offset
                uint32_t dst_off = 0xFFFFFFFF;
                if (abs_addr >= FLASH_BASE && abs_addr < FLASH_BASE + FLASH_SIZE) {
                    dst_off = FLASH_OFFSET + (abs_addr - FLASH_BASE);
                } else if (abs_addr >= SRAM_BASE && abs_addr < SRAM_BASE + SRAM_SIZE) {
                    dst_off = SRAM_OFFSET + (abs_addr - SRAM_BASE);
                }

                if (dst_off != 0xFFFFFFFF && dst_off + record.byte_count <= m_size) {
                    std::memcpy(&m_memory[dst_off], record.data.data(), record.byte_count);
                    LOG_DEBUG("HEX load: " + std::to_string(record.byte_count) + " bytes @abs 0x" +
                              std::to_string(abs_addr) + " -> off 0x" + std::to_string(dst_off));
                } else {
                    LOG_WARNING("HEX data outside mapped memory: abs=0x" + std::to_string(abs_addr));
                }
                break;
            }
            case 0x01: // End of file
                LOG_INFO("HEX file loaded successfully");
                return true;
            case 0x04: // Extended linear address
                if (record.byte_count == 2) {
                    extended_address = (record.data[0] << 24) | (record.data[1] << 16);
                }
                break;
        }
    }
    
    LOG_INFO("HEX file loaded successfully");
    return true;
}

void Memory::b_transport(tlm_generic_payload& trans, sc_time& delay)
{
    if (trans.get_command() == TLM_READ_COMMAND) {
        handle_read(trans);
    } else if (trans.get_command() == TLM_WRITE_COMMAND) {
        handle_write(trans);
    } else {
        trans.set_response_status(TLM_COMMAND_ERROR_RESPONSE);
        return;
    }
    
    // Simulate memory access delay
    delay += sc_time(10, SC_NS);
}

tlm_sync_enum Memory::nb_transport_fw(tlm_generic_payload& trans, tlm_phase& phase, sc_time& delay)
{
    // Simple implementation - convert to blocking transport
    b_transport(trans, delay);
    return TLM_COMPLETED;
}

void Memory::handle_read(tlm_generic_payload& trans)
{
    uint32_t abs_addr = trans.get_address();
    uint32_t length = trans.get_data_length();
    uint8_t* data_ptr = trans.get_data_ptr();

    // Translate absolute to backing-store offset
    uint32_t off = 0xFFFFFFFF;
    if (abs_addr >= FLASH_BASE && abs_addr < FLASH_BASE + FLASH_SIZE) {
        off = FLASH_OFFSET + (abs_addr - FLASH_BASE);
    } else if (abs_addr >= SRAM_BASE && abs_addr < SRAM_BASE + SRAM_SIZE) {
        off = SRAM_OFFSET + (abs_addr - SRAM_BASE);
    }

    if (off == 0xFFFFFFFF || (off + length) > m_size) {
        trans.set_response_status(TLM_ADDRESS_ERROR_RESPONSE);
        return;
    }

    std::memcpy(data_ptr, &m_memory[off], length);
    trans.set_response_status(TLM_OK_RESPONSE);
}

void Memory::handle_write(tlm_generic_payload& trans)
{
    uint32_t abs_addr = trans.get_address();
    uint32_t length = trans.get_data_length();
    uint8_t* data_ptr = trans.get_data_ptr();

    // Translate absolute to backing-store offset
    uint32_t off = 0xFFFFFFFF;
    if (abs_addr >= FLASH_BASE && abs_addr < FLASH_BASE + FLASH_SIZE) {
        off = FLASH_OFFSET + (abs_addr - FLASH_BASE);
    } else if (abs_addr >= SRAM_BASE && abs_addr < SRAM_BASE + SRAM_SIZE) {
        off = SRAM_OFFSET + (abs_addr - SRAM_BASE);
    }

    if (off == 0xFFFFFFFF || (off + length) > m_size) {
        trans.set_response_status(TLM_ADDRESS_ERROR_RESPONSE);
        return;
    }

    std::memcpy(&m_memory[off], data_ptr, length);
    trans.set_response_status(TLM_OK_RESPONSE);
}

bool Memory::is_valid_address(uint32_t address, uint32_t length)
{
    // Legacy helper (not used by TLM path anymore), keep simple bounds on backing store
    return (address + length) <= m_size;
}

uint32_t Memory::read_word(uint32_t address)
{
    // Treat address as absolute; translate
    uint32_t off = 0xFFFFFFFF;
    if (address >= FLASH_BASE && address < FLASH_BASE + FLASH_SIZE) {
        off = FLASH_OFFSET + (address - FLASH_BASE);
    } else if (address >= SRAM_BASE && address < SRAM_BASE + SRAM_SIZE) {
        off = SRAM_OFFSET + (address - SRAM_BASE);
    }
    if (off == 0xFFFFFFFF || off + 4 > m_size) {
        return 0;
    }

    return *reinterpret_cast<uint32_t*>(&m_memory[off]);
}

void Memory::write_word(uint32_t address, uint32_t data)
{
    uint32_t off = 0xFFFFFFFF;
    if (address >= FLASH_BASE && address < FLASH_BASE + FLASH_SIZE) {
        off = FLASH_OFFSET + (address - FLASH_BASE);
    } else if (address >= SRAM_BASE && address < SRAM_BASE + SRAM_SIZE) {
        off = SRAM_OFFSET + (address - SRAM_BASE);
    }
    if (off == 0xFFFFFFFF || off + 4 > m_size) {
        return;
    }

    *reinterpret_cast<uint32_t*>(&m_memory[off]) = data;
}

bool Memory::get_direct_mem_ptr(tlm_generic_payload& trans, tlm_dmi& dmi_data)
{
    // Disable DMI for simplicity with absolute address translation
    return false;
}

unsigned int Memory::transport_dbg(tlm_generic_payload& trans)
{
    uint32_t abs_addr = trans.get_address();
    uint32_t length = trans.get_data_length();
    uint8_t* data_ptr = trans.get_data_ptr();

    uint32_t off = 0xFFFFFFFF;
    if (abs_addr >= FLASH_BASE && abs_addr < FLASH_BASE + FLASH_SIZE) {
        off = FLASH_OFFSET + (abs_addr - FLASH_BASE);
    } else if (abs_addr >= SRAM_BASE && abs_addr < SRAM_BASE + SRAM_SIZE) {
        off = SRAM_OFFSET + (abs_addr - SRAM_BASE);
    }
    if (off == 0xFFFFFFFF || off + length > m_size) {
        return 0;
    }

    if (trans.get_command() == TLM_READ_COMMAND) {
        std::memcpy(data_ptr, &m_memory[off], length);
    } else if (trans.get_command() == TLM_WRITE_COMMAND) {
        std::memcpy(&m_memory[off], data_ptr, length);
    }

    return length;
}

bool Memory::parse_hex_line(const std::string& line, HexRecord& record)
{
    if (line.length() < 11 || line[0] != ':') {
        return false;
    }
    
    try {
        record.byte_count = std::stoi(line.substr(1, 2), nullptr, 16);
        record.address = std::stoi(line.substr(3, 4), nullptr, 16);
        record.record_type = std::stoi(line.substr(7, 2), nullptr, 16);
        
        record.data.clear();
        for (int i = 0; i < record.byte_count; i++) {
            int pos = 9 + i * 2;
            if (pos + 1 < line.length()) {
                uint8_t byte = std::stoi(line.substr(pos, 2), nullptr, 16);
                record.data.push_back(byte);
            }
        }
        
        int checksum_pos = 9 + record.byte_count * 2;
        if (checksum_pos + 1 < line.length()) {
            record.checksum = std::stoi(line.substr(checksum_pos, 2), nullptr, 16);
        }
        
        return true;
    } catch (const std::exception& e) {
        return false;
    }
}

uint8_t Memory::hex_char_to_byte(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return 0;
}