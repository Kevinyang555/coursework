//tile_rom.sv
//Combined storage for tile graphics AND tilemap indices (16384 x 12-bit).


module tile_rom (
    input  logic        clk,           
    input  logic [13:0] addr,          
    output logic [11:0] data           // 12-bit output (RGB444 or tile index)
);

    blk_mem_gen_2 tile_bram (
        .clka(clk),
        .ena(1'b1),
        .addra(addr),         
        .douta(data)           
    );

endmodule
