`timescale 1ns/1ps

// ------------------------------------------------------------
// Test Program: Auto Counter (Start Address x009C)
// Purpose: Verify continuous increment of HEX display value
// Instructions tested: ADDi, ANDi, JSR, LDR, STR, BR
// ------------------------------------------------------------

module tb_auto_count;

  // ------------------------------
  // DUT control and I/O signals
  // ------------------------------
  logic clk = 0;
  logic reset = 1;
  logic run_i = 0;
  logic continue_i = 0;
  logic [15:0] sw_i = 16'h009C;   // start vector â†? auto count program

  // HEX and LED outputs
  logic [15:0] led_o;
  logic [7:0]  hex_seg_o, hex_seg_debug;
  logic [3:0]  hex_grid_o, hex_grid_debug;

  // SRAM interface
  logic [15:0] sram_rdata, sram_wdata, sram_addr;
  logic        sram_mem_ena, sram_wr_ena;

  // ------------------------------
  // Instantiate DUT (your top-level SLC3)
  // ------------------------------
  slc3 dut (
    .clk(clk),
    .reset(reset),
    .run_i(run_i),
    .continue_i(continue_i),
    .sw_i(sw_i),
    .led_o(led_o),
    .hex_seg_o(hex_seg_o),
    .hex_grid_o(hex_grid_o),
    .hex_seg_debug(hex_seg_debug),
    .hex_grid_debug(hex_grid_debug),
    .sram_rdata(sram_rdata),
    .sram_wdata(sram_wdata),
    .sram_addr(sram_addr),
    .sram_mem_ena(sram_mem_ena),
    .sram_wr_ena(sram_wr_ena)
  );

  // ------------------------------
  // Instantiate memory model
  // ------------------------------
 memory mem_subsystem (
	.clk		(clk),
	.reset		(reset),

	.data		(sram_wdata),
	.address	(sram_addr[9:0]),
	.ena		(sram_mem_ena),
	.wren		(sram_wr_ena),
	.readout	(sram_rdata)
);

  // ------------------------------
  // Convenience probes
  // ------------------------------
 // logic [15:0] ir_value;          // shows current IR (right HEX)
  logic [15:0] hex_display;     // shows what's written to HEX display (left HEX)

 // assign ir_value = dut.hex_display_debug;              // IR contents on right hex
  assign hex_display = dut.io_bridge.hex_display;     // memory-mapped display value (left hex)

  // 100 MHz clock
  always #5 clk = ~clk;

  // Debug: Monitor writes to hex display (address 0xFFFF)
  always @(posedge clk) begin
    if (!reset && sram_mem_ena && sram_wr_ena && sram_addr == 16'hFFFF) begin
      $display("T=%0t: [HEX WRITE] CPU writing %h to hex display (addr=0xFFFF)", $time, sram_wdata);
    end
  end

  // Debug: Monitor display_value changes
  logic [15:0] display_value_prev = 0;
  always @(posedge clk) begin
    if (hex_display !== display_value_prev) begin
    
      display_value_prev <= hex_display;
    end
  end

  // Helper tasks
  task press_run();       begin run_i <= 1; @(posedge clk); run_i <= 0; end endtask
  task wait_cycles(int N); begin repeat (N) @(posedge clk); end endtask

  // ------------------------------
  // Main test sequence
  // ------------------------------
  initial begin
    $display("===== Auto Count Testbench Start =====");
    // $dumpfile("tb_auto_count.vcd");
    // $dumpvars(0, tb_auto_count);

    // Initial reset
    wait_cycles(4);
    reset <= 0;

    // Press Run to begin bootloader (0 â†? 2 â†? jump to 009C)
    press_run();

    // Monitor execution progress
    repeat (20) begin
      wait_cycles(1000);
     
    end

    // Let it run long enough to show multiple increments
    wait_cycles(180000);

   
  end

endmodule
