//-------------------------------------------------------------------------
//  tile_rom.sv
//-------------------------------------------------------------------------
//  Purpose:
//      Combined storage for tile graphics AND tilemap indices.
//      Uses 100 MHz clock so BRAM read completes within one 25 MHz pixel cycle.
//
//  Memory Layout (16384 x 12-bit words):
//      0x0000 - 0x10FF: Tile pixel data (17 tiles x 256 pixels each)
//      0x1000 - 0x1383: Tilemap 0 - Empty room (900 entries)
//      0x1384 - 0x1707: Tilemap 1 - I-shape room (900 entries)
//      0x1708 - 0x1A8B: Tilemap 2 - Cross room (900 entries)
//      0x1A8C - 0x1E0F: Tilemap 3 - Pillars room (900 entries)
//      0x1E10 - 0x2193: Tilemap 4 - Stair room (900 entries)
//
//  RGB444 Format (12 bits per pixel):
//      - Bits [11:8]:  Red (4 bits, 16 levels)
//      - Bits [7:4]:   Green (4 bits, 16 levels)
//      - Bits [3:0]:   Blue (4 bits, 16 levels)
//
//  Tilemap Entry Format (12 bits, only lower 8 used):
//      - Bits [7:0]: Tile index (0-255, supports up to 256 tiles)
//
//  Address Calculation:
//      Tile pixels: addr = tile_idx * 256 + pixel_y * 16 + pixel_x
//      Tilemap:     addr = 0x1000 + (map_id * 900) + row * 30 + col
//
//  Clock:
//      - Uses 100 MHz clock (not 25 MHz pixel clock)
//      - 2-cycle BRAM latency = 20ns, fits within 40ns pixel period
//
//  Author: ECE 385 Final Project - Soul Knight Team
//-------------------------------------------------------------------------

module tile_rom (
    input  logic        clk,           // 100 MHz clock (fast clock)
    input  logic [13:0] addr,          // 14-bit address (up to 16384 words)
    output logic [11:0] data           // 12-bit output (RGB444 or tile index)
);

    // Instantiate Block RAM for combined tile + tilemap storage
    // Configuration: 16384 x 12-bit, Single Port ROM
    // Uses 100 MHz clock for fast reads
    blk_mem_gen_2 tile_bram (
        .clka(clk),
        .ena(1'b1),
        .addra(addr),          // Full 14-bit address for 16384 words
        .douta(data)           // 12-bit output
    );

endmodule
