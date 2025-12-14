//boss_rom.sv
//Wrapper for boss sprite BRAM (blk_mem_gen_4).


module boss_rom (
    input  logic        clk,
    input  logic [16:0] addr,     // 17-bit address for 131072 words (covers 83456 actual)
    output logic [5:0]  data      // 6-bit RGB222 pixel data
);

    blk_mem_gen_4 boss_bram (
        .clka(clk),
        .ena(1'b1),
        .addra(addr),
        .douta(data)
    );

endmodule
