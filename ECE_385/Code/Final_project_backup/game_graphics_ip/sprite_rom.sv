//-------------------------------------------------------------------------
//  sprite_rom.sv
//-------------------------------------------------------------------------
//  Purpose:
//      Wrapper for sprite BRAM (blk_mem_gen_1).
//      Stores player/entity sprites loaded from COE file.
//
//  Memory Organization (REDUCED FRAMES, 6-bit RGB222):
//      - 32 movement frames (32x32) + 20 attack frames (48x48)
//      - 32x32: 1024 pixels per frame
//      - 48x48: 2304 pixels per frame
//      - 6-bit RGB222 per pixel
//      - Total: 78848 words
//
//  Frame Layout:
//      === 32x32 Movement (frames 0-31, using sheet frames 1,3,5,7) ===
//      0-3:   Idle Down       4-7:   Idle Right
//      8-11:  Idle Up         12-15: Idle Left
//      16-19: Run Down        20-23: Run Right
//      24-27: Run Up          28-31: Run Left
//
//      === 48x48 Attack (frames 32-51) ===
//      32-36: Attack Down     37-41: Attack Left
//      42-46: Attack Right    47-51: Attack Up
//
//  Address Format:
//      32x32: frame * 1024 + y * 32 + x  (frame 0-31)
//      48x48: 32768 + (frame-32) * 2304 + y * 48 + x  (frame 32-51)
//
//  Data Format: 6-bit RGB222: [5:4]=R, [3:2]=G, [1:0]=B
//      Transparent color: 0x33 (magenta: R=3, G=0, B=3)
//
//  Author: ECE 385 Final Project - Soul Knight Team
//-------------------------------------------------------------------------

module sprite_rom (
    input  logic        clk,
    input  logic [16:0] addr,     // 17-bit address for up to 131072 words
    output logic [5:0]  data      // 6-bit RGB222 pixel data
);

    // Instantiate BRAM (blk_mem_gen_1, reconfigured for 6-bit sprites)
    blk_mem_gen_1 sprite_bram (
        .clka(clk),
        .ena(1'b1),
        .addra(addr),
        .douta(data)
    );

endmodule
