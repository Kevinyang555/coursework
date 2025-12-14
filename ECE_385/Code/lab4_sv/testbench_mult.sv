`timescale 1ns/1ps

module testbench_mult;

    logic Clk;
    logic reset_load_clear;
    logic Run;
    logic [7:0] SW_i;

    logic Xval;
    logic [7:0] Aval;
    logic [7:0] Bval;
    logic [7:0] hex_seg;
    logic [3:0] hex_grid;

    processor_top dut (
        .Clk(Clk),
        .reset_load_clear(reset_load_clear),
        .Run(Run),
        .SW_i(SW_i),
        .Xval(Xval),
        .Aval(Aval),
        .Bval(Bval),
        .hex_seg(hex_seg),
        .hex_grid(hex_grid)
    );

    
    initial begin
        force dut.SW_s = SW_i;
        force dut.reset_load_clear_S = reset_load_clear;
        force dut.Run_S = Run;
    end

    initial Clk = 0;
    always #5 Clk = ~Clk;

    task do_multiply(input signed [7:0] S, input signed [7:0] B);
        begin
            SW_i = B;
            reset_load_clear = 1; Run = 0;
            repeat (2) @(posedge Clk);
            reset_load_clear = 0;
            repeat (2) @(posedge Clk);

            SW_i = S;
            Run = 1;
            repeat (40) @(posedge Clk);  // allow enough cycles
            Run = 0;

            $display("%0d * %0d = %0d", S, B, $signed({Xval, Aval, Bval}));
            repeat (5) @(posedge Clk);
        end
    endtask

    initial begin
        reset_load_clear = 0;
        Run = 0;
        SW_i = 0;
        repeat (10) @(posedge Clk);

        do_multiply(  7,  59);
        do_multiply( -7,  59);
        do_multiply(  7, -59);
        do_multiply( -7, -59);

        $stop;
    end

endmodule
