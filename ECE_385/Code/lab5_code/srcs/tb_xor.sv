`timescale 1ns/1ps

// ------------------------------------------------------------
// Test Program: XOR (Start Address x0014)
// Purpose: Verify XOR operation using AND/NOT instructions
// Instructions tested: AND, ANDi, NOT, LDR, BR, PSE
// This testbench tests ONE example with clear waveform markers
// ------------------------------------------------------------

module tb_xor;

  // ------------------------------
  // DUT control and I/O signals
  // ------------------------------
  logic clk = 0;
  logic reset = 1;
  logic run_i = 0;
  logic continue_i = 0;
  logic [15:0] sw_i = 16'h0014;   // start vector -> XOR test program

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
      wait_cycles(10);  // Let it settle
      $display("T=%0t: [PAUSE STATE] Checkpoint LED=%h, IR(right_hex)=%h",
               $time, led_o[11:0], ir_value);
      $display("                   IR opcode=%b (1101=PAUSE)", ir_value[15:12]);
    end
  endtask

  task test_xor(input [15:0] a, b, expected);
    begin
      $display("\n========================================");
      $display("=== XOR Test: %h XOR %h ===", a, b);
      $display("=== Expected Result: %h           ===", expected);
      $display("========================================\n");

      // Checkpoint 1: Set first input
      $display(">>> PHASE 1: Waiting for first input prompt...");
      wait_for_pause();
      wait_cycles(50);  // Extra time to see pause clearly

      sw_i = a;
      $display(">>> Setting INPUT A = %h on switches", a);
      wait_cycles(100);  // Hold input A stable on waveform

      press_continue();
      $display(">>> Continue pressed, CPU reading input A");
      wait_cycles(50);

      // Checkpoint 2: Set second input
      $display("\n>>> PHASE 2: Waiting for second input prompt...");
      wait_for_pause();
      wait_cycles(50);  // Extra time to see pause clearly

      sw_i = b;
      $display(">>> Setting INPUT B = %h on switches", b);
      wait_cycles(100);  // Hold input B stable on waveform

      press_continue();
      $display(">>> Continue pressed, CPU reading input B");

      // Wait for XOR computation
      $display("\n>>> PHASE 3: CPU computing XOR...");
      wait_cycles(200);

      // Checkpoint 5: Read result
      $display("\n>>> PHASE 4: Waiting for result display...");
      wait_for_pause();
      wait_cycles(50);

      $display("\n========================================");
      $display(">>> OUTPUT (left hex)  = %h", display_value);
      $display(">>> IR (right hex)     = %h (opcode=%b)", ir_value, ir_value[15:12]);
      $display(">>> Expected result    = %h", expected);

      if (display_value === expected) begin
        $display(">>> *** PASS *** Test succeeded!");
      end else begin
        $display(">>> *** FAIL *** Got %h, expected %h", display_value, expected);
      end
      $display("========================================\n");

      wait_cycles(100);  // Hold result stable on waveform
    end
  endtask

  // ------------------------------
  // Main test sequence
  // ------------------------------
  initial begin
    $display("===== XOR Test Testbench Start =====");
    $display("Testing ONE XOR example with distinct, complex values for clear waveform");
    $display("");

    // Initial reset
    wait_cycles(4);
    reset <= 0;
    wait_cycles(20);

    // Press Run to begin bootloader (jump to 0x0014)
    $display(">>> Pressing RUN to start program at address 0x0014");
    press_run();
    wait_cycles(100);

    // Test ONE example with complicated, distinct numbers for clear waveform
    // A5C3 XOR 3C96 = 9955
    // Binary verification:
    // A5C3: 1010 0101 1100 0011
    // 3C96: 0011 1100 1001 0110
    // XOR:  1001 1001 0101 0101 = 0x9955
    test_xor(16'hA5C3, 16'h3C96, 16'h9955);

    $display("\n===== XOR Test Complete =====");
    $display("\nWaveform Checkpoints:");
    $display("  - sw_i = A5C3 during INPUT A phase");
    $display("  - sw_i = 3C96 during INPUT B phase");
    $display("  - display_value = 9955 during OUTPUT phase (left hex)");
    $display("  - ir_value shows PAUSE instruction (opcode 1101) at each checkpoint (right hex)");
    $display("  - led_o[11:0] shows checkpoint numbers (801, 802, 405)");
    wait_cycles(100);
    $finish;
  end

endmodule
