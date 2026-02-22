# ECE 385: Digital Systems Laboratory

## Course Information

**Subject Area:** Computer Engineering
**Course Director:** Zuofu Cheng
**Prerequisites:** ECE 110 and ECE 220

### Description

Introduction to the experimental analysis and synthesis of digital networks, including the use of a microcomputer as a controller. Design, build, and test digital systems using transistor-transistor logic (TTL), SystemVerilog, and field-programmable gate arrays (FPGAs).

### Course Goals

This course is designed to give students in computer and electrical engineering an ability to design, build, and debug digital systems that include programmable logic, microprocessors, memory systems, and peripherals.

### Topics Covered

- Combinational logic circuits
- Storage elements
- Hazards and race conditions
- Circuit characteristics (fanout, delays, etc.)
- Field Programmable Gate Arrays (FPGAs)
- Combinational networks (adders, multiplexes, etc.) in SystemVerilog
- Sequential networks (counters, shift registers, etc.) in SystemVerilog
- Synchronous state machines
- Static timing analysis, clock domains, metastability, and synchronization
- Logic simulation and testbenches
- Microprocessors and system on chip
- Project using a microprocessor and system on chip concepts

---

## Repository Structure

This repository contains **clean code exports** from labs 2-7. Full Vivado projects are kept locally but not tracked in version control due to size.

### What's Included

```
Code/
├── lab2.2_code/    - Lab 2: Basic combinational logic
├── lab3_code/      - Lab 3: Sequential logic and storage elements
├── lab4_sv/        - Lab 4: SystemVerilog design
├── lab5_code/      - Lab 5: State machines
├── lab6_code/      - Lab 6: Memory and I/O systems
└── lab7_code/      - Lab 7: Advanced digital systems
```

### What's NOT Included

- Full Vivado project files (`.xpr`, `.runs/`, `.cache/`, etc.)
- Final Project (maintained in separate repository)
- Build artifacts and generated files
- IP cores and custom IP repositories

---

## Development Environment

### Software Tools

- **Xilinx Vivado** - FPGA synthesis and implementation
- **Xilinx Vitis** - Embedded software development (for later labs)
- **ModelSim/Vivado Simulator** - HDL simulation and testbenches

### Hardware Platforms

- **FPGA:** Xilinx Spartan-7 (or similar development board)
- **Lab Equipment:** Oscilloscope, logic analyzer, pulse generator
- **Protoboard and TTL chips** (early labs)

---

## Lab Overview

### Lab 2: Combinational Logic
Introduction to basic logic gates, multiplexers, and combinational circuits using TTL chips and/or FPGA.

### Lab 3: Sequential Logic
Flip-flops, registers, and sequential circuit design. Introduction to timing analysis.

### Lab 4: SystemVerilog Design
Transition to hardware description languages (HDL). Design and simulation of combinational and sequential circuits in SystemVerilog.

### Lab 5: State Machines
Finite state machine (FSM) design, Moore and Mealy machines, state encoding, and synchronous design principles.

### Lab 6: Memory and I/O Systems
Working with memory blocks (RAM/ROM), interfacing with peripherals, and data path design.

### Lab 7: Advanced Systems
Integration of multiple components, system-on-chip concepts, and preparation for final project.

---

## ABET Classification

- **Engineering Science:** 10%
- **Engineering Design:** 90%

---

## Final Project

The final project (weeks 8-12) involves designing, building, and debugging a complex digital system using FPGA hardware and system-on-chip platform. The final project is maintained in a **separate repository**.

---

## Notes

- This repository contains **source code only** - portable across different development environments
- Full project files with Vivado configurations are maintained locally
- Code exports are organized for easy review and portfolio purposes
- For build instructions and detailed documentation, refer to individual lab directories

---

## Contact

For questions about course content, contact the ECE 385 course staff or refer to the official course website.
