`timescale 1ns/1ps

// Week 1 Fetch Sequence Testbench
// - Verifies: PC -> MAR and PC increment (S_18)
// - Verifies: MDR loads from memory (S_33_1..S_33_3)
// - Verifies: IR loads from MDR (S_35)

// No external includes needed; use provided test_memory for contents

module cpu_tb;
    // Clock/Reset
    logic clk = 0;
    logic reset = 1;

    // Control inputs
    logic run_i = 0;
    logic continue_i = 0;

    // CPU <-> Memory interface
    logic [15:0] mem_rdata;
    logic [15:0] mem_wdata;
    logic [15:0] mem_addr;
    logic        mem_mem_ena;
    logic        mem_wr_ena;

    // Unused outputs
    logic [15:0] hex_display_debug;
    logic [15:0] led_o;

    // Instantiate the CPU directly so we can observe internal regs
    cpu uut (
        .clk            (clk),
        .reset          (reset),
        .run_i          (run_i),
        .continue_i     (continue_i),
        .hex_display_debug(hex_display_debug),
        .led_o          (led_o),
        .mem_rdata      (mem_rdata),
        .mem_wdata      (mem_wdata),
        .mem_addr       (mem_addr),
        .mem_mem_ena    (mem_mem_ena),
        .mem_wr_ena     (mem_wr_ena)
    );

    // Use the provided test_memory model so contents match the lab
    test_memory mem_model (
        .clk     (clk),
        .reset   (reset),
        .data    (mem_wdata),
        .address (mem_addr[9:0]),
        .ena     (mem_mem_ena),
        .wren    (mem_wr_ena),
        .readout (mem_rdata)
    );

    // Clock generation: 100 MHz (10 ns period)
    always #5 clk = ~clk;

    // Expose internal registers on waveform for convenience
    // (hierarchical references to internal signals in cpu.sv)
    wire [15:0] pc_tb  = uut.pc;
    wire [15:0] mar_tb = uut.mar;
    wire [15:0] mdr_tb = uut.mdr;
    wire [15:0] ir_tb  = uut.ir;

    initial begin
        // Optional VCD for generic simulators; XSim will produce WDB
        $dumpfile("cpu_fetch.vcd");
        $dumpvars(0, cpu_tb);

        // Hold reset for a few cycles
        repeat (4) @(posedge clk);
        reset <= 0;

        // Press Run once (pulse). Machine will fetch and pause at IR.
        run_i <= 1'b1; @(posedge clk); run_i <= 1'b0;

        // Perform 5 consecutive fetch cycles, pressing Continue between each
        for (int f = 0; f < 5; f++) begin
            // Allow fetch to progress through s_18, s_33_1..3, s_35 and pause
            repeat (12) @(posedge clk);
            $display("Fetch %0d complete | PC=%h MAR=%h MDR=%h IR=%h", f, pc_tb, mar_tb, mdr_tb, ir_tb);

            // Advance to the next fetch by pressing Continue (except after last)
            if (f < 4) begin
                continue_i <= 1'b1; @(posedge clk);
                continue_i <= 1'b0; @(posedge clk);
            end
        end

        $finish;
    end

    // Console monitor (helps confirm values alongside waveform)
    initial begin
        $display(" time |   PC    MAR    MDR    IR");
        $monitor("%5t | %h %h %h %h", $time, pc_tb, mar_tb, mdr_tb, ir_tb);
    end

endmodule
