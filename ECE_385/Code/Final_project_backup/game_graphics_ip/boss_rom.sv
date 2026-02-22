//-------------------------------------------------------------------------
//  boss_rom.sv
//-------------------------------------------------------------------------
//  Purpose:
//      Wrapper for boss sprite BRAM (blk_mem_gen_4).
//      Stores boss sprites and boss projectile loaded from COE file.
//
//  Memory Organization (6-bit RGB222):
//      - 20 boss frames x 64x64 = 81920 pixels
//      - 1 projectile sprite: 48x32 = 1536 pixels
//      - 6-bit RGB222 per pixel
//      - Total: 83456 words
//
//  Frame Layout:
//      Frames 0-3:   Idle   (4 frames)  addresses 0-16383
//      Frames 4-7:   Flying (4 frames)  addresses 16384-32767
//      Frames 8-15:  Attack (8 frames)  addresses 32768-65535
//      Frames 16-19: Death  (4 frames)  addresses 65536-81919
//      Projectile:   48x32              addresses 81920-83455
//
//  Address Format:
//      Boss: addr = frame * 4096 + y * 64 + x
//            addr = {frame[4:0], y[5:0], x[5:0]}
//      Projectile: addr = 81920 + y * 48 + x
//
//  Data Format: 6-bit RGB222: [5:4]=R, [3:2]=G, [1:0]=B
//      Transparent color: 0x33 (magenta: R=3, G=0, B=3)
//
//  Author: ECE 385 Final Project - Soul Knight Team
//-------------------------------------------------------------------------

module boss_rom (
    input  logic        clk,
    input  logic [16:0] addr,     // 17-bit address for 131072 words (covers 83456 actual)
    output logic [5:0]  data      // 6-bit RGB222 pixel data
);

    // Instantiate BRAM (blk_mem_gen_4, 131072 x 6-bit, actual data: 83456 words)
    blk_mem_gen_4 boss_bram (
        .clka(clk),
        .ena(1'b1),
        .addra(addr),
        .douta(data)
    );

endmodule
