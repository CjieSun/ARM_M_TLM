# ARM_M_TLM

An ARM Cortex-M0 SystemC-TLM2 Virtual Platform Simulator

This project implements a cycle-accurate, transaction-level modeling (TLM-2) based virtual platform for the ARM Cortex-M0 processor. Built using SystemC, it provides a comprehensive simulation environment supporting the complete ARMv6-M Thumb/Thumb2 instruction set architecture with integrated peripherals and debugging capabilities.

## 📖 Table of Contents

- [🚀 Key Features](#-key-features)
- [📋 Project Architecture](#-project-architecture)
  - [System Overview](#system-overview)
  - [TLM Socket Communication Architecture](#tlm-socket-communication-architecture)  
  - [Address Space and Memory Mapping](#address-space-and-memory-mapping)
  - [Core Design Principles](#core-design-principles)
- [🏗️ Building the Simulator](#-building-the-simulator)
- [💻 Usage](#-usage)
- [🧪 Testing](#-testing)
- [🔧 SystemC Module Details](#-systemc-module-details)
- [🔌 Adding New Peripherals](#-adding-new-peripherals)
- [📊 Module Summary](#-module-summary)
- [🏗️ SystemC Module Hierarchy](#-systemc-module-hierarchy)

## 🚀 Key Features

- **Complete ARMv6-M ISA Support**: Full Thumb/Thumb2 instruction set implementation
- **Harvard Architecture**: Separate instruction and data buses with TLM-2 sockets  
- **Integrated Peripherals**: NVIC with SysTick timer, ITM-like trace output
- **Advanced Debugging**: GDB remote debugging server with breakpoint support
- **Performance Monitoring**: Built-in performance counters and detailed logging
- **Intel HEX Support**: Direct firmware loading from compiled binaries
- **Extensible Design**: Modular architecture for easy peripheral addition

## 📋 Project Architecture

### System Overview

The ARM_M_TLM simulator implements a complete virtual Cortex-M0 system-on-chip using SystemC and Transaction Level Modeling (TLM-2.0). The design follows a modular, loosely-coupled architecture where components communicate through standardized TLM sockets.

```
┌─────────────────────────────────────────────────────────────────┐
│                        Simulator (Top Level)                    │
├─────────────────────────────────────────────────────────────────┤
│  ┌─────────────────┐    ┌──────────────────────────────────────┐ │
│  │      CPU        │    │            BusCtrl                   │ │
│  │ ┌─────────────┐ │    │ ┌──────────┐    ┌─────────────────┐ │ │
│  │ │   Execute   │ │    │ │ Address  │    │    Target       │ │ │
│  │ │             │ │    │ │ Decoder  │    │   Sockets       │ │ │
│  │ ├─────────────┤ │    │ │          │    │                 │ │ │
│  │ │ Instruction │ │    │ └──────────┘    │ inst_socket     │ │ │
│  │ │   Decoder   │ │◄──►│                 │ data_socket     │ │ │
│  │ ├─────────────┤ │    │ ┌──────────┐    │                 │ │ │
│  │ │  Registers  │ │    │ │ Routing  │    │ ┌─────────────┐ │ │ │
│  │ │   (R0-R15)  │ │    │ │ Logic    │    │ │ Initiator   │ │ │ │
│  │ └─────────────┘ │    │ │          │    │ │  Sockets    │ │ │ │
│  │                 │    │ └──────────┘    │ │             │ │ │ │
│  │ inst_bus ───────┼────┤                 │ │memory_socket│ │ │ │
│  │ data_bus ───────┼────┤                 │ │trace_socket │ │ │ │
│  │ irq_line ◄──────┼─┐  │                 │ │nvic_socket  │ │ │ │
│  └─────────────────┘ │  └─────────────────┴─┴─────────────┴─┘ │ │
└───────────────────────┼────────────────────────────────────────┘ │
                        │  ┌─────────────────────────────────────┐ │
                        │  │             Peripherals              │ │
                        │  ├─────────────────────────────────────┤ │
                        │  │ ┌──────────┐ ┌─────────┐ ┌────────┐ │ │
                        │  │ │  Memory  │ │  Trace  │ │  NVIC  │ │ │
                        │  │ │          │ │         │ │        │ │ │
                        │  │ │  1MB     │ │ xterm   │ │SysTick │ │ │
                        │  │ │  SRAM    │ │ output  │ │ + IRQ  │ │ │
                        │  │ │          │ │         │ │ Ctrl   │ │ │
                        │  │ │ HEX Load │ │ ITM-    │ │        │ │ │
                        │  │ │ Support  │ │ like    │ │        │ │ │
                        │  │ └──────────┘ └─────────┘ └────────┘ │ │
                        │  └─────────────────────────────────────┘ │
                        │                                          │
                        │  ┌─────────────────────────────────────┐ │
                        └──┤            Helper Classes            │ │
                           ├─────────────────────────────────────┤ │
                           │ ┌──────────┐ ┌──────────┐ ┌──────┐ │ │
                           │ │   Log    │ │Performan-│ │ GDB  │ │ │
                           │ │ System   │ │ce Monitor│ │Server│ │ │
                           │ │(Singleton│ │(Singleton│ │      │ │ │
                           │ └──────────┘ └──────────┘ └──────┘ │ │
                           └─────────────────────────────────────┘ │
                                                                  │
               Performance: ~3M instructions/sec (Core i5-5200U)  │
               Performance: ~4.5M instructions/sec (Core i7-8550U)│
```

### Core Design Principles

1. **Modular Architecture**: Each functional unit is implemented as a separate SystemC module
2. **TLM-2.0 Compliance**: All inter-module communication uses standardized TLM sockets
3. **Harvard Architecture**: Separate instruction and data memory interfaces
4. **Event-Driven Simulation**: SystemC kernel handles timing and synchronization
5. **Extensible Platform**: New peripherals can be easily added via the bus controller

### TLM Socket Communication Architecture

The simulator uses a hierarchical socket-based communication model where all transactions flow through the central bus controller:

```
     CPU Module                    BusCtrl                    Peripherals
┌─────────────────┐           ┌─────────────────┐         ┌─────────────────┐
│                 │           │                 │         │                 │
│ inst_bus ───────┼──────────►│ inst_socket     │         │                 │
│ (initiator)     │           │ (target)        │         │                 │
│                 │           │                 │         │                 │
│ data_bus ───────┼──────────►│ data_socket     │         │                 │
│ (initiator)     │           │ (target)        │         │                 │
│                 │           │                 │         │                 │
│ irq_line        │           │                 │         │                 │
│ (target)        │◄─┐        │ memory_socket ──┼────────►│ Memory::socket  │
│                 │  │        │ (initiator)     │         │ (target)        │
└─────────────────┘  │        │                 │         │                 │
                     │        │ trace_socket ───┼────────►│ Trace::socket   │
                     │        │ (initiator)     │         │ (target)        │
                     │        │                 │         │                 │
                     │        │ nvic_socket ────┼────────►│ NVIC::socket    │
                     │        │ (initiator)     │         │ (target)        │
                     │        │                 │         │                 │
                     └────────┤ NVIC::cpu_socket│◄────────┤ NVIC::cpu_socket│
                              │ (initiator)     │         │ (initiator)     │
                              └─────────────────┘         └─────────────────┘
```

**Socket Types and Purposes:**

- **CPU → BusCtrl**
  - `inst_bus → inst_socket`: Instruction fetch transactions
  - `data_bus → data_socket`: Data memory load/store transactions
  
- **BusCtrl → Peripherals**  
  - `memory_socket → Memory::socket`: All memory access (code + data)
  - `trace_socket → Trace::socket`: ITM-like debug output transactions
  - `nvic_socket → NVIC::socket`: NVIC register access transactions

- **NVIC → CPU**
  - `NVIC::cpu_socket → CPU::irq_line`: Exception/interrupt delivery

### Address Space and Memory Mapping

The BusCtrl performs address decoding to route transactions to appropriate peripherals:

```
Memory Map:
┌─────────────────┬─────────────────┬──────────────────────────────────────┐
│ Address Range   │ Module          │ Description                          │
├─────────────────┼─────────────────┼──────────────────────────────────────┤
│ 0x00000000      │ Memory          │ Flash/ROM (Instruction + Data)       │
│ - 0x0FFFFFFF    │                 │                                      │
├─────────────────┼─────────────────┼──────────────────────────────────────┤
│ 0x20000000      │ Memory          │ SRAM (Data)                          │
│ - 0x3FFFFFFF    │                 │                                      │
├─────────────────┼─────────────────┼──────────────────────────────────────┤
│ 0x40000000      │ Trace           │ ITM-like Debug Output                │
├─────────────────┼─────────────────┼──────────────────────────────────────┤
│ 0xE000E000      │ NVIC            │ Cortex-M0 System Control Block       │
│ - 0xE000EFFF    │                 │                                      │
│   └─E000E010    │                 │ SysTick CTRL                         │
│   └─E000E014    │                 │ SysTick LOAD (24-bit)                │
│   └─E000E018    │                 │ SysTick VAL (24-bit)                 │
│   └─E000E01C    │                 │ SysTick CALIB (Read-Only)            │
│   └─E000E100    │                 │ ISER (Interrupt Set-Enable)          │
│   └─E000E180    │                 │ ICER (Interrupt Clear-Enable)        │
│   └─E000E200    │                 │ ISPR (Interrupt Set-Pending)         │
│   └─E000E280    │                 │ ICPR (Interrupt Clear-Pending)       │
│   └─E000E400-   │                 │ IPR0-IPR7 (Interrupt Priorities)     │
│     E000E41C    │                 │                                      │
│   └─E000ED1C    │                 │ SHPR2 (SVCall Priority)              │
│   └─E000ED20    │                 │ SHPR3 (SysTick, PendSV Priority)    │
│   └─E000ED24    │                 │ SHCSR (System Handler Control)       │
└─────────────────┴─────────────────┴──────────────────────────────────────┘
```

## 🏗️ Building the Simulator
- CMake (version 3.10 or higher)
- G++ compiler with C++17 support
- SystemC library (version 2.3.x)

### Installation Steps
1. **Install SystemC dependencies:**
   ```bash
   sudo apt-get update
   sudo apt-get install -y libsystemc-dev cmake build-essential
   ```

2. **Clone and build the simulator:**
   ```bash
   git clone https://github.com/CjieSun/ARM_M_TLM.git
   cd ARM_M_TLM
   mkdir build && cd build
   cmake ..
   make -j4
   ```

3. **Run the simulator:**
   ```bash
   # Basic simulation (1ms duration)
   ./bin/arm_m_tlm

   # With debug logging
   ./bin/arm_m_tlm --debug

   # Load Intel HEX program
   ./bin/arm_m_tlm --hex program.hex --trace

   # Enable GDB debugging
   ./bin/arm_m_tlm --gdb
   
   # Show help
   ./bin/arm_m_tlm --help
   ```

## 💻 Usage

### Command Line Options
- `--hex <file>`: Load Intel HEX file into memory
- `--log <file>`: Specify log file (default: simulation.log)
- `--debug`: Enable debug-level logging
- `--trace`: Enable instruction-level tracing
- `--gdb`: Enable GDB server on default port (3333)
- `--gdb-port <port>`: Enable GDB server on specified port
- `--help, -h`: Show usage information

### GDB Debugging

The simulator now supports remote debugging with GDB! Start the simulator with GDB support:

```bash
# Start GDB server
./bin/arm_m_tlm --gdb

# In another terminal, connect with GDB
arm-none-eabi-gdb -ex 'target remote localhost:3333'
```

Supported GDB features:
- Register read/write (R0-R15, PSR)
- Memory read/write
- Single step execution (`stepi`)
- Continue execution (`continue`) 
- Software breakpoints
- Standard GDB commands

See [docs/GDB_DEBUGGING.md](docs/GDB_DEBUGGING.md) for detailed usage instructions.

### Example Usage
```bash
# Run with maximum logging detail
./bin/arm_m_tlm --hex firmware.hex --trace --log detailed.log

# Quick test run with debug info
./bin/arm_m_tlm --debug
```

## 🧪 Testing

### Quick Test Execution
Run all tests and generate comprehensive reports:
```bash
./run_tests.sh
```

### Manual Test Execution
```bash
# Build and run individual tests
cd tests/assembly
make test-report           # Generate HTML and JSON reports
make test-report-html      # HTML report only
make test-report-json      # JSON report only
make run-all              # Run tests without reports
```

### Test Reports
The testing system generates:
- **HTML reports**: Interactive web-based test results with performance metrics
- **JSON reports**: Machine-readable data for CI/CD integration
- **Console output**: Real-time test execution status

For detailed testing documentation, see [TEST_REPORTING.md](docs/TEST_REPORTING.md).

## 🔧 SystemC Module Details

### Core Modules

#### 1. CPU Module (`src/cpu/CPU.h`)
**Purpose**: Complete ARM Cortex-M0 processor implementation  
**Sub-modules**: 
- `Execute`: Instruction execution engine
- `Instruction`: Instruction decoder  
- `Registers`: Register file (R0-R15, PSR)

**TLM Interfaces**:
```cpp
tlm_utils::simple_initiator_socket<CPU> inst_bus;  // Instruction fetch
tlm_utils::simple_initiator_socket<CPU> data_bus;  // Data memory access
tlm_utils::simple_target_socket<CPU> irq_line;     // Interrupt delivery
```

**Key Features**:
- Complete ARMv6-M instruction set support
- Exception handling (Reset, NMI, HardFault, SVCall, PendSV, SysTick)
- Single-cycle execution model
- GDB debug interface support

#### 2. BusCtrl Module (`src/bus/BusCtrl.h`)
**Purpose**: Central interconnect with address decoding and transaction routing  

**TLM Interfaces**:
```cpp
// Target sockets (from CPU)
tlm_utils::simple_target_socket<BusCtrl> inst_socket;
tlm_utils::simple_target_socket<BusCtrl> data_socket;

// Initiator sockets (to peripherals)  
tlm_utils::simple_initiator_socket<BusCtrl> memory_socket;
tlm_utils::simple_initiator_socket<BusCtrl> trace_socket;
tlm_utils::simple_initiator_socket<BusCtrl> nvic_socket;
```

**Address Decoding Logic**:
```cpp
enum AddressSpace {
    ADDR_MEMORY,    // 0x00000000-0x3FFFFFFF 
    ADDR_TRACE,     // 0x40000000
    ADDR_NVIC,      // 0xE000E000-0xE000EFFF
    ADDR_INVALID    // All other addresses
};
```

#### 3. Memory Module (`src/memory/Memory.h`) 
**Purpose**: Unified instruction and data memory with Intel HEX loading

**TLM Interfaces**:
```cpp
tlm_utils::simple_target_socket<Memory> socket;
```

**Key Features**:
- 1MB default memory size (configurable)
- Intel HEX file format support
- Direct Memory Interface (DMI) for performance
- Debug transport for GDB integration

#### 4. NVIC Module (`src/peripherals/NVIC.h`)
**Purpose**: Nested Vectored Interrupt Controller with SysTick timer

**TLM Interfaces**:
```cpp
tlm_utils::simple_target_socket<NVIC> socket;        // Register access
tlm_utils::simple_initiator_socket<NVIC> cpu_socket; // CPU interrupt delivery
```

**Key Features**:
- 32 external IRQ support (IRQ0-IRQ31)
- SysTick timer with 24-bit countdown
- System exception priorities (SVCall, PendSV, SysTick)
- Full ARMv6-M NVIC register map compliance

#### 5. Trace Module (`src/peripherals/Trace.h`)
**Purpose**: ITM-like debug output via xterm terminal

**TLM Interfaces**:
```cpp
tlm_utils::simple_target_socket<Trace> socket;
```

**Key Features**:
- Automatic xterm window creation
- Character-by-character output
- Pseudo-terminal (PTY) based implementation
- Process management for xterm lifecycle

#### 6. GDBServer Module (`src/debug/GDBServer.h`)
**Purpose**: Remote debugging support using GDB Remote Serial Protocol

**Key Features**:
- TCP server on configurable port (default: 3333)
- Standard GDB commands (continue, step, breakpoints)
- Memory and register read/write
- Software breakpoint support

### Helper Classes

#### Performance Monitor (`src/helpers/Performance.h`)
**Purpose**: Simulation performance metrics collection  
**Pattern**: Singleton  
**Metrics**: Instructions/sec, memory accesses, register operations

#### Logger (`src/helpers/Log.h`) 
**Purpose**: Comprehensive logging system  
**Pattern**: Singleton  
**Features**: Multiple log levels, file output, instruction tracing

## 🔌 Adding New Peripherals

The modular TLM-based architecture makes adding new peripherals straightforward. Follow these steps:

### Step 1: Create Peripheral Module

Create your peripheral class inheriting from `sc_module` and implementing `tlm_fw_transport_if<>`:

```cpp
// src/peripherals/YourPeripheral.h
#ifndef YOURPERIPHERAL_H
#define YOURPERIPHERAL_H

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>

using namespace sc_core;
using namespace tlm;

class YourPeripheral : public sc_module, public tlm_fw_transport_if<>
{
public:
    // TLM target socket for CPU access
    tlm_utils::simple_target_socket<YourPeripheral> socket;
    
    // Optional: TLM initiator socket for CPU interrupts
    tlm_utils::simple_initiator_socket<YourPeripheral> irq_socket;

    // Constructor
    SC_HAS_PROCESS(YourPeripheral);
    YourPeripheral(sc_module_name name);

    // TLM-2 interface methods (required)
    virtual void b_transport(tlm_generic_payload& trans, sc_time& delay);
    virtual tlm_sync_enum nb_transport_fw(tlm_generic_payload& trans, tlm_phase& phase, sc_time& delay);
    virtual bool get_direct_mem_ptr(tlm_generic_payload& trans, tlm_dmi& dmi_data);
    virtual unsigned int transport_dbg(tlm_generic_payload& trans);

private:
    // Register handling
    void handle_read(tlm_generic_payload& trans);
    void handle_write(tlm_generic_payload& trans);
    
    // Internal registers
    uint32_t m_control_reg;
    uint32_t m_status_reg;
    // ... other registers
};

#endif
```

### Step 2: Implement TLM Methods

```cpp
// src/peripherals/YourPeripheral.cpp
#include "YourPeripheral.h"
#include "Log.h"

YourPeripheral::YourPeripheral(sc_module_name name) : 
    sc_module(name),
    socket("socket"),
    irq_socket("irq_socket"),  // if needed
    m_control_reg(0),
    m_status_reg(0)
{
    // Register TLM callbacks
    socket.register_b_transport(this, &YourPeripheral::b_transport);
    socket.register_get_direct_mem_ptr(this, &YourPeripheral::get_direct_mem_ptr);
    socket.register_transport_dbg(this, &YourPeripheral::transport_dbg);
    
    LOG_INFO("YourPeripheral initialized");
}

void YourPeripheral::b_transport(tlm_generic_payload& trans, sc_time& delay)
{
    if (trans.is_read()) {
        handle_read(trans);
    } else if (trans.is_write()) {
        handle_write(trans);
    }
    
    trans.set_response_status(TLM_OK_RESPONSE);
}

void YourPeripheral::handle_read(tlm_generic_payload& trans)
{
    uint32_t address = trans.get_address();
    uint32_t* data_ptr = reinterpret_cast<uint32_t*>(trans.get_data_ptr());
    
    switch (address & 0xFF) {  // Use offset addressing
        case 0x00: *data_ptr = m_control_reg; break;
        case 0x04: *data_ptr = m_status_reg; break;
        default:
            LOG_WARNING("Invalid read address: 0x" + std::to_string(address));
            trans.set_response_status(TLM_ADDRESS_ERROR_RESPONSE);
            return;
    }
}

void YourPeripheral::handle_write(tlm_generic_payload& trans)
{
    uint32_t address = trans.get_address();
    uint32_t data = *reinterpret_cast<uint32_t*>(trans.get_data_ptr());
    
    switch (address & 0xFF) {
        case 0x00: 
            m_control_reg = data;
            // Trigger functionality based on control register
            break;
        case 0x04:
            // Status register might be read-only or have special write behavior
            break;
        default:
            LOG_WARNING("Invalid write address: 0x" + std::to_string(address));
            trans.set_response_status(TLM_ADDRESS_ERROR_RESPONSE);
            return;
    }
}
```

### Step 3: Update Bus Controller

Add your peripheral to the bus controller's address decoding:

```cpp
// In src/bus/BusCtrl.h - Add new socket
tlm_utils::simple_initiator_socket<BusCtrl> your_peripheral_socket;

// In src/bus/BusCtrl.h - Add new address space
enum AddressSpace {
    ADDR_MEMORY,
    ADDR_TRACE,
    ADDR_NVIC,
    ADDR_YOUR_PERIPHERAL,  // Add this
    ADDR_INVALID
};

// In src/bus/BusCtrl.cpp - Add address decoding logic
BusCtrl::AddressSpace BusCtrl::decode_address(uint32_t address)
{
    if (address < 0x40000000) return ADDR_MEMORY;
    else if (address == 0x40000000) return ADDR_TRACE;
    else if (address >= 0x40001000 && address < 0x40002000) return ADDR_YOUR_PERIPHERAL;
    else if (address >= 0xE000E000 && address <= 0xE000EFFF) return ADDR_NVIC;
    else return ADDR_INVALID;
}

// In src/bus/BusCtrl.cpp - Add routing logic  
void BusCtrl::route_transaction(tlm_generic_payload& trans, sc_time& delay, AddressSpace space)
{
    switch (space) {
        case ADDR_YOUR_PERIPHERAL:
            trans.set_address(trans.get_address() - 0x40001000);  // Convert to offset
            your_peripheral_socket->b_transport(trans, delay);
            break;
        // ... other cases
    }
}
```

### Step 4: Update Simulator Integration

```cpp
// In src/Simulator.h - Add member
YourPeripheral* m_your_peripheral;

// In src/Simulator.cpp - Create and connect
void Simulator::initialize_components()
{
    // ... existing components
    m_your_peripheral = new YourPeripheral("your_peripheral");
}

void Simulator::connect_components() 
{
    // ... existing connections
    m_bus_ctrl->your_peripheral_socket.bind(m_your_peripheral->socket);
    
    // If your peripheral needs to send interrupts:
    m_your_peripheral->irq_socket.bind(m_cpu->irq_line);
}
```

### Step 5: Update Build System

Add your new files to CMakeLists.txt:

```cmake
set(SOURCES
    # ... existing sources
    src/peripherals/YourPeripheral.cpp
)
```

### Step 6: Documentation and Testing

1. **Update Memory Map**: Add your peripheral's address range to the README
2. **Create Tests**: Write assembly test programs to verify functionality  
3. **Add Logging**: Use the Log system for debugging and monitoring
4. **Performance Impact**: Monitor using the Performance class

### Example: Timer Peripheral

For a complete example, here's a simple timer peripheral:

```cpp
class Timer : public sc_module, public tlm_fw_transport_if<>
{
private:
    enum Registers {
        TIMER_CTRL = 0x00,    // Control: [0] Enable, [1] Interrupt Enable
        TIMER_COUNT = 0x04,   // Current count value  
        TIMER_RELOAD = 0x08,  // Reload value
        TIMER_STATUS = 0x0C   // Status: [0] Interrupt Pending
    };
    
    uint32_t m_ctrl, m_count, m_reload, m_status;
    sc_event m_tick_event;
    
public:
    tlm_utils::simple_target_socket<Timer> socket;
    tlm_utils::simple_initiator_socket<Timer> irq_socket;
    
    void timer_thread(); // SystemC process for counting
    // ... implement TLM methods
};
```

This approach ensures your peripheral integrates seamlessly with the existing TLM infrastructure and follows the established patterns in the simulator.

##Description
## 📊 Module Summary

**Core Processing Modules:**
- **CPU**: Complete ARM Cortex-M0 processor with Execute, Instruction decoder, and Registers (R0-R15, PSR)
- **Memory**: 1MB unified instruction/data memory with Intel HEX loading support  
- **Registers**: Register file implementation for general-purpose registers and control/status registers
- **Instruction**: Instruction decoder supporting complete ARMv6-M instruction set
- **BASE_ISA**: Instruction execution engine supporting:
  1. Branch instructions
  2. Data-processing operations  
  3. Status register access
  4. Load and store operations
  5. Multiple load/store (LDM/STM)
  6. Miscellaneous instructions
  7. Exception generation

**System Integration:**
- **Simulator**: Top-level module orchestrating all components and simulation lifecycle
- **BusCtrl**: Intelligent bus controller with address decoding and transaction routing

**Peripheral Modules:**
- **Trace**: ITM-like debug output creating xterm terminal for character display
- **NVIC**: Nested Vectored Interrupt Controller with SysTick timer and external IRQ support  

**Debug and Development:**
- **Debug**: GDB remote debugging server with breakpoint and single-step support (Beta)

**Helper Classes (Singletons):**
- **Performance**: Comprehensive performance metrics (instructions/sec, memory accesses, register operations)
- **Log**: Multi-level logging system with file output and instruction tracing capabilities

**Performance Benchmarks:**
- Intel Core i5-5200U @ 2.2GHz: ~3,000,000 instructions/second
- Intel Core i7-8550U @ 1.8GHz: ~4,500,000 instructions/second

The Trace peripheral automatically creates an xterm window for real-time character output, providing a visual interface for embedded software debug output.

**System Architecture Characteristics:**
The CPU implements Harvard architecture with separate TLM sockets for instruction and data buses, enabling independent memory access patterns typical of embedded processors. The bus controller provides centralized address decoding, automatically routing memory transactions to the appropriate peripheral based on the ARM Cortex-M memory map.

## 🏗️ SystemC Module Hierarchy

The following diagram shows the actual module hierarchy as implemented in the current system:

<img width="718" height="730" alt="ARM_M_TLM Module Hierarchy" src="https://github.com/user-attachments/assets/e53aa654-e415-4194-88e7-798a1886adb0" />

### Detailed NVIC Configuration Guide

The NVIC implementation provides complete ARMv6-M system control functionality:

**SysTick Timer Setup (Cortex-M0/M0+ compatible):**

1. **Write LOAD register** with 24-bit reload value (N-1) at `0xE000E014`
2. **Optionally clear VAL** by writing any value to `0xE000E018` 
3. **Configure CTRL register** at `0xE000E010`:
   - Bit 0 (ENABLE): Set to 1 to enable SysTick
   - Bit 1 (TICKINT): Set to 1 to generate exception 15 (SysTick)
   - Bit 2 (CLKSOURCE): Set to 1 for internal processor clock
4. **Provide SysTick_Handler** in vector table at entry 15

**External Interrupt Configuration:**
- Enable interrupts via ISER (0xE000E100)
- Clear interrupts via ICER (0xE000E180)  
- Set pending via ISPR (0xE000E200)
- Clear pending via ICPR (0xE000E280)
- Configure priorities via IPR0-IPR7 (0xE000E400-0xE000E41C)

**System Handler Priorities:**
- SVCall priority: SHPR2 (0xE000ED1C)
- SysTick/PendSV priorities: SHPR3 (0xE000ED20)
- System handler control: SHCSR (0xE000ED24)

### Performance and Debug Features

**Comprehensive Logging System:**
At maximum debug level, each instruction execution generates detailed log entries containing:
- Current simulation time
- Program Counter (PC) value  
- Instruction name and encoding
- Register values accessed
- Memory addresses and data
- Exception and interrupt handling details

**Performance Monitoring:**
The Performance singleton tracks:
- Instructions executed per second
- Memory access patterns and frequency
- Register file utilization statistics
- TLM transaction latencies
- Simulation kernel overhead

**GDB Integration:**
The debug server provides standard GDB remote debugging capabilities:
- Connect via TCP on port 3333 (configurable)
- Set breakpoints with `break` command
- Single-step execution with `stepi`
- Memory inspection with `x` command
- Register examination with `info registers`
- Continue execution with `continue`

Example GDB session:
```bash
# Terminal 1: Start simulator with GDB server
./bin/arm_m_tlm --gdb --hex firmware.hex

# Terminal 2: Connect with GDB
arm-none-eabi-gdb -ex 'target remote localhost:3333'
(gdb) break *0x400
(gdb) continue
(gdb) stepi  
(gdb) info registers
(gdb) x/10x $sp
```

This comprehensive architecture provides a complete virtual platform for ARM Cortex-M0 development, testing, and debugging with performance characteristics suitable for both educational use and embedded software development workflows.
