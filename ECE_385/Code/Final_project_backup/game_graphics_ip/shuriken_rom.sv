//-------------------------------------------------------------------------
//  shuriken_rom.sv
//-------------------------------------------------------------------------
//  Purpose:
//      Wrapper for player projectile (shuriken) BRAM (blk_mem_gen_5).
//      Stores shuriken sprites loaded from COE file.
//
//  Memory Organization (12-bit RGB444):
//      - 4 rotation frames x 16x16 = 1024 pixels
//      - 12-bit RGB444 per pixel
//      - Total: 1024 words
//
//  Frame Layout:
//      Frame 0: addresses 0-255
//      Frame 1: addresses 256-511
//      Frame 2: addresses 512-767
//      Frame 3: addresses 768-1023
//
//  Address Format:
//      addr = frame * 256 + y * 16 + x
//      addr = {frame[1:0], y[3:0], x[3:0]}
//
//  Data Format: 12-bit RGB444: [11:8]=R, [7:4]=G, [3:0]=B
//      Transparent color: 0xF0F (magenta: R=15, G=0, B=15)
//
//  Author: ECE 385 Final Project - Soul Knight Team
//-------------------------------------------------------------------------

module shuriken_rom (
    input  logic        clk,
    input  logic [9:0]  addr,     // 10-bit address for 1024 words
    output logic [11:0] data      // 12-bit RGB444 pixel data
);

    // Instantiate BRAM (blk_mem_gen_5, 1024 x 12-bit)
    blk_mem_gen_5 shuriken_bram (
        .clka(clk),
        .ena(1'b1),
        .addra(addr),
        .douta(data)
    );

endmodule
