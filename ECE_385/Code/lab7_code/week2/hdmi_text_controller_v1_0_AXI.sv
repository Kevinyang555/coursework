`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// Company: ECE-Illinois
// Engineer: Zuofu Cheng
//
// Create Date: 06/08/2023 12:21:05 PM
// Design Name:
// Module Name: hdmi_text_controller_v1_0_AXI
// Project Name: ECE 385 - hdmi_text_controller
// Target Devices:
// Tool Versions:
// Description:
// This is a modified version of the Vivado template for an AXI4-Lite peripheral,
// rewritten into SystemVerilog for use with ECE 385.
//
// Dependencies:
//
// Revision:
// Revision 0.02 - File modified to be more consistent with generated template
// Revision 11/18 - Made comments less confusing
// Additional Comments:
//
//////////////////////////////////////////////////////////////////////////////////

`timescale 1 ns / 1 ps

module hdmi_text_controller_v1_0_AXI #
(
    parameter integer C_S_AXI_DATA_WIDTH	= 32,
    parameter integer C_S_AXI_ADDR_WIDTH	= 14
)
(
    // BRAM Port A interface
    output logic                                bram_ena,
    output logic [3:0]                          bram_wea,
    output logic [10:0]                         bram_addra,
    output logic [C_S_AXI_DATA_WIDTH-1:0]       bram_dina,
    input  logic [C_S_AXI_DATA_WIDTH-1:0]       bram_douta,

    // Color palette output
    output logic [C_S_AXI_DATA_WIDTH-1:0]       color_palette_out[8],

    // VGA controller inputs
    input logic [9:0] drawX,
    input logic [9:0] drawY,
    input logic vsync,

    // Global Clock Signal
    input logic  S_AXI_ACLK,
    // Global Reset Signal. This Signal is Active LOW
    input logic  S_AXI_ARESETN,
    // Write address (issued by master, acceped by Slave)
    input logic [C_S_AXI_ADDR_WIDTH-1 : 0] S_AXI_AWADDR,
    // Write channel Protection type. This signal indicates the
        // privilege and security level of the transaction, and whether
        // the transaction is a data access or an instruction access.
    input logic [2 : 0] S_AXI_AWPROT,
    // Write address valid. This signal indicates that the master signaling
        // valid write address and control information.
    input logic  S_AXI_AWVALID,
    // Write address ready. This signal indicates that the slave is ready
        // to accept an address and associated control signals.
    output logic  S_AXI_AWREADY,
    // Write data (issued by master, acceped by Slave) 
    input logic [C_S_AXI_DATA_WIDTH-1 : 0] S_AXI_WDATA,
    // Write strobes. This signal indicates which byte lanes hold
        // valid data. There is one write strobe bit for each eight
        // bits of the write data bus.    
    input logic [(C_S_AXI_DATA_WIDTH/8)-1 : 0] S_AXI_WSTRB,
    // Write valid. This signal indicates that valid write
        // data and strobes are available.
    input logic  S_AXI_WVALID,
    // Write ready. This signal indicates that the slave
        // can accept the write data.
    output logic  S_AXI_WREADY,
    // Write response. This signal indicates the status
        // of the write transaction.
    output logic [1 : 0] S_AXI_BRESP,
    // Write response valid. This signal indicates that the channel
        // is signaling a valid write response.
    output logic  S_AXI_BVALID,
    // Response ready. This signal indicates that the master
        // can accept a write response.
    input logic  S_AXI_BREADY,
    // Read address (issued by master, acceped by Slave)
    input logic [C_S_AXI_ADDR_WIDTH-1 : 0] S_AXI_ARADDR,
    // Protection type. This signal indicates the privilege
        // and security level of the transaction, and whether the
        // transaction is a data access or an instruction access.
    input logic [2 : 0] S_AXI_ARPROT,
    // Read address valid. This signal indicates that the channel
        // is signaling valid read address and control information.
    input logic  S_AXI_ARVALID,
    // Read address ready. This signal indicates that the slave is
        // ready to accept an address and associated control signals.
    output logic  S_AXI_ARREADY,
    // Read data (issued by slave)
    output logic [C_S_AXI_DATA_WIDTH-1 : 0] S_AXI_RDATA,
    // Read response. This signal indicates the status of the
        // read transfer.
    output logic [1 : 0] S_AXI_RRESP,
    // Read valid. This signal indicates that the channel is
        // signaling the required read data.
    output logic  S_AXI_RVALID,
    // Read ready. This signal indicates that the master can
        // accept the read data and response information.
    input logic  S_AXI_RREADY
);

