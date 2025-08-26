# GDB Debugging Support

This ARM Cortex-M0 simulator now supports remote debugging with GDB using the GDB Remote Serial Protocol (RSP).

## Prerequisites

- ARM GDB debugger: `arm-none-eabi-gdb` (install with `sudo apt-get install gcc-arm-none-eabi`)
- Or any GDB that supports ARM targets

## Usage

### 1. Start the Simulator with GDB Server

```bash
# Start GDB server on default port 3333
./bin/arm_m_tlm --gdb

# Start GDB server on custom port
./bin/arm_m_tlm --gdb-port 1234

# With program loading and debug logging
./bin/arm_m_tlm --hex program.hex --gdb --debug
```

The simulator will start and display:
```
GDB server enabled on port 3333
Starting simulation...
Waiting for GDB connection...
Connect with: arm-none-eabi-gdb -ex 'target remote localhost:3333'
```

### 2. Connect with GDB

In another terminal:

```bash
# Quick connection
arm-none-eabi-gdb -ex 'target remote localhost:3333'

# Or step by step
arm-none-eabi-gdb
(gdb) target remote localhost:3333
```

### 3. Basic GDB Commands

Once connected, you can use standard GDB commands:

```gdb
# View registers
(gdb) info registers

# Read memory
(gdb) x/10x 0x0      # Read 10 words from address 0x0
(gdb) x/10i 0x0      # Disassemble 10 instructions from address 0x0

# Set breakpoints  
(gdb) break *0x100   # Set breakpoint at address 0x100

# Control execution
(gdb) continue       # Continue execution
(gdb) stepi         # Single step instruction
(gdb) next          # Step to next instruction

# Modify registers
(gdb) set $r0 = 0x12345678
(gdb) set $pc = 0x200

# Modify memory
(gdb) set {int}0x20000000 = 0xdeadbeef
```

## Supported GDB Features

- ✅ Register read/write (R0-R15, PSR)
- ✅ Memory read/write  
- ✅ Single step execution
- ✅ Continue execution
- ✅ Software breakpoints
- ✅ Basic query commands
- ✅ Connection management

## Example Debug Session

1. Start simulator:
   ```bash
   ./bin/arm_m_tlm --gdb --debug
   ```

2. Connect GDB:
   ```bash
   arm-none-eabi-gdb -ex 'target remote localhost:3333'
   ```

3. Debug:
   ```gdb
   (gdb) info registers     # View current CPU state
   (gdb) x/4x 0x0          # Check vector table
   (gdb) stepi             # Execute one instruction
   (gdb) continue          # Run until breakpoint
   ```

## Protocol Details

The GDB server implements the GDB Remote Serial Protocol (RSP) over TCP. Supported commands include:

- `g` - Read all registers
- `G` - Write all registers  
- `m<addr>,<len>` - Read memory
- `M<addr>,<len>:<data>` - Write memory
- `c` - Continue execution
- `s` - Single step
- `Z0,<addr>,<kind>` - Insert breakpoint
- `z0,<addr>,<kind>` - Remove breakpoint
- `?` - Get halt reason
- `qSupported` - Query capabilities

## Troubleshooting

**Port already in use**: Try a different port with `--gdb-port <port>`

**Connection refused**: Ensure the simulator is running and listening

**GDB won't connect**: Check that you're using an ARM-compatible GDB version

**Simulation completes too quickly**: The simulator runs indefinitely when GDB is enabled to allow debugging