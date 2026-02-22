`timescale 1ns/1ps

// ------------------------------------------------------------
// Test Program: Bubble Sort (Start Address x005A)
// Purpose: Verify sorting algorithm and all SLC-3 instructions
// Instructions tested: ALL (complete ISA test)
// This testbench: Display unsorted → Sort → Display sorted
// Long breaks between phases for screenshot capture
// ------------------------------------------------------------

module tb_sort;

  // ------------------------------
  // DUT control and I/O signals
  // ------------------------------
  logic clk = 0;
  logic reset = 1;
  logic run_i = 0;
  logic continue_i = 0;
  logic [15:0] sw_i = 16'h005A;   // start vector -> Sort test program

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
    end
  endtask

  task wait_for_menu();
    begin
      // Wait for menu pause (checkpoint -1 = 0x3FF on LEDs)
      wait (led_o[11:0] == 12'h3FF);
      wait_cycles(10);
      $display("T=%0t: [MENU] At menu, LED=3FF, IR=%h", $time, ir_value);
    end
  endtask

  task select_menu_option(input [15:0] option, input string option_name);
    begin
      $display("\n>>> Selecting menu option: %s (entering %h on switches)", option_name, option);
      wait_for_menu();
      wait_cycles(50);  // Pause for screenshot

      sw_i = option;
      $display(">>> Switches set to %h", option);
      wait_cycles(100);  // Hold for waveform visibility

      press_continue();
      $display(">>> Continue pressed, executing %s", option_name);
      wait_cycles(50);
    end
  endtask

  task display_array(input string phase_name);
    integer i;
    begin
      $display("\n========================================");
      $display("=== PHASE: %s ===", phase_name);
      $display("========================================");
      $display("Displaying all 16 array values...\n");

      for (i = 0; i < 16; i++) begin
        // Wait for pause with current array value
        wait_for_pause();

        $display(">>> Index %2d: Value = %h (LED shows index=%h)",
                 i, display_value, led_o[11:4]);
        $display("    IR (right hex) = %h", ir_value);

        wait_cycles(200);  // LONG pause for screenshot

        press_continue();
        $display("    Continue pressed, moving to next value\n");
        wait_cycles(50);
      end

      $display("=== Array display complete ===\n");
      wait_cycles(100);
    end
  endtask

  // ------------------------------
  // Main test sequence
  // ------------------------------
  initial begin
    $display("========================================");
    $display("===== Bubble Sort Test Start =====");
    $display("========================================");
    $display("Test sequence:");
    $display("  1. Display UNSORTED array (16 values)");
    $display("  2. Run SORT algorithm");
    $display("  3. Display SORTED array (16 values)");
    $display("");
    $display("Expected unsorted values (from types.sv):");
    $display("  Index  0: 00EF    Index  8: 001F");
    $display("  Index  1: 001B    Index  9: 000D");
    $display("  Index  2: 0001    Index 10: 00B8");
    $display("  Index  3: 008C    Index 11: 0003");
    $display("  Index  4: 00DB    Index 12: 006B");
    $display("  Index  5: 00FA    Index 13: 004E");
    $display("  Index  6: 0047    Index 14: 00F8");
    $display("  Index  7: 0046    Index 15: 0007");
    $display("");
    $display("Expected sorted values (ascending order):");
    $display("  0001, 0003, 0007, 000D, 001B, 001F, 0046, 0047,");
    $display("  004E, 006B, 008C, 00B8, 00DB, 00EF, 00F8, 00FA");
    $display("========================================\n");

    // Initial reset
    wait_cycles(4);
    reset <= 0;
    wait_cycles(20);

    // Press Run to begin bootloader (jump to 0x005A)
    $display(">>> Pressing RUN to start program at address 0x005A");
    press_run();
    wait_cycles(100);

    //=========================================================================
    // PHASE 1: DISPLAY UNSORTED ARRAY
    //=========================================================================
    $display("\n\n");
    $display("************************************************************************");
    $display("*** PHASE 1: DISPLAY UNSORTED ARRAY ***");
    $display("************************************************************************");
    wait_cycles(200);  // Long pause before starting

    select_menu_option(16'h0003, "DISPLAY function");
    wait_cycles(100);

    display_array("UNSORTED ARRAY");

    // Long break for user to review/screenshot
    $display("\n>>> LONG PAUSE: Review unsorted array output above");
    $display(">>> (Take screenshots if needed)");
    wait_cycles(500);

    //=========================================================================
    // PHASE 2: SORT THE ARRAY
    //=========================================================================
    $display("\n\n");
    $display("************************************************************************");
    $display("*** PHASE 2: SORTING ARRAY ***");
    $display("************************************************************************");
    wait_cycles(200);  // Long pause before sorting

    select_menu_option(16'h0002, "SORT function");

    $display("\n>>> Bubble sort algorithm executing...");
    $display(">>> (This takes many cycles - no visual feedback during sort)");

    // Wait for sort to complete (it returns to menu when done)
    // Bubble sort with 16 elements takes many iterations
    wait_cycles(5000);  // Give it plenty of time

    $display(">>> Sort should be complete, returning to menu");
    wait_cycles(100);

    // Long break before displaying sorted results
    $display("\n>>> LONG PAUSE: Sort complete, preparing to display sorted array");
    wait_cycles(500);

    //=========================================================================
    // PHASE 3: DISPLAY SORTED ARRAY
    //=========================================================================
    $display("\n\n");
    $display("************************************************************************");
    $display("*** PHASE 3: DISPLAY SORTED ARRAY ***");
    $display("************************************************************************");
    wait_cycles(200);  // Long pause before starting

    select_menu_option(16'h0003, "DISPLAY function");
    wait_cycles(100);

    display_array("SORTED ARRAY");

    // Final summary
    $display("\n\n");
    $display("========================================");
    $display("===== Bubble Sort Test Complete =====");
    $display("========================================");
    $display("\nVerify in waveform:");
    $display("  - display_value shows each array element on left hex");
    $display("  - ir_value shows PAUSE instructions (opcode 1101) on right hex");
    $display("  - led_o[11:4] shows current array index during display");
    $display("  - led_o[11:0] = 3FF at menu");
    $display("  - Array values change from unsorted to sorted order");
    $display("\nExpected sorted order:");
    $display("  0001 < 0003 < 0007 < 000D < 001B < 001F < 0046 < 0047");
    $display("  0047 < 004E < 006B < 008C < 00B8 < 00DB < 00EF < 00F8 < 00FA");

    wait_cycles(500);
    $finish;
  end

endmodule
