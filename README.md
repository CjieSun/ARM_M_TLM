# ARM_M_TLM
This is another ARM cortex-M ISA simulator, this is coded in SystemC + TLM-2. It supports ARMv6-M thumb/thumb2 Instruction set.

## Building the Simulator

### Prerequisites
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

   # Show help
   ./bin/arm_m_tlm --help
   ```

## Usage

### Command Line Options
- `--hex <file>`: Load Intel HEX file into memory
- `--log <file>`: Specify log file (default: simulation.log)
- `--debug`: Enable debug-level logging
- `--trace`: Enable instruction-level tracing
- `--help, -h`: Show usage information

### Example Usage
```bash
# Run with maximum logging detail
./bin/arm_m_tlm --hex firmware.hex --trace --log detailed.log

# Quick test run with debug info
./bin/arm_m_tlm --debug
```

## Testing

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

##Description
Brief description of the modules:
  CPU: Top entity that includes all other modules.
  Memory: Memory highly based on TLM-2 example with read file capability
  Registers: Implements the register file, R0~R14, PC and PSR register
  Instruction: Decodes instruction type and keeps instruction field
  BASE_ISA: Executes Base ISA,
      1. Branch
      2. Data-processing
      3. Status register access
      4. Load and store
      5. Load Multiple and store multiple
      6. Miscellaneous
      7. Exception generating
  Simulator: Top-level entity that builds & starts the simulation
  BusCtrl: Simple bus manager
  Trace: Simple trace peripheral
  NVIC: Minimal Nested Vectored Interrupt Controller with SysTick and external IRQ support
  Debug: GDB server for remote debugging (Beta)

Helper classes:
  Performance: Performance indicators stores here (singleton class)
  Log: Log class to log them all (singleton class)
Current performance is about 3.000.000 instructions / sec in a Intel Core i5-5200@2.2Ghz and about 4.500.000 instructions / sec in a Intel Core i7-8550U@1.8Ghz.

Trace perihperal creates a xterm window where it prints out all received data.

Structure
Modules' hierarchy
<img width="718" height="730" alt="image" src="https://github.com/user-attachments/assets/e53aa654-e415-4194-88e7-798a1886adb0" />

Memory map (key regions)
    Base	Module	Description
    0x40000000	Trace	Output data to xterm
    0xE000E000–0xE000EFFF	NVIC	ARMv6-M NVIC register block
    0xE000E010	SysTick CTRL
    0xE000E014	SysTick LOAD (24-bit)
    0xE000E018	SysTick VAL (24-bit)
    0xE000E01C	SysTick CALIB (RO)
    0xE000E100	ISER (Interrupt Set-Enable)
    0xE000E180	ICER (Interrupt Clear-Enable)
    0xE000E200	ISPR (Interrupt Set-Pending)
    0xE000E280	ICPR (Interrupt Clear-Pending)
    0xE000E400–0xE000E41C	IPR0..IPR7 (Priorities)
    0xE000ED1C	SHPR2 (SVCall priority)
    0xE000ED20	SHPR3 (SysTick, PendSV priority)
    0xE000ED24	SHCSR (System Handler Control & State)

2.1 CPU
The ISS simulates a single hardware thread (HART) and includes privileged instructions. It is divided in three modules: Instruction, Execute and Registers:
• Instruction Decodes instructions and checks for extensions.
• Execute Executes instructions, accessing registers and memory and performing operations. 
• Registers Implements the register file for the entire CPU, including general-purpose registers (r0-r31), Program counter(pc) and all necessary entries in Control and Status Registers (CSR) registers.
This CPU is a minimal, fully functional model with a end-less loop fetching and executing instructions without pipeline, branch
predictions or any other optimization technique. All instructions are executed in one single cycle, but it can be easy customized to per instruction cycle count.
The Execute module implements each instruction with a class method that receives the instruction register. These methods perform all necessary steps to execute the instruction. In case of a branch instruction, these methods are able to change the PC value.
For Load/Store instructions, the methods are in charge to access the required memory address.
The CPU is designed following Harvard architecture, hence the ISS has separate TLM sockets to interface with external modules:
• Data bus: Simple initiator socket to access data memory.
• Instruction bus: Simple initiator socket to access instruction memory.
• IRQ line: Simple target socket to signal external IRQs.
2.2 Bus Controller
The simulator also includes a Bus controller in charge of the interconnection of all modules. The bus controller decodes the accesses address and does the communication to the proper module. In the actual status of the project, it contains two target sockets (instruction and data buses) and three initiator sockets: Memory, Trace and Timer modules.
2.3 Memory
The Memory module simulates a simple RAM memory, which is the main memory of the SoC, acting as instruction memory and data memory. This module can read a binary file in Intel HEX format obtained from the .elf file and load it to be the main program for the ISS. This module has a Simple target socket to be accessed that supports DMI to increase simulation speed.
2.4 Peripherals
 NVIC (Nested Vectored Interrupt Controller): Provides ARMv6‑M system handlers and external interrupt control. This simulator models:
  • SysTick timer via SysTick CTRL/LOAD/VAL/CALIB at 0xE000E010..0xE000E01C
  • External IRQ enable/pending via ISER/ICER/ISPR/ICPR
  • System handler priorities via SHPR2/SHPR3 and SHCSR
Using SysTick (Cortex‑M0/M0+ compatible):
  1) Write LOAD with a 24‑bit reload value (N‑1)
  2) Optionally clear VAL by writing any value
  3) Set CTRL bits:
     - Bit0 ENABLE = 1
     - Bit1 TICKINT = 1 to generate exception 15
     - Bit2 CLKSOURCE = 1 (internal clock)
  4) Provide a SysTick_Handler in the vector table (entry 15)
The Trace module is a very simple tracing device, that outputs through a xterm window the characters received. This module is intended as a basic mimic of the ITM module of Cortex-M CPUs. Figure 2 shows the simulator running with an xterm windows as output console.
2.5 Helper
Two other modules are included in the simulator: Performance and Log. The Performance module take statistics of the simulation, like instructions executed, registers accessed, memory accesses, etc.
It dumps this information when the simulation ends. The other module allows the simulator to create a log file with different levels of information.
At maximum level of logging, each instruction executed is logged into the file with its name, address, time and register values or addresses accessed. The log file at maximum debug level shows information about the current time, PC value and the instruction executed. It also prints the values of the registers used. Figure 3 shows a real executed log file.
The log file at maximum debug level shows information about the current time, PC value and the instruction executed. It also prints the values of the registers used. Figure 3 shows a real executed log file.
