//-------------------------------------------------------------------------
//  enemy_rom.sv
//-------------------------------------------------------------------------
//  Purpose:
//      Wrapper for enemy + projectile sprite BRAM (blk_mem_gen_3).
//      Stores enemy sprites and projectile sprite loaded from COE file.
//
//  Memory Organization (6-bit RGB222):
//      - 6 enemy types x 8 frames each = 48 frames total
//      - 32x32: 1024 pixels per frame
//      - 1 projectile sprite: 16x16 = 256 pixels
//      - 6-bit RGB222 per pixel
//      - Total: 49408 words
//
//  Frame Layout:
//      Enemy Type 0 (Frankie):     addresses 0-8191
//      Enemy Type 1 (MINION_10):   addresses 8192-16383
//      Enemy Type 2 (MINION_11):   addresses 16384-24575
//      Enemy Type 3 (MINION_12):   addresses 24576-32767
//      Enemy Type 4 (MINION_9):    addresses 32768-40959
//      Enemy Type 5 (Ghost):       addresses 40960-49151
//      Projectile:                 addresses 49152-49407 (16x16)
//      Each enemy type:
//        0-3:   Idle (4 animation frames)
//        4-7:   Walk (4 animation frames)
//
//  Address Format:
//      Enemy: addr = enemy_type * 8192 + frame * 1024 + y * 32 + x
//             addr = {enemy_type[2:0], frame[2:0], y[4:0], x[4:0]}
//      Projectile: addr = 49152 + y * 16 + x
//
//  Data Format: 6-bit RGB222: [5:4]=R, [3:2]=G, [1:0]=B
//      Transparent color: 0x33 (magenta: R=3, G=0, B=3)
//
//  Author: ECE 385 Final Project - Soul Knight Team
//-------------------------------------------------------------------------

module enemy_rom (
    input  logic        clk,
    input  logic [15:0] addr,     // 16-bit address for 49408 words (6 enemies + projectile)
    output logic [5:0]  data      // 6-bit RGB222 pixel data
);

    // Instantiate BRAM (blk_mem_gen_3, 65536 x 6-bit, actual data: 49408 words)
    blk_mem_gen_0 enemy_bram (
        .clka(clk),
        .ena(1'b1),
        .addra(addr),
        .douta(data)
    );

endmodule
