`timescale 1ns/1ps

// ------------------------------------------------------------
// Test Program: Multiplication (Start Address x0031)
// Purpose: Verify multiplication using shift-and-add algorithm
// Instructions tested: AND, ANDi, ADD, ADDi, NOT, LDR, STR, BR, JSR, PSE
// Note: Multiplication operates on UNSIGNED values only
// This testbench tests ONE example with clear waveform markers
// ------------------------------------------------------------

module tb_multiply;

  // ------------------------------
  // DUT control and I/O signals
  // ------------------------------
  logic clk = 0;
  logic reset = 1;
  logic run_i = 0;
  logic continue_i = 0;
  logic [15:0] sw_i = 16'h0031;   // start vector → Multiplier program

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
  logic [15:0] display_value;     // shows what's written to HEX display (left hex)
  logic [15:0] ir_value;          // shows IR contents (right hex)

  assign display_value = dut.io_bridge.hex_display;  // memory-mapped display value (left hex)
  assign ir_value = dut.hex_display_debug;           // IR contents displayed on right hex

  // 100 MHz clock
  always #5 clk = ~clk;

  // Helper tasks
  task press_run();
    begin
      run_i <= 1;
      @(posedge clk);
      run_i <= 0;
    end
  endtask

  task press_continue();
    begin
      continue_i <= 1;
      @(posedge clk);
      continue_i <= 0;
    end
  endtask

  task wait_cycles(int N);
    begin
      repeat (N) @(posedge clk);
    end
  endtask

  task wait_for_pause();
    begin
      // Wait until we hit a PAUSE instruction (pause_ir1 or pause_ir2 state)
      wait (led_o[11:0] != 12'h000);  // LED shows pause checkpoint
      wait_cycles(5);  // Let it settle
      $display("T=%0t: [PAUSE STATE] Checkpoint LED=%h, IR(right_hex)=%h",
               $time, led_o[11:0], ir_value);
      $display("                   IR opcode=%b (1101=PAUSE)", ir_value[15:12]);
    end
  endtask

  task test_multiply(input [15:0] a, b, expected);
    begin
      $display("\n========================================");
      $display("=== Multiply Test: %0d * %0d ===", a, b);
      $display("=== Inputs: %h * %h        ===", a, b);
      $display("=== Expected: %0d (0x%h)     ===", expected, expected);
      $display("========================================\n");

      // Checkpoint 1: Set first operand
      $display(">>> PHASE 1: Waiting for first operand prompt...");
      wait_for_pause();
      wait_cycles(50);  // Extra time to see pause clearly

      sw_i = a;
      $display(">>> Setting OPERAND 1 = %h (%0d decimal) on switches", a, a);
      wait_cycles(100);  // Hold operand 1 stable on waveform

      press_continue();
      $display(">>> Continue pressed, CPU reading operand 1");
      wait_cycles(50);

      // Checkpoint 2: Set second operand
      $display("\n>>> PHASE 2: Waiting for second operand prompt...");
      wait_for_pause();
      wait_cycles(50);  // Extra time to see pause clearly

      sw_i = b;
      $display(">>> Setting OPERAND 2 = %h (%0d decimal) on switches", b, b);
      wait_cycles(100);  // Hold operand 2 stable on waveform

      press_continue();
      $display(">>> Continue pressed, CPU reading operand 2");

      // Wait for multiplication (8 iterations through the loop)
      // Each iteration takes multiple cycles
      $display("\n>>> PHASE 3: CPU computing multiplication (shift-and-add algorithm)...");
      wait_cycles(300);

      // Checkpoint 3: Read result
      $display("\n>>> PHASE 4: Waiting for result display...");
      wait_for_pause();
      wait_cycles(50);

      $display("\n========================================");
      $display(">>> OUTPUT (left hex)  = %h (%0d decimal)", display_value, display_value);
      $display(">>> IR (right hex)     = %h (opcode=%b)", ir_value, ir_value[15:12]);
      $display(">>> Expected result    = %h (%0d decimal)", expected, expected);

      if (display_value === expected) begin
        $display(">>> *** PASS *** Test succeeded!");
      end else begin
        $display(">>> *** FAIL *** Got %0d, expected %0d", display_value, expected);
      end
      $display("========================================\n");

      wait_cycles(100);  // Hold result stable on waveform
    end
  endtask

  // ------------------------------
  // Main test sequence
  // ------------------------------
  initial begin
    $display("===== Multiplication Test Testbench Start =====");
    $display("Testing ONE multiplication example with distinct values for clear waveform");
    $display("");

    // Initial reset
    wait_cycles(4);
    reset <= 0;
    wait_cycles(20);

    // Press Run to begin bootloader (jump to 0x0031)
    $display(">>> Pressing RUN to start program at address 0x0031");
    press_run();
    wait_cycles(100);

    // Test ONE example with distinct, clear values for waveform visibility
    // 25 * 31 = 775
    // Hex: 0x0019 * 0x001F = 0x0307
    // All three values are clearly different and easily distinguishable
    test_multiply(16'd25, 16'd31, 16'd775);

    $display("\n===== Multiplication Test Complete =====");
    $display("\nWaveform Checkpoints:");
    $display("  - sw_i = 0019 (25 decimal) during OPERAND 1 phase");
    $display("  - sw_i = 001F (31 decimal) during OPERAND 2 phase");
    $display("  - display_value = 0307 (775 decimal) during OUTPUT phase (left hex)");
    $display("  - ir_value shows PAUSE instruction (opcode 1101) at each checkpoint (right hex)");
    $display("  - led_o[11:0] shows checkpoint numbers (801, 802, 403)");
    wait_cycles(100);
    $finish;
  end

endmodule
