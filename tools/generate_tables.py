#!/usr/bin/env python3
"""
Instruction Table Generator for ARM_M_TLM
Generates instruction encoding tables in various formats from the C++ tables.
"""

import sys
import os
import subprocess
import tempfile

def create_test_program():
    """Create a simple test program that uses the table generation functions."""
    cpp_code = """
#include <iostream>
#include "InstructionTablesStandalone.h"

int main() {
    generate_instruction_tables();
    return 0;
}
"""
    return cpp_code

def compile_and_run_table_generator():
    """Compile a simple program to generate the tables and run it."""
    
    # Get the project root directory
    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_root = os.path.dirname(script_dir)
    tools_dir = script_dir
    
    # Create temporary files
    with tempfile.NamedTemporaryFile(mode='w', suffix='.cpp', delete=False) as f:
        f.write(create_test_program())
        temp_cpp = f.name
    
    temp_exe = temp_cpp.replace('.cpp', '')
    
    try:
        # Compile command (standalone version without SystemC)
        compile_cmd = [
            'g++', '-std=c++17', '-I' + tools_dir,
            temp_cpp,
            os.path.join(tools_dir, 'InstructionTablesStandalone.cpp'),
            '-o', temp_exe
        ]
        
        print("Compiling table generator...")
        result = subprocess.run(compile_cmd, capture_output=True, text=True)
        
        if result.returncode != 0:
            print("Compilation failed:")
            print("STDOUT:", result.stdout)
            print("STDERR:", result.stderr)
            return False
        
        # Run the program
        print("Running table generator...")
        result = subprocess.run([temp_exe], capture_output=True, text=True)
        
        if result.returncode != 0:
            print("Execution failed:")
            print("STDERR:", result.stderr)
            return False
            
        print("Generated instruction tables:")
        print(result.stdout)
        
        return True
        
    finally:
        # Clean up
        if os.path.exists(temp_cpp):
            os.unlink(temp_cpp)
        if os.path.exists(temp_exe):
            os.unlink(temp_exe)

