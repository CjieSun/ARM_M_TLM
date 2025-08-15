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
  Timer: Simple IRQ programable real-time counter peripheral
  Debug: GDB server for remote debugging (Beta)

Helper classes:
  Performance: Performance indicators stores here (singleton class)
  Log: Log class to log them all (singleton class)
Current performance is about 3.000.000 instructions / sec in a Intel Core i5-5200@2.2Ghz and about 4.500.000 instructions / sec in a Intel Core i7-8550U@1.8Ghz.

Trace perihperal creates a xterm window where it prints out all received data.

Structure
Modules' hierarchy
<img width="718" height="730" alt="image" src="https://github.com/user-attachments/assets/e53aa654-e415-4194-88e7-798a1886adb0" />

Memory map
  Base	Module	Description
  0x40000000	Trace	Output data to xterm
  0x40004000	Timer	LSB Timer
  0x40004004	Timer	MSB Timer
  0x40004008	Timer	MSB Timer Comparator
  0x4000400C	Timer	LSB Timer Comparator

2.1 CPU
The ISS simulates a single hardware thread (HART) and includes
privileged instructions. It is divided in three modules: Instruction,
Execute and Registers:
• Instruction Decodes instructions and checks for extensions.
• Execute Executes instructions, accessing registers and memory and performing operations. 
• Registers Implements the register file for the entire CPU, including general-purpose registers (r0-r31), Program counter(pc) and all necessary entries in Control and Status Registers (CSR) registers.
This CPU is a minimal, fully functional model with a end-less
loop fetching and executing instructions without pipeline, branch
predictions or any other optimization technique. All instructions
are executed in one single cycle, but it can be easy customized to
per instruction cycle count.
The Execute module implements each instruction with a class
method that receives the instruction register. These methods perform all necessary steps to execute the instruction. In case of a
branch instruction, these methods are able to change the PC value.
For Load/Store instructions, the methods are in charge to access
the required memory address.
The CPU is designed following Harvard architecture, hence the
ISS has separate TLM sockets to interface with external modules:
• Data bus: Simple initiator socket to access data memory.
• Instruction bus: Simple initiator socket to access instruction memory.
• IRQ line: Simple target socket to signal external IRQs.
2.2 Bus Controller
The simulator also includes a Bus controller in charge of the interconnection of all modules. The bus controller decodes the accesses
address and does the communication to the proper module. In the actual status of the project, it contains two target sockets (instruction and data buses) and three initiator sockets: Memory, Trace and Timer modules.
2.3 Peripherals
The Memory module simulates a simple RAM memory, which is the
main memory of the SoC, acting as instruction memory and data
memory. This module can read a binary file in Intel HEX format
obtained from the .elf file and load it to be the main program for
the ISS. This module has a Simple target socket to be accessed that
supports DMI to increase simulation speed.
The simulated Soc includes a very basic Timer module. This
module includes two 64 bits register mapped to 4 addresses. On of
this registers (mtime) keeps current simulated time in nanosecodns
resolution. The second register (mtimecmp) is intended to program
a future IRQ. The module triggers an IRQ using its Simple initiator
socket.
The Trace module is a very simple tracing device, that outputs
through a xterm window the characters received. This module is
intended as a basic mimic of the ITM module of Cortex-M CPUs
[10]. Figure 2 shows the simulator running with an xterm windows
as output console.
Two other modules are included in the simulator: Performance
and Log. The Performance module take statistics of the simulation,
like instructions executed, registers accessed, memory accesses, etc.
It dumps this information when the simulation ends. The other
module allows the simulator to create a log file with different levels
of information.
At maximum level of logging, each instruction executed is logged
into the file with its name, address, time and register values or
addresses accessed. The log file at maximum debug level shows
information about the current time, PC value and the instruction
executed. It also prints the values of the registers used. Figure 3
shows a real executed log file.
The log file at maximum debug level shows information about the
current time, PC value and the instruction executed. It also prints
the values of the registers used. Figure 3 shows a real executed log
file.
