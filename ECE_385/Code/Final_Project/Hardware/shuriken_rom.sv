//shuriken_rom.sv
//Wrapper for player projectile (shuriken) BRAM (blk_mem_gen_5).


module shuriken_rom (
    input  logic        clk,
    input  logic [9:0]  addr,     
    output logic [11:0] data      // 12-bit RGB444 pixel data
);

    blk_mem_gen_5 shuriken_bram (
        .clka(clk),
        .ena(1'b1),
        .addra(addr),
        .douta(data)
    );

endmodule
