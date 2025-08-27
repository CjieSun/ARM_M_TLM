# GDB Debugging Support

This ARM Cortex-M0 simulator now supports remote debugging with GDB using the GDB Remote Serial Protocol (RSP).

## Prerequisites

- ARM GDB debugger: 
  - `arm-none-eabi-gdb` (install with `sudo apt-get install gcc-arm-none-eabi`)
  - OR `gdb-multiarch` (install with `sudo apt-get install gdb-multiarch`)
- Any GDB that supports ARM targets

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
# Using arm-none-eabi-gdb (recommended)
arm-none-eabi-gdb c_test.elf \
  -ex 'set architecture armv6-m' \
  -ex 'target remote localhost:3333'

# Using gdb-multiarch (alternative)
gdb-multiarch c_test.elf \
  -ex 'set architecture armv6-m' \
  -ex 'target remote localhost:3333'

# Or step by step with gdb-multiarch
gdb-multiarch c_test.elf
(gdb) set architecture armv6-m
(gdb) target remote localhost:3333

# Quick connection without symbols (not recommended)
gdb-multiarch -ex 'set architecture armv6-m' -ex 'target remote localhost:3333'
```

**Important Notes**: 
- Always set architecture to `armv6-m` for Cortex-M0+ compatibility
- Load the ELF file with symbols (`c_test.elf` not `c_test.hex`) for best debugging experience
- Both `arm-none-eabi-gdb` and `gdb-multiarch` are supported
- The simulator is optimized for both GDB variants

### 3. Basic GDB Commands

Once connected, you can use these GDB commands (note: remote debugging has some limitations):

```gdb
# View registers
(gdb) info registers

# Read memory
(gdb) x/10x 0x0      # Read 10 words from address 0x0
(gdb) x/10i 0x0      # Disassemble 10 instructions from address 0x0

# Set breakpoints  
(gdb) break *0x100   # Set breakpoint at address 0x100
(gdb) hbreak *0x100  # Hardware breakpoint (if supported)

# Control execution - USE THESE INSTEAD OF 'run'
(gdb) continue       # Continue execution from current point
(gdb) stepi         # Single step instruction
(gdb) nexti         # Step over instruction
(gdb) finish        # Run until function returns

# DO NOT USE 'run' - the program is already running on the remote target
# (gdb) run          # ❌ This will give "remote target does not support run" error

# Modify registers
(gdb) set $r0 = 0x12345678
(gdb) set $pc = 0x200

# Modify memory
(gdb) set {int}0x20000000 = 0xdeadbeef

# Get current program counter and stack pointer
(gdb) print $pc
(gdb) print $sp

# View call stack (if symbols are loaded)
(gdb) backtrace
(gdb) bt
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
   ./bin/arm_m_tlm --hex c_test.hex --gdb --debug
   ```

2. Connect GDB (in another terminal):
   ```bash
   # With symbol file for better debugging
   arm-none-eabi-gdb c_test.elf -ex 'target remote localhost:3333'
   
   # Or without symbols
   arm-none-eabi-gdb -ex 'target remote localhost:3333'
   ```

3. Debug session:
   ```gdb
   (gdb) target remote localhost:3333
   Remote debugging using localhost:3333
   
   # Check current state
   (gdb) info registers     # View current CPU state
   (gdb) x/4x 0x0          # Check vector table
   (gdb) x/10i $pc         # Disassemble from current PC
   
   # Set breakpoint and continue
   (gdb) break *0x100      # Set breakpoint at address
   (gdb) continue          # Resume execution (will hit breakpoint)
   
   # Single step debugging
   (gdb) stepi             # Execute one instruction
   (gdb) stepi             # Execute another instruction
   (gdb) continue          # Continue until next breakpoint
   
   # Examine memory and registers
   (gdb) x/10x 0x20000000  # Check RAM contents
   (gdb) print $r0         # Print register value
   (gdb) set $r1 = 0x1234  # Modify register
   ```

4. Common debugging workflow:
   ```gdb
   # Start debugging from reset
   (gdb) continue          # Let program run from reset vector
   
   # If program gets stuck or you want to pause:
   (gdb) ^C               # Ctrl+C to interrupt
   (gdb) info registers   # Check current state
   (gdb) x/5i $pc        # See next instructions
   (gdb) stepi           # Step through instructions
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

## Quick Reference

| GDB Command | Description | Notes |
|-------------|-------------|--------|
| `target remote localhost:3333` | Connect to simulator | Required first step |
| `file program.elf` | Load symbols | Optional but recommended |
| `info registers` | Show all registers | |
| `x/10x 0x0` | Examine memory (hex) | |
| `x/10i $pc` | Disassemble instructions | |
| `break *0x100` | Set breakpoint | Use even addresses for Thumb |
| `continue` | Resume execution | ✅ Use this, NOT `run` |
| `stepi` | Single step instruction | |
| `nexti` | Step over instruction | |
| `print $pc` | Show program counter | |
| `set $r0=0x1234` | Set register value | |
| `^C` | Interrupt execution | Ctrl+C to pause |

## Troubleshooting

**Port already in use**: Try a different port with `--gdb-port <port>`

**Connection refused**: Ensure the simulator is running and listening

**GDB won't connect**: Check that you're using an ARM-compatible GDB version

**"No executable has been specified" warning**: This is normal when debugging without symbols. To eliminate it:
```gdb
(gdb) file program.elf  # Load the ELF file with symbols
```

**"Truncated register" warning**: This warning has been resolved in the latest version. If you still see it:
- Set the correct architecture: `(gdb) set architecture armv6-m`
- The GDB server now sends 32 registers for full compatibility

**"Cannot find bounds of current function"**: This happens when debugging without symbols:
- Load the ELF file: `arm-none-eabi-gdb program.elf`
- Use `stepi` instead of `step` for instruction-level stepping
- Use `nexti` instead of `next` for instruction-level stepping

**"remote target does not support run"**: This is correct behavior. In remote debugging:
- ❌ Don't use: `run`, `start`  
- ✅ Use instead: `continue`, `stepi`, `nexti`
- The program is already loaded and running on the remote target

**Program doesn't stop at breakpoints**: Make sure you're setting breakpoints at valid instruction addresses (even addresses for Thumb mode)

**Simulation completes too quickly**: The simulator runs indefinitely when GDB is enabled to allow debugging

**GDB commands seem to hang**: The simulator might be waiting for GDB commands. Use `continue` to resume execution or `stepi` to step through instructions.