// AXI4LITE signals
logic  [C_S_AXI_ADDR_WIDTH-1 : 0] 	axi_awaddr;
logic  axi_awready;
logic  axi_wready;
logic  [1 : 0] 	axi_bresp;
logic  axi_bvalid;
logic  [C_S_AXI_ADDR_WIDTH-1 : 0] 	axi_araddr;
logic  axi_arready;
logic  [C_S_AXI_DATA_WIDTH-1 : 0] 	axi_rdata;
logic  [1 : 0] 	axi_rresp;
logic  	axi_rvalid;

localparam integer ADDR_LSB = (C_S_AXI_DATA_WIDTH/32) + 1;
localparam integer OPT_MEM_ADDR_BITS = 11;

//----------------------------------------------
//-- Signals for user logic register space example
//------------------------------------------------
//-- Number of Slave Registers 4
//logic [C_S_AXI_DATA_WIDTH-1:0]	slv_reg0;
//logic [C_S_AXI_DATA_WIDTH-1:0]	slv_reg1;
//logic [C_S_AXI_DATA_WIDTH-1:0]	slv_reg2;
//logic [C_S_AXI_DATA_WIDTH-1:0]	slv_reg3;
//
//Note: the provided Verilog template had the registered declared as above, but in order to give 
//students a hint we have replaced the 4 individual registers with an unpacked array of packed logic. 
//Note that you as the student will still need to extend this to the full register set needed for the lab.
logic [C_S_AXI_DATA_WIDTH-1:0] color_palette[8];
logic [C_S_AXI_DATA_WIDTH-1:0] frame_counter;
logic [C_S_AXI_DATA_WIDTH-1:0] current_draw_x;
logic [C_S_AXI_DATA_WIDTH-1:0] current_draw_y;

// BRAM Port A control
logic bram_en_reg;
logic [3:0] bram_we_reg;
logic [10:0] bram_addr_reg;
logic [C_S_AXI_DATA_WIDTH-1:0] bram_din_reg;

logic	 slv_reg_rden;
logic	 slv_reg_wren;
logic [C_S_AXI_DATA_WIDTH-1:0]	 reg_data_out;
integer	 byte_index;
logic	 aw_en;

// Vsync edge detection
logic vsync_prev;
logic vChange;

// Multi-cycle read counter for BRAM latency
logic [2:0] read_delay_counter;

// I/O Connections
assign S_AXI_AWREADY	= axi_awready;
assign S_AXI_WREADY	= axi_wready;
assign S_AXI_BRESP	= axi_bresp;
assign S_AXI_BVALID	= axi_bvalid;
assign S_AXI_ARREADY = axi_arready;
assign S_AXI_RDATA	= axi_rdata;
assign S_AXI_RRESP	= axi_rresp;
assign S_AXI_RVALID	= axi_rvalid;

assign bram_ena = bram_en_reg;
assign bram_wea = bram_we_reg;
assign bram_addra = bram_addr_reg;
assign bram_dina = bram_din_reg;
assign color_palette_out = color_palette;

// Address decode
logic[3:0] palette_idx
logic is_vram_region_write;
logic is_palette_region_write;
logic is_control_region_write;
logic is_vram_region_read;
logic is_palette_region_read;
logic is_control_region_read;
logic [11:0] write_word_addr;
logic [11:0] read_word_addr;

assign write_word_addr = axi_awaddr[ADDR_LSB+OPT_MEM_ADDR_BITS:ADDR_LSB];
assign read_word_addr = axi_araddr[ADDR_LSB+OPT_MEM_ADDR_BITS:ADDR_LSB];

