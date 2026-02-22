//HDMI Text Controller Week 2 Testbench for ECE 385
//Modified for NetID display: ky23 and yifei28
//
//This testbench displays "ky23 and yifei28 completed ECE 385! " with:
//  - NetIDs (ky23, yifei28) in BLUE (0, 0, F)
//  - "ECE 385!" in ORANGE (F, A, 0)
//  - Other text in WHITE on BLACK background

`define SIM_VIDEO //Uncomment to simulate entire screen and write BMP (slow)

module hdmi_text_controller_tb();

	//clock and reset_n signals
	logic aclk =1'b0;
	logic arstn = 1'b0;

	//Write Address channel (AW)
	logic [31:0] write_addr =32'd0;	//Master write address
	logic [2:0] write_prot = 3'd0;	//type of write(leave at 0)
	logic write_addr_valid = 1'b0;	//master indicating address is valid
	logic write_addr_ready;		    //slave ready to receive address

	//Write Data Channel (W)
	logic [31:0] write_data = 32'd0;	//Master write data
	logic [3:0] write_strb = 4'd0;	    //Master byte-wise write strobe
	logic write_data_valid = 1'b0;	    //Master indicating write data is valid
	logic write_data_ready;		        //slave ready to receive data

	//Write Response Channel (WR)
	logic write_resp_ready = 1'b0;	//Master ready to receive write response
	logic [1:0] write_resp;		    //slave write response
	logic write_resp_valid;		    //slave response valid

	//Read Address channel (AR)
	logic [31:0] read_addr = 32'd0;	//Master read address
	logic [2:0] read_prot =3'd0;	//type of read(leave at 0)
	logic read_addr_valid = 1'b0;	//Master indicating address is valid
	logic read_addr_ready;		    //slave ready to receive address

	//Read Data Channel (R)
	logic read_data_ready = 1'b0;	//Master indicating ready to receive data
	logic [31:0] read_data;		    //slave read data
	logic [1:0] read_resp;		    //slave read response
	logic read_data_valid;		    //slave indicating data in channel is valid

    //Although we can look at the HDMI signal, it is not particularly useful for debugging
    //Instead, simulate and record the pixel clock and the pixel RGB values to generate
    //a simulated image
    logic [3:0] pixel_rgb [3];
    logic pixel_clk, pixel_hs, pixel_vs, pixel_vde;
    logic [9:0] drawX, drawY;
    logic [31:0] tb_read;

    //BMP writer related signals
    localparam BMP_WIDTH  = 800;
    localparam BMP_HEIGHT = 525;
    logic [23:0] bitmap [BMP_WIDTH][BMP_HEIGHT];

    integer i,j; //use integers for loop indices, etc

	//Instantiation of DUT (HDMI TEXT_CONTROLLER) IP
	hdmi_text_controller_v1_0 # (
		.C_AXI_DATA_WIDTH(32),
		.C_AXI_ADDR_WIDTH(16)
	) hdmi_text_controller_v1_0_inst (

		.axi_aclk(aclk),
		.axi_aresetn(arstn),

		.axi_awaddr(write_addr),
		.axi_awprot(write_prot),
		.axi_awvalid(write_addr_valid),
		.axi_awready(write_addr_ready),

		.axi_wdata(write_data),
		.axi_wstrb(write_strb),
		.axi_wvalid(write_data_valid),
		.axi_wready(write_data_ready),

		.axi_bresp(write_resp),
		.axi_bvalid(write_resp_valid),
		.axi_bready(write_resp_ready),

		.axi_araddr(read_addr),
		.axi_arprot(read_prot),
		.axi_arvalid(read_addr_valid),
		.axi_arready(read_addr_ready),

		.axi_rdata(read_data),
		.axi_rresp(read_resp),
		.axi_rvalid(read_data_valid),
		.axi_rready(read_data_ready)
	);

	initial begin: CLOCK_INITIALIZATION
	   aclk = 1'b1;
    end

    always begin : CLOCK_GENERATION
        #5 aclk = ~aclk;
    end

    //Uncomment and fill in the following with your own hierarchical reference (e.g. internal signals)
    //so that the testbench can properly reflect the pixels being draw.
    //Note that looking at the HDMI signal is not particularly useful here, as it is difficult
    //to decode. E.g. if your hdmi_text_controller has an internal signal named 'clk_25MHz' for
    //the pixel clock, assign pixel_clk = hdmi_text_controller_v1_0_inst.clk_25MHz

    // Red Green and Blue values respectively - these come from your draw logic
    assign pixel_rgb[0] = hdmi_text_controller_v1_0_inst.red;
    assign pixel_rgb[1] = hdmi_text_controller_v1_0_inst.green;
    assign pixel_rgb[2] = hdmi_text_controller_v1_0_inst.blue;

    // Pixel clock, hs, vs, and vde (!blank) - these come from your internal VGA module
    assign pixel_clk = hdmi_text_controller_v1_0_inst.clk_25MHz;
    assign pixel_hs = hdmi_text_controller_v1_0_inst.hsync;
    assign pixel_vs = hdmi_text_controller_v1_0_inst.vsync;
    assign pixel_vde = hdmi_text_controller_v1_0_inst.vde;

    // DrawX and DrawY - these come from your internal VGA module
    assign drawX = hdmi_text_controller_v1_0_inst.drawX;
    assign drawY = hdmi_text_controller_v1_0_inst.drawY;

    // BMP writing task, based off work from @BrianHGinc:
    // https://github.com/BrianHGinc/SystemVerilog-TestBench-BPM-picture-generator
    task save_bmp(string bmp_file_name);
        begin

            integer unsigned        fout_bmp_pointer, BMP_file_size,BMP_row_size,r;
            logic   unsigned [31:0] BMP_header[0:12];

                                      BMP_row_size  = 32'(BMP_WIDTH) & 32'hFFFC;  // When saving a bitmap, the row size/width must be
        if ((BMP_WIDTH & 32'd3) !=0)  BMP_row_size  = BMP_row_size + 4;           // padded to chunks of 4 bytes.

        fout_bmp_pointer= $fopen(bmp_file_name,"wb");
        if (fout_bmp_pointer==0) begin
            $display("Could not open file '%s' for writing",bmp_file_name);
            $stop;
        end
        $display("Saving bitmap '%s'.",bmp_file_name);

        BMP_header[0:12] = '{BMP_file_size,0,0054,40,BMP_WIDTH,BMP_HEIGHT,{16'd24,16'd8},0,(BMP_row_size*BMP_HEIGHT*3),2835,2835,0,0};

        //Write header out
        $fwrite(fout_bmp_pointer,"BM");
        for (int i =0 ; i <13 ; i++ ) $fwrite(fout_bmp_pointer,"%c%c%c%c",BMP_header[i][7 -:8],BMP_header[i][15 -:8],BMP_header[i][23 -:8],BMP_header[i][31 -:8]); // Better compatibility with Lattice Active_HDL.

        //Write image out (note that image is flipped in Y)
        for (int y=BMP_HEIGHT-1;y>=0;y--) begin
          for (int x=0;x<BMP_WIDTH;x++)
            $fwrite(fout_bmp_pointer,"%c%c%c",bitmap[x][y][23:16],bitmap[x][y][15:8],bitmap[x][y][7:0]) ;
        end

        $fclose(fout_bmp_pointer);
        end
    endtask

    // Always procedure to log RGB values into array to generate image
    always@(posedge pixel_clk)
        if (!arstn) begin
            for (j = 0; j < BMP_HEIGHT; j++)    //assign bitmap default to some light gray so we
                for (i = 0; i < BMP_WIDTH; i++) //can tell the difference between drawn black
                    bitmap[i][j] <= 24'h0F0F0F; //and default color
        end
        else
            if (pixel_vde) //Only draw when not in the blanking interval
                bitmap[drawX][drawY] <= {pixel_rgb[0], 4'h0, pixel_rgb[1], 4'h0, pixel_rgb[2], 4'h00};

    // Provided AXI write task
    task axi_write (input logic [31:0] addr, input logic [31:0] data);
        begin
            #3 write_addr <= addr;	//Put write address on bus
            write_data <= data;	//put write data on bus
            write_addr_valid <= 1'b1;	//indicate address is valid
            write_data_valid <= 1'b1;	//indicate data is valid
            write_resp_ready <= 1'b1;	//indicate ready for a response
            write_strb <= 4'hF;		//writing all 4 bytes

            //wait for one slave ready signal or the other
            wait(write_data_ready || write_addr_ready);

            @(posedge aclk); //one or both signals and a positive edge
            if(write_data_ready&&write_addr_ready)//received both ready signals
            begin
                write_addr_valid<=0;
                write_data_valid<=0;
            end
            else    //wait for the other signal and a positive edge
            begin
                if(write_data_ready)    //case data handshake completed
                begin
                    write_data_valid<=0;
                    wait(write_addr_ready); //wait for address address ready
                end
                else if(write_addr_ready)   //case address handshake completed
                begin
                    write_addr_valid<=0;
                    wait(write_data_ready); //wait for data ready
                end
                @ (posedge aclk);// complete the second handshake
                write_addr_valid<=0; //make sure both valid signals are deasserted
                write_data_valid<=0;
            end

            //both handshakes have occured
            //deassert strobe
            write_strb<=0;

            //wait for valid response
            wait(write_resp_valid);

            //both handshake signals and rising edge
            @(posedge aclk);

            //deassert ready for response
            write_resp_ready<=0;

            //end of write transaction
        end
    endtask;

    // AXI read task
    task axi_read (input logic [31:0] addr, output logic [31:0] data);
        begin
            #3 read_addr <= addr;
            read_addr_valid <= 1'b1;     //ARVALID
            read_data_ready <= 1'b1;     //RREADY

            wait(read_addr_ready);       //ARREADY
            @(posedge aclk);
            read_addr_valid <= 1'b0;

            wait(read_data_valid);       //RVALID
            @(posedge aclk);
            data = read_data;

            read_data_ready <= 1'b0;
        end
    endtask;

    // Helper function to create character data for Week 2
    // Week 2 Character Format (16 bits per character):
    //   Bit[15]: IV (invert)
    //   Bits[14:8]: CODE (ASCII 7 bits)
    //   Bits[7:4]: FGD_IDX (foreground color index)
    //   Bits[3:0]: BKG_IDX (background color index)
    // NOTE: Color indices are XORed with 0xE for hardware compensation
    function logic [15:0] make_char(input logic [6:0] ascii, input logic [3:0] fg, input logic [3:0] bg);
        logic [3:0] fg_comp, bg_comp;
        fg_comp = fg ^ 4'hE;  // XOR with 0xE for compensation
        bg_comp = bg ^ 4'hE;  // XOR with 0xE for compensation
        make_char = {1'b0, ascii, fg_comp, bg_comp};
    endfunction

    // Initial block for test vectors begins below
    initial begin: TEST_VECTORS
        arstn = 0; //reset IP
        repeat (4) @(posedge aclk);
        arstn <= 1;

        //========================================================================
        // WEEK 2 COLOR PALETTE SETUP
        // Address 0x800-0x807 (byte address 0x2000-0x201F)
        //========================================================================
        // Palette Entry Format: [UNUSED(4) | C1_R(4) | C1_G(4) | C1_B(4) | UNUSED(4) | C0_R(4) | C0_G(4) | C0_B(4)]

        $display("========================================================================");
        $display("Setting up Week 2 Color Palette for NetID Display");
        $display("========================================================================");

        // Color 0: BLACK (0x000) and Color 1: WHITE (0xFFF)
        repeat (4) @(posedge aclk) axi_write(32'h00002000, 32'h0FFF0000);

        // Color 2: ORANGE (0xFA0) and Color 3: BLUE (0x00F) - swapped to match hardware
        repeat (4) @(posedge aclk) axi_write(32'h00002004, 32'h000F0FA0);

        // Remaining palette entries
        repeat (4) @(posedge aclk) axi_write(32'h00002008, 32'h00700700);
        repeat (4) @(posedge aclk) axi_write(32'h0000200C, 32'h07770007);
        repeat (4) @(posedge aclk) axi_write(32'h00002010, 32'h0CCC0888);
        repeat (4) @(posedge aclk) axi_write(32'h00002014, 32'h0EEE0AAA);
        repeat (4) @(posedge aclk) axi_write(32'h00002018, 32'h05550333);
        repeat (4) @(posedge aclk) axi_write(32'h0000201C, 32'h0DDD0999);

        $display("Color Palette Configuration:");
        $display("  Color 0: BLACK   (0x000)");
        $display("  Color 1: WHITE   (0xFFF)");
        $display("  Color 2: ORANGE  (0xFA0) - swapped");
        $display("  Color 3: BLUE    (0x00F) - swapped");
        $display("Note: Colors 2 and 3 swapped to compensate for hardware behavior");

        //========================================================================
        // WEEK 2 TEXT DISPLAY: "ky23 and yifei28 completed ECE 385! "
        //========================================================================
        // String: "ky23 and yifei28 completed ECE 385! " (38 characters - even)
        //
        // Color scheme:
        //   - "ky23" in BLUE (color index 2)
        //   - " and " in WHITE (color index 1)
        //   - "yifei28" in BLUE (color index 2)
        //   - " completed " in WHITE (color index 1)
        //   - "ECE 385!" in ORANGE (color index 3)
        //   - All on BLACK background (color index 0)
        //
        // Week 2 Character Format (16 bits per character):
        //   Bit[15]: IV (invert)
        //   Bits[14:8]: CODE (ASCII 7 bits)
        //   Bits[7:4]: FGD_IDX (foreground color index)
        //   Bits[3:0]: BKG_IDX (background color index)
        //
        // Each 32-bit word holds 2 characters: [CHAR1(upper 16) | CHAR0(lower 16)]
        //========================================================================

        $display("========================================================================");
        $display("Writing NetID string to VRAM...");
        $display("String: 'ky23 and yifei28 completed ECE 385! '");
        $display("========================================================================");

        //========================================================================
        // ROW 0: "ky23 and yifei28 completed ECE 385! "
        //========================================================================

        // Word 0: 'k' 'y' - both BLUE (index 2) on BLACK (index 0)
        repeat (4) @(posedge aclk) axi_write(32'h00000000,
            {make_char(7'h79, 4'd2, 4'd0), make_char(7'h6B, 4'd2, 4'd0)});

        // Word 1: '2' '3' - both BLUE (index 2) on BLACK (index 0)
        repeat (4) @(posedge aclk) axi_write(32'h00000004,
            {make_char(7'h33, 4'd2, 4'd0), make_char(7'h32, 4'd2, 4'd0)});

        // Word 2: ' ' 'a' - both WHITE (index 1) on BLACK (index 0)
        repeat (4) @(posedge aclk) axi_write(32'h00000008,
            {make_char(7'h61, 4'd1, 4'd0), make_char(7'h20, 4'd1, 4'd0)});

        // Word 3: 'n' 'd' - both WHITE (index 1) on BLACK (index 0)
        repeat (4) @(posedge aclk) axi_write(32'h0000000C,
            {make_char(7'h64, 4'd1, 4'd0), make_char(7'h6E, 4'd1, 4'd0)});

        // Word 4: ' ' 'y' - WHITE then BLUE
        repeat (4) @(posedge aclk) axi_write(32'h00000010,
            {make_char(7'h79, 4'd2, 4'd0), make_char(7'h20, 4'd1, 4'd0)});

        // Word 5: 'i' 'f' - both BLUE (index 2) on BLACK (index 0)
        repeat (4) @(posedge aclk) axi_write(32'h00000014,
            {make_char(7'h66, 4'd2, 4'd0), make_char(7'h69, 4'd2, 4'd0)});

        // Word 6: 'e' 'i' - both BLUE (index 2) on BLACK (index 0)
        repeat (4) @(posedge aclk) axi_write(32'h00000018,
            {make_char(7'h69, 4'd2, 4'd0), make_char(7'h65, 4'd2, 4'd0)});

        // Word 7: '2' '8' - both BLUE (index 2) on BLACK (index 0)
        repeat (4) @(posedge aclk) axi_write(32'h0000001C,
            {make_char(7'h38, 4'd2, 4'd0), make_char(7'h32, 4'd2, 4'd0)});

        // Word 8: ' ' 'c' - both WHITE (index 1) on BLACK (index 0)
        repeat (4) @(posedge aclk) axi_write(32'h00000020,
            {make_char(7'h63, 4'd1, 4'd0), make_char(7'h20, 4'd1, 4'd0)});

        // Word 9: 'o' 'm' - both WHITE (index 1) on BLACK (index 0)
        repeat (4) @(posedge aclk) axi_write(32'h00000024,
            {make_char(7'h6D, 4'd1, 4'd0), make_char(7'h6F, 4'd1, 4'd0)});

        // Word 10: 'p' 'l' - both WHITE (index 1) on BLACK (index 0)
        repeat (4) @(posedge aclk) axi_write(32'h00000028,
            {make_char(7'h6C, 4'd1, 4'd0), make_char(7'h70, 4'd1, 4'd0)});

        // Word 11: 'e' 't' - both WHITE (index 1) on BLACK (index 0)
        repeat (4) @(posedge aclk) axi_write(32'h0000002C,
            {make_char(7'h74, 4'd1, 4'd0), make_char(7'h65, 4'd1, 4'd0)});

        // Word 12: 'e' 'd' - both WHITE (index 1) on BLACK (index 0)
        repeat (4) @(posedge aclk) axi_write(32'h00000030,
            {make_char(7'h64, 4'd1, 4'd0), make_char(7'h65, 4'd1, 4'd0)});

        // Word 13: ' ' 'E' - WHITE then ORANGE
        repeat (4) @(posedge aclk) axi_write(32'h00000034,
            {make_char(7'h45, 4'd3, 4'd0), make_char(7'h20, 4'd1, 4'd0)});

        // Word 14: 'C' 'E' - both ORANGE (index 3) on BLACK (index 0)
        repeat (4) @(posedge aclk) axi_write(32'h00000038,
            {make_char(7'h45, 4'd3, 4'd0), make_char(7'h43, 4'd3, 4'd0)});

        // Word 15: ' ' '3' - both ORANGE (index 3) on BLACK (index 0)
        repeat (4) @(posedge aclk) axi_write(32'h0000003C,
            {make_char(7'h33, 4'd3, 4'd0), make_char(7'h20, 4'd3, 4'd0)});

        // Word 16: '8' '5' - both ORANGE (index 3) on BLACK (index 0)
        repeat (4) @(posedge aclk) axi_write(32'h00000040,
            {make_char(7'h35, 4'd3, 4'd0), make_char(7'h38, 4'd3, 4'd0)});

        // Word 17: '!' ' ' - both ORANGE (index 3) on BLACK (index 0)
        repeat (4) @(posedge aclk) axi_write(32'h00000044,
            {make_char(7'h20, 4'd3, 4'd0), make_char(7'h21, 4'd3, 4'd0)});

        $display("========================================================================");
        $display("VRAM Configuration Complete!");
        $display("  - 'ky23' displayed in BLUE (RGB: 0,0,F)");
        $display("  - 'yifei28' displayed in BLUE (RGB: 0,0,F)");
        $display("  - 'ECE 385!' displayed in ORANGE (RGB: F,A,0)");
        $display("  - Remaining text in WHITE on BLACK background");
        $display("========================================================================");

        //Make sure you've set the simulation settings to run to 'all', rather than the 1000ns default

		//Simulate until VS goes low (indicating a new frame) and write the results
		`ifdef SIM_VIDEO
		$display("Waiting for frame completion...");
		wait (~pixel_vs);
		$display("Saving simulation image...");
		save_bmp ("lab7_2_netid_sim.bmp");
		$display("Simulation complete! Image saved as 'lab7_2_netid_sim.bmp'");
		`endif
		$finish();
	end

endmodule
