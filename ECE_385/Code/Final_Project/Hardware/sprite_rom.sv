//sprite_rom.sv
//Wrapper for player sprite BRAM (blk_mem_gen_1).


module sprite_rom (
    input  logic        clk,
    input  logic [16:0] addr,     
    output logic [5:0]  data      
);

    blk_mem_gen_1 sprite_bram (
        .clka(clk),
        .ena(1'b1),
        .addra(addr),
        .douta(data)
    );

endmodule