always_comb begin
    is_vram_region_write = (write_word_addr < 12'd1200);
    is_palette_region_write = (write_word_addr >= 12'd2048) && (write_word_addr <= 12'd2055);
    is_control_region_write = (write_word_addr >= 12'd2056) && (write_word_addr <= 12'd2058);
end

always_comb begin
    is_vram_region_read = (read_word_addr < 12'd1200);
    is_palette_region_read = (read_word_addr >= 12'd2048) && (read_word_addr <= 12'd2055);
    is_control_region_read = (read_word_addr >= 12'd2056) && (read_word_addr <= 12'd2058);
end

// Implement axi_awready generation
always_ff @( posedge S_AXI_ACLK )
begin
  if ( S_AXI_ARESETN == 1'b0 )
    begin
      axi_awready <= 1'b0;
      aw_en <= 1'b1;
    end
  else
    begin
      if (~axi_awready && S_AXI_AWVALID && S_AXI_WVALID && aw_en)
        begin
          // slave is ready to accept write address when 
          // there is a valid write address and write data
          // on the write address and data bus. This design 
          // expects no outstanding transactions. 
          axi_awready <= 1'b1;
          aw_en <= 1'b0;
        end
        else if (S_AXI_BREADY && axi_bvalid)
            begin
              aw_en <= 1'b1;
              axi_awready <= 1'b0;
            end
      else
        begin
          axi_awready <= 1'b0;
        end
    end
end

// Implement axi_awaddr latching
// This process is used to latch the address when both 
// S_AXI_AWVALID and S_AXI_WVALID are valid.
always_ff @( posedge S_AXI_ACLK )
begin
  if ( S_AXI_ARESETN == 1'b0 )
    begin
      axi_awaddr <= 0;
    end
  else
    begin
      if (~axi_awready && S_AXI_AWVALID && S_AXI_WVALID && aw_en)
        begin
          axi_awaddr <= S_AXI_AWADDR;
        end
    end
end

// Implement axi_wready generation
// axi_wready is asserted for one S_AXI_ACLK clock cycle when both
// S_AXI_AWVALID and S_AXI_WVALID are asserted. axi_wready is 
// de-asserted when reset is low. 
always_ff @( posedge S_AXI_ACLK )
begin
  if ( S_AXI_ARESETN == 1'b0 )
    begin
      axi_wready <= 1'b0;
    end
  else
    begin
      if (~axi_wready && S_AXI_WVALID && S_AXI_AWVALID && aw_en )
        begin
          // slave is ready to accept write data when 
          // there is a valid write address and write data
          // on the write address and data bus. This design 
          // expects no outstanding transactions. 
          axi_wready <= 1'b1;
        end
      else
        begin
          axi_wready <= 1'b0;
        end
    end
end

// Implement memory mapped register select and write logic generation
// The write data is accepted and written to memory mapped registers when
// axi_awready, S_AXI_WVALID, axi_wready and S_AXI_WVALID are asserted. Write strobes are used to
// select byte enables of slave registers while writing.
// These registers are cleared when reset (active low) is applied.
// Slave register write enable is asserted when valid address and data are available
// and the slave is ready to accept the write address and write data.
assign slv_reg_wren = axi_wready && S_AXI_WVALID && axi_awready && S_AXI_AWVALID;

always_ff @( posedge S_AXI_ACLK )
begin
  if ( S_AXI_ARESETN == 1'b0 )
    begin
        bram_en_reg <= 1'b0;
        bram_we_reg <= 4'b0000;
        bram_addr_reg <= 11'd0;
        bram_din_reg <= 32'd0;

        for (int i = 0; i < 8; i++) begin
            color_palette[i] <= 32'd0;
        end

        current_draw_x <= 32'd0;
        current_draw_y <= 32'd0;
    end
  else begin
    bram_we_reg <= 4'b0000;
    bram_en_reg <= 1'b0;

    current_draw_x <= {22'd0, drawX};
    current_draw_y <= {22'd0, drawY};

    if (slv_reg_wren)
      begin
        if (is_vram_region_write) begin
            bram_en_reg <= 1'b1;
            bram_we_reg <= S_AXI_WSTRB;
            bram_addr_reg <= write_word_addr[10:0];
            bram_din_reg <= S_AXI_WDATA;
        end
        else if (is_palette_region_write) begin
            palette_idx = write_word_addr - 12'd2048;
            color_palette[palette_idx] <= S_AXI_WDATA;
        end
      end
    else if (slv_reg_rden)
      begin
        bram_addr_reg <= read_word_addr[10:0];
        bram_en_reg <= 1'b1;
        bram_we_reg <= 4'b0000;
      end
    else begin
        bram_en_reg <= 1'b0;
        bram_we_reg <= 4'b0000;
    end
  end
end

// Implement write response logic generation
// The write response and response valid signals are asserted by the slave 
// when axi_wready, S_AXI_WVALID, axi_wready and S_AXI_WVALID are asserted.  
// This marks the acceptance of address and indicates the status of 
// write transaction.
always_ff @( posedge S_AXI_ACLK )
begin
  if ( S_AXI_ARESETN == 1'b0 )
    begin
      axi_bvalid  <= 0;
      axi_bresp   <= 2'b0;
    end
  else
    begin
      if (axi_awready && S_AXI_AWVALID && ~axi_bvalid && axi_wready && S_AXI_WVALID)
        begin
          axi_bvalid <= 1'b1;
          axi_bresp  <= 2'b0;
        end
      else
        begin
          if (S_AXI_BREADY && axi_bvalid)
            begin
              axi_bvalid <= 1'b0;
            end
        end
    end
end

// Implement axi_arready generation
// axi_arready is asserted for one S_AXI_ACLK clock cycle when
// S_AXI_ARVALID is asserted. axi_awready is 
// de-asserted when reset (active low) is asserted. 
// The read address is also latched when S_AXI_ARVALID is 
// asserted. axi_araddr is reset to zero on reset assertion.
always_ff @(posedge S_AXI_ACLK) begin
    if (~S_AXI_ARESETN) begin
        axi_arready <= 1'b0;
        axi_araddr  <= '0;
    end
    else begin
        if (~axi_arready && S_AXI_ARVALID) begin
            // indicates that the slave has acceped the valid read address
            axi_arready <= 1'b1;
            // Read address latching
            axi_araddr  <= S_AXI_ARADDR;
        end
        else begin
            axi_arready <= 1'b0;
        end
    end
end

// Implement axi_arvalid generation
// axi_rvalid is asserted for one S_AXI_ACLK clock cycle when both 
// S_AXI_ARVALID and axi_arready are asserted. The slave registers 
// data are available on the axi_rdata bus at this instance. The 
// assertion of axi_rvalid marks the validity of read data on the 
// bus and axi_rresp indicates the status of read transaction.axi_rvalid 
// is deasserted on reset (active low). axi_rresp and axi_rdata are 
// cleared to zero on reset (active low).
always_ff @(posedge S_AXI_ACLK) begin
    if (~S_AXI_ARESETN) begin
        axi_rvalid <= 1'b0;
        axi_rresp  <= 2'b00;
        read_delay_counter <= 3'b000;
        axi_rdata  <= '0;
    end
    else begin
        if (axi_arready && S_AXI_ARVALID && ~axi_rvalid && read_delay_counter == 3'b000) begin
            read_delay_counter <= 3'b001;
        end
        else if (read_delay_counter == 3'b001) begin
            read_delay_counter <= 3'b010;
        end
        else if (read_delay_counter == 3'b010) begin
            axi_rdata <= reg_data_out;
            axi_rvalid <= 1'b1;
            axi_rresp  <= 2'b00;
            read_delay_counter <= 3'b000;
        end
        else if (axi_rvalid && S_AXI_RREADY) begin
            axi_rvalid <= 1'b0;
        end
    end
end

assign slv_reg_rden = axi_arready & S_AXI_ARVALID & ~axi_rvalid;

// Read data multiplexer
always_comb
begin
    if (is_vram_region_read) begin
        reg_data_out = bram_douta;
    end
    else if (is_palette_region_read) begin
        automatic int palette_idx = read_word_addr - 12'd2048;
        reg_data_out = color_palette[palette_idx];
    end
    else if (is_control_region_read) begin
        case (read_word_addr)
            12'd2056: reg_data_out = frame_counter;
            12'd2057: reg_data_out = current_draw_x;
            12'd2058: reg_data_out = current_draw_y;
            default:  reg_data_out = 32'd0;
        endcase
    end
    else begin
        reg_data_out = 32'd0;
    end
end

// Vsync edge detection
always_ff @(posedge S_AXI_ACLK)
begin
  if(~S_AXI_ARESETN) begin
    vsync_prev <= 1'b0;
    vChange <= 1'b0;
  end else begin
    vsync_prev <= vsync;
    vChange <= vsync && ~vsync_prev;
  end
end

// Frame counter
always_ff @(posedge S_AXI_ACLK)
begin
  if(~S_AXI_ARESETN) begin
    frame_counter <= 32'd0;
  end else if (vChange) begin
    frame_counter <= frame_counter + 1;
  end
end

endmodule
