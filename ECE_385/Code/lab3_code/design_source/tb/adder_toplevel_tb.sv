`timescale 1ns/1ps

module adder_toplevel_tb;
    // Clock and stimulus
    logic clk;
    logic reset;
    logic run_i;
    logic [15:0] sw_i;

    // DUT outputs (unused in checks, but connected)
    logic sign_led;
    logic [7:0]  hex_seg_a;
    logic [3:0]  hex_grid_a;
    logic [7:0]  hex_seg_b;
    logic [3:0]  hex_grid_b;

    // Instantiate DUT
    adder_toplevel dut (
        .clk        (clk),
        .reset      (reset),
        .run_i      (run_i),
        .sw_i       (sw_i),
        .sign_led   (sign_led),
        .hex_seg_a  (hex_seg_a),
        .hex_grid_a (hex_grid_a),
        .hex_seg_b  (hex_seg_b),
        .hex_grid_b (hex_grid_b)
    );

    // Waveform-friendly aliases for internal values
    // RegA = synchronized switches; RegB = accumulator lower 16 bits
    logic [15:0] regA;      // A input to adder
    logic [15:0] regB;      // B input to adder
    logic [15:0] sum;       // Sum output of adder
    logic        carry;     // Carry out of adder
    assign regA  = dut.sw_s;
    assign regB  = dut.out[15:0];
    assign {carry, sum} = dut.s; // s[16]=carry, s[15:0]=sum

    // Expose debounced internal controls for waveform and stable waits
    logic reset_s_mon, run_s_mon;
    assign reset_s_mon = dut.reset_s;
    assign run_s_mon   = dut.run_s;

    // 100 MHz clock
    initial clk = 1'b0;
    always #5 clk = ~clk; // 10ns period

    // Utility: wait N rising edges
    task automatic wait_cycles(input int n);
        repeat (n) @(posedge clk);
    endtask

    // Emulate a debounced button "press" that generates a 1->0 negedge on run_s
    task automatic press_run_once();
        // Ensure stable high first so negedge is meaningful
        run_i = 1'b1; wait_cycles(4);
        // Drive low and hold long enough for sync_debounce (COUNTER_WIDTH=1 in sim)
        run_i = 1'b0; wait_cycles(6);
        // Return high before next operation
        run_i = 1'b1; wait_cycles(4);
    endtask

    // Self-check helper
    task automatic expect_accum(input logic [16:0] exp, input string msg);
        // "out" is the 17-bit accumulator inside DUT
        if (dut.out !== exp) begin
            $error("%s FAILED: got out=%h, expected %h", msg, dut.out, exp);
            $fatal;
        end else begin
            $display("%s PASS: out=%h", msg, dut.out);
        end
    endtask

    // Pretty-print internal state, including RegA/RegB and adder inputs
    task automatic show_state(input string tag);
        $display("[%s] RegA(sw_s)=%h  RegB(out[15:0])=%h  adder.a=%h  adder.b=%h  S=%h  Cout=%b  sign=%b",
                 tag, dut.sw_s, dut.out[15:0], dut.adder_la.a, dut.adder_la.b, dut.s[15:0], dut.s[16], sign_led);
    endtask

    initial begin
        $display("Starting adder_toplevel_tb");
        // Optional VCD waveform dump (view in GTKWave or compatible)
        // XSIM users can instead use the provided TCL: design_source/tb/adder_toplevel_wave.tcl
        $dumpfile("adder_toplevel_tb.vcd");
        $dumpvars(0, adder_toplevel_tb);
        // Init: hold inputs stable long enough for synchronizers
        run_i = 1'b1; // idle high so a 1->0 edge acts as a press
        sw_i  = 16'h0000;
        reset = 1'b1;
        // Wait until debounced reset goes high (synchronous reset active)
        wait_cycles(1);
        while ($isunknown(reset_s_mon)) wait_cycles(1);
        while (reset_s_mon !== 1'b1)     wait_cycles(1);
        // Keep asserted a few more cycles
        wait_cycles(4);
        // Deassert reset and wait until debounced reset is low
        reset = 1'b0;
        wait_cycles(1);
        while ($isunknown(reset_s_mon)) wait_cycles(1);
        while (reset_s_mon !== 1'b0)     wait_cycles(1);
        // Also ensure run_s has debounced to 1
        while ($isunknown(run_s_mon))    wait_cycles(1);
        while (run_s_mon   !== 1'b1)     wait_cycles(1);
        // Give the accumulator a cycle post-reset
        wait_cycles(2);

        // 1) Add 0x0003 -> expect accumulator 0x00003
        sw_i = 16'h0003; wait_cycles(2);
        press_run_once();
        // Allow one extra cycle for load to capture
        wait_cycles(2);
        expect_accum(17'h00003, "Add 0x0003");
        show_state("After +0x0003");

        // 2) Add 0x0004 -> expect accumulator 0x00007
        sw_i = 16'h0004; wait_cycles(2);
        press_run_once();
        wait_cycles(2);
        expect_accum(17'h00007, "Add 0x0004");
        show_state("After +0x0004");
        
        // 3) Add 0x0001 -> expect accumulator 0x00008
        sw_i = 16'h0001; wait_cycles(2);
        press_run_once();
        wait_cycles(2);
        expect_accum(17'h00008, "Add 0x0001");
        show_state("After +0x0001");
    
        $display("All tests passed.");
        $finish;
    end
endmodule