def generate_html_table():
    """Generate an HTML version of the instruction tables."""
    html_content = """
<!DOCTYPE html>
<html>
<head>
    <title>ARM Cortex-M Thumb Instruction Encoding Tables</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; }
        table { border-collapse: collapse; width: 100%; margin: 20px 0; }
        th, td { border: 1px solid #ddd; padding: 8px; text-align: left; }
        th { background-color: #f2f2f2; }
        .opcode { font-family: monospace; }
        h1, h2 { color: #333; }
    </style>
</head>
<body>
    <h1>ARM Cortex-M Thumb Instruction Encoding Tables</h1>
    
    <h2>16-bit Thumb instruction encoding</h2>
    <p>The encoding of 16-bit Thumb instructions is:</p>
    <div style="font-family: monospace; margin: 10px 0;">
    15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0<br>
    |___________________opcode_______________|
    </div>
    
    <table>
        <caption><strong>Table: 16-bit Thumb instruction encoding</strong></caption>
        <thead>
            <tr>
                <th>opcode</th>
                <th>Instruction or instruction class</th>
            </tr>
        </thead>
        <tbody>
            <tr><td class="opcode">000xxx</td><td>Shift (immediate), add, subtract, move, and compare</td></tr>
            <tr><td class="opcode">001xxx</td><td>Move/compare/add/subtract immediate</td></tr>
            <tr><td class="opcode">010000</td><td>Data processing</td></tr>
            <tr><td class="opcode">010001</td><td>Special data instructions and branch and exchange</td></tr>
            <tr><td class="opcode">01001x</td><td>Load from Literal Pool</td></tr>
            <tr><td class="opcode">0101xx</td><td>Load/store single data item (register offset)</td></tr>
            <tr><td class="opcode">011xxx</td><td>Load/store single data item (immediate offset)</td></tr>
            <tr><td class="opcode">1000xx</td><td>Load/store single data item (halfword)</td></tr>
            <tr><td class="opcode">1001xx</td><td>SP-relative load/store</td></tr>
            <tr><td class="opcode">1010xx</td><td>Generate PC/SP-relative address</td></tr>
            <tr><td class="opcode">1011xx</td><td>Miscellaneous 16-bit instructions</td></tr>
            <tr><td class="opcode">11000x</td><td>Store multiple registers</td></tr>
            <tr><td class="opcode">11001x</td><td>Load multiple registers</td></tr>
            <tr><td class="opcode">1101xx</td><td>Conditional branch, and Supervisor Call</td></tr>
            <tr><td class="opcode">11100x</td><td>Unconditional Branch</td></tr>
            <tr><td class="opcode">1111xx</td><td>32-bit instruction prefixes</td></tr>
        </tbody>
    </table>
    
    <h3>Format 4: Data processing operations</h3>
    <table>
        <thead>
            <tr>
                <th>op[3:0]</th>
                <th>Mnemonic</th>
                <th>Description</th>
            </tr>
        </thead>
        <tbody>
            <tr><td class="opcode">0000</td><td>AND</td><td>Bitwise AND</td></tr>
            <tr><td class="opcode">0001</td><td>EOR</td><td>Bitwise exclusive OR</td></tr>
            <tr><td class="opcode">0010</td><td>LSL</td><td>Logical shift left</td></tr>
            <tr><td class="opcode">0011</td><td>LSR</td><td>Logical shift right</td></tr>
            <tr><td class="opcode">0100</td><td>ASR</td><td>Arithmetic shift right</td></tr>
            <tr><td class="opcode">0101</td><td>ADC</td><td>Add with carry</td></tr>
            <tr><td class="opcode">0110</td><td>SBC</td><td>Subtract with carry</td></tr>
            <tr><td class="opcode">0111</td><td>ROR</td><td>Rotate right</td></tr>
            <tr><td class="opcode">1000</td><td>TST</td><td>Test</td></tr>
            <tr><td class="opcode">1001</td><td>NEG</td><td>Negate</td></tr>
            <tr><td class="opcode">1010</td><td>CMP</td><td>Compare</td></tr>
            <tr><td class="opcode">1011</td><td>CMN</td><td>Compare negative</td></tr>
            <tr><td class="opcode">1100</td><td>ORR</td><td>Bitwise OR</td></tr>
            <tr><td class="opcode">1101</td><td>MUL</td><td>Multiply</td></tr>
            <tr><td class="opcode">1110</td><td>BIC</td><td>Bit clear</td></tr>
            <tr><td class="opcode">1111</td><td>MVN</td><td>Bitwise NOT</td></tr>
        </tbody>
    </table>

    <h2>32-bit Thumb instruction encoding</h2>
    <table>
        <caption><strong>Table: 32-bit Thumb instruction encoding</strong></caption>
        <thead>
            <tr>
                <th>opcode</th>
                <th>Instruction or instruction class</th>
            </tr>
        </thead>
        <tbody>
            <tr><td class="opcode">11110x</td><td>Branch with Link (BL)</td></tr>
        </tbody>
    </table>
    
    <p><em>Generated by ARM_M_TLM Instruction Table Generator</em></p>
</body>
</html>
"""
    return html_content

def main():
    print("ARM_M_TLM Instruction Table Generator")
    print("=====================================")
    
    if len(sys.argv) < 2:
        print("Usage: python3 generate_tables.py [console|html]")
        sys.exit(1)
    
    output_format = sys.argv[1].lower()
    
    if output_format == "console":
        compile_and_run_table_generator()
    elif output_format == "html":
        html_content = generate_html_table()
        output_file = "instruction_tables.html"
        with open(output_file, 'w') as f:
            f.write(html_content)
        print(f"HTML table generated: {output_file}")
    else:
        print("Invalid format. Use 'console' or 'html'")
        sys.exit(1)

if __name__ == "__main__":
    main()