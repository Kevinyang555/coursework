# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is an ECE 385 Lab 7 project implementing an HDMI text controller using SystemVerilog and AXI4-Lite interface. The project involves hardware design for FPGA (likely Xilinx) that displays text on an HDMI monitor through a VGA controller, with software drivers running on a MicroBlaze processor.

The codebase is organized into two weeks, representing progressive lab milestones.

## Architecture

### Hardware (SystemVerilog)

The HDMI text controller implements a complete video pipeline:

1. **Top Level** (`mb_usb_hdmi_top.sv`): Instantiates the Block Design wrapper `hdmi_block1` which contains the MicroBlaze system
   - Receives 100 MHz clock and reset
   - Manages UART and HDMI TMDS differential pairs

2. **AXI4-Lite Slave Interface** (`hdmi_text_controller_v1_0_AXI.sv`):
   - Implements AXI4-Lite protocol for processor communication
   - Address width: 12 bits (parameter `C_S_AXI_ADDR_WIDTH`)
   - Manages 604-entry VRAM array (`slv_regs[604]`)
   - Register map:
     - 0-599: Text VRAM (80 columns × 30 rows / 4 chars per register = 600 registers)
     - 600: Color control register (foreground/background)
     - 601: Frame counter (auto-incremented on vsync)
     - 602-603: DrawX/DrawY position registers at vsync
   - Handles vsync-synchronized frame counting

3. **HDMI Controller** (`hdmi_text_controller_v1_0.sv`):
   - Instantiates clock wizard to generate 25 MHz pixel clock and 125 MHz serialization clock
   - Connects VGA controller, color mapper, and HDMI TX IP
   - Passes VRAM from AXI interface to color mapper

4. **VGA Controller** (`VGA_controller.sv`):
   - Generates 640×480 @ 60Hz timing (actual rate ~59.94Hz due to 25MHz vs 25.175MHz)
   - Horizontal: 800 total pixels (640 visible)
   - Vertical: 525 total lines (480 visible)
   - Outputs drawX, drawY coordinates and sync signals

5. **Color Mapper** (`color_mapper.sv`):
   - Text display: 80 columns × 30 rows
   - Character size: 8×16 pixels
   - Memory organization: 4 ASCII characters per 32-bit VRAM register
   - Character format: bit[7] = inverted flag, bits[6:0] = ASCII code
   - Color register (VRAM[600]) format:
     - Bits[27:16]: Foreground RGB (4 bits per channel)
     - Bits[11:0]: Background RGB (4 bits per channel)
   - Performs real-time character-to-pixel conversion using font ROM

6. **Font ROM** (`font_rom.sv`): Contains 8×16 bitmap font data for ASCII characters

### Software (C)

**Driver** (`drivers_src/hdmi_text_controller.c/h`):

- Memory-mapped structure `HDMI_TEXT_STRUCT`:
  - `VRAM[ROWS*COLUMNS]`: Text buffer (byte-addressable view)
  - `COLOR`: 32-bit color control
  - `FRAME_COUNT`, `DRAWX`, `DRAWY`: Read-only status registers

- Key functions:
  - `hdmiSetColor(background, foreground)`: Set display colors
  - `hdmiClr()`: Clear all VRAM to 0x00
  - `hdmiTestWeek1()`: Comprehensive test routine that validates:
    - AXI read/write with checksum verification
    - Color rendering (green, red, blue)
    - Character inversion (bit 7 set)
    - Vsync synchronization registers

- Base address defined via Xilinx parameter: `XPAR_HDMI_TEXT_CONTROLLER_0_AXI_BASEADDR`

## Key Technical Details

### Memory Addressing

- **VRAM Layout**: Linear addressing where `register_index = row * 20 + (column / 4)`
- **Character Extraction**: `charData = VRAM[RegIdx][(ByteReg+1)*8-1:ByteReg*8]` where ByteReg = column % 4
- **Font Address**: `font_addr = ascii_char * 16 + pixel_row` (16 bytes per character)

### Clock Domain Crossing

The AXI interface runs on `axi_aclk` while the video pipeline runs on 25 MHz pixel clock. The vsync signal crosses from pixel clock to AXI clock domain for frame counter synchronization (using edge detection).

### AXI4-Lite Implementation

- Uses standard handshake: AWVALID/AWREADY, WVALID/WREADY, ARVALID/ARREADY, RVALID/RREADY
- Write strobes (`S_AXI_WSTRB`) enable byte-level writes
- Address decode restricted to registers 0-600 for writes
- Single-transaction protocol (no bursting, `aw_en` prevents outstanding transactions)

## Development Workflow

This is an educational lab project - there are no build scripts. Development would typically involve:

1. Synthesize/implement in Vivado (Xilinx FPGA toolchain)
2. Program hardware with generated bitstream
3. Compile C driver code in Vitis/SDK for MicroBlaze
4. Debug hardware via ILA or software via UART terminal

## File Organization

- `week1/`: Initial implementation milestone
- `week2/`: Extended functionality milestone
- Each week contains identical file structure (`.sv` hardware, `drivers_src/` software, `.xdc` constraints)
