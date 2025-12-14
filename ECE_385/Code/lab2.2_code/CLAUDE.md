# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is a SystemVerilog implementation of a 4-bit logic processor for ECE 385. The processor performs bitwise logical operations on two 4-bit registers (A and B) with serial computation capabilities.

## Architecture

### Core Modules

- **Processor.sv**: Top-level module that integrates all components and handles I/O
  - Manages push button inputs (Reset, LoadA, LoadB, Execute)
  - Routes 4-bit data input and control signals
  - Drives hex display outputs

- **Control.sv**: State machine controller with states:
  - `s_start`: Idle state, allows loading registers
  - `s_count0-3`: Four computation cycles for 4-bit serial operation
  - `s_done`: Operation complete state

- **compute.sv**: 1-bit ALU implementing 8 logical functions:
  - 000: AND
  - 001: OR
  - 010: XOR
  - 011: Constant 1
  - 100: NAND
  - 101: NOR
  - 110: XNOR
  - 111: Constant 0

- **Router.sv**: Routes computation results based on 2-bit R signal:
  - 00: A→A, B→B (no change)
  - 01: A→A, F(A,B)→B
  - 10: F(A,B)→A, B→B
  - 11: B→A, A→B (swap)

- **Register_unit.sv**: Contains two 4-bit shift registers (A and B)
- **Reg_4.sv**: Individual 4-bit shift register with load capability
- **Synchronizers.sv**: Button debouncing and synchronization
- **HexDriver.sv**: 7-segment hex display driver

## Simulation

### Test Files
- **testbench.sv**: Basic testbench for processor verification
- **testbench_8.sv**: Extended testbench (likely for 8-bit variant testing)

### Simulation Parameters
- Timeunit: 10ns
- Clock period: 20ns (50 MHz simulation clock)
- Tests include loading values, executing operations, and verifying results

## Development Commands

Since no build scripts or makefiles were found, use standard SystemVerilog simulation tools:

### For ModelSim/Questa:
```bash
# Compile all design files
vlog design_source/*.sv

# Compile testbench
vlog sim_sources/testbench.sv

# Run simulation
vsim -novopt testbench
run -all
```

### For Vivado:
```bash
# Create project and add sources
# Run behavioral simulation through GUI or:
xsim testbench -R
```

## Key Signal Interfaces

- **Inputs**: Clk, Reset, LoadA, LoadB, Execute, Din[3:0], F[2:0], R[1:0]
- **Outputs**: LED[3:0], Aval[3:0], Bval[3:0], hex_seg[7:0], hex_grid[3:0]
- Serial computation takes 4 clock cycles after Execute is pressed