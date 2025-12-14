//enemy_rom.sv
//Wrapper for enemy + projectile sprite BRAM (blk_mem_gen_3).


module enemy_rom (
    input  logic        clk,
    input  logic [15:0] addr,     
    output logic [5:0]  data      
);

    blk_mem_gen_0 enemy_bram (
        .clka(clk),
        .ena(1'b1),
        .addra(addr),
        .douta(data)
    );

endmodule
