//Provided HDMI_Text_controller_v1_0 for HDMI AXI4 IP
//Fall 2024 Distribution
//Modified 3/10/24 by Zuofu
//Updated 11/18/24 by Zuofu

`timescale 1 ns / 1 ps

module hdmi_text_controller_v1_0 #
(
    parameter integer C_AXI_DATA_WIDTH	= 32,
    parameter integer C_AXI_ADDR_WIDTH	= 14
)
(
    // HDMI outputs
    output logic hdmi_clk_n,
    output logic hdmi_clk_p,
    output logic [2:0] hdmi_tx_n,
    output logic [2:0] hdmi_tx_p,

    // AXI Slave Bus Interface
    input logic  axi_aclk,
    input logic  axi_aresetn,
    input logic [C_AXI_ADDR_WIDTH-1 : 0] axi_awaddr,
    input logic [2 : 0] axi_awprot,
    input logic  axi_awvalid,
    output logic  axi_awready,
    input logic [C_AXI_DATA_WIDTH-1 : 0] axi_wdata,
    input logic [(C_AXI_DATA_WIDTH/8)-1 : 0] axi_wstrb,
    input logic  axi_wvalid,
    output logic  axi_wready,
    output logic [1 : 0] axi_bresp,
    output logic  axi_bvalid,
    input logic  axi_bready,
    input logic [C_AXI_ADDR_WIDTH-1 : 0] axi_araddr,
    input logic [2 : 0] axi_arprot,
    input logic  axi_arvalid,
    output logic  axi_arready,
    output logic [C_AXI_DATA_WIDTH-1 : 0] axi_rdata,
    output logic [1 : 0] axi_rresp,
    output logic  axi_rvalid,
    input logic  axi_rready
);

    logic clk_25MHz, clk_125MHz;
    logic locked;
    logic [9:0] drawX, drawY;
    logic hsync, vsync, vde;
    logic [3:0] red, green, blue;
    logic reset_ah;

    // BRAM Port A (AXI access)
    logic mem_enable_a;
    logic [3:0] mem_write_enable_a;
    logic [10:0] mem_addra_a;
    logic [31:0] mem_write_a;
    logic [31:0] mem_data_a;

    // BRAM Port B (Color_Mapper access)
    logic mem_enable_b;
    logic [3:0] mem_write_enable_b;
    logic [10:0] mem_addra_b;
    logic [31:0] mem_write_b;
    logic [31:0] mem_data_b;

    // Color palette
    logic [31:0] color_palette[8];

    assign reset_ah = ~axi_aresetn;
    assign mem_enable_b = 1'b1;
    assign mem_write_enable_b = 4'b0000;
    assign mem_write_b = 32'h0;

    // Instantiation of Axi Bus Interface
    hdmi_text_controller_v1_0_AXI # (
        .C_S_AXI_DATA_WIDTH(C_AXI_DATA_WIDTH),
        .C_S_AXI_ADDR_WIDTH(C_AXI_ADDR_WIDTH)
    ) hdmi_text_controller_v1_0_AXI_inst (
        .S_AXI_ACLK(axi_aclk),
        .S_AXI_ARESETN(axi_aresetn),
        .S_AXI_AWADDR(axi_awaddr),
        .S_AXI_AWPROT(axi_awprot),
        .S_AXI_AWVALID(axi_awvalid),
        .S_AXI_AWREADY(axi_awready),
        .S_AXI_WDATA(axi_wdata),
        .S_AXI_WSTRB(axi_wstrb),
        .S_AXI_WVALID(axi_wvalid),
        .S_AXI_WREADY(axi_wready),
        .S_AXI_BRESP(axi_bresp),
        .S_AXI_BVALID(axi_bvalid),
        .S_AXI_BREADY(axi_bready),
        .S_AXI_ARADDR(axi_araddr),
        .S_AXI_ARPROT(axi_arprot),
        .S_AXI_ARVALID(axi_arvalid),
        .S_AXI_ARREADY(axi_arready),
        .S_AXI_RDATA(axi_rdata),
        .S_AXI_RRESP(axi_rresp),
        .S_AXI_RVALID(axi_rvalid),
        .S_AXI_RREADY(axi_rready),
        .bram_ena(mem_enable_a),
        .bram_wea(mem_write_enable_a),
        .bram_addra(mem_addra_a),
        .bram_dina(mem_write_a),
        .bram_douta(mem_data_a),
        .color_palette_out(color_palette),
        .drawX(drawX),
        .drawY(drawY),
        .vsync(vsync)
    );

    clk_wiz_0 clk_wiz (
        .clk_out1(clk_25MHz),
        .clk_out2(clk_125MHz),
        .reset(reset_ah),
        .locked(locked),
        .clk_in1(axi_aclk)
    );

    vga_controller vga (
        .pixel_clk(clk_25MHz),
        .reset(reset_ah),
        .hs(hsync),
        .vs(vsync),
        .active_nblank(vde),
        .drawX(drawX),
        .drawY(drawY)
    );

    hdmi_tx_0 vga_to_hdmi (
        .pix_clk(clk_25MHz),
        .pix_clkx5(clk_125MHz),
        .pix_clk_locked(locked),
        .rst(reset_ah),
        .red(red),
        .green(green),
        .blue(blue),
        .hsync(hsync),
        .vsync(vsync),
        .vde(vde),
        .aux0_din(4'b0),
        .aux1_din(4'b0),
        .aux2_din(4'b0),
        .ade(1'b0),
        .TMDS_CLK_P(hdmi_clk_p),
        .TMDS_CLK_N(hdmi_clk_n),
        .TMDS_DATA_P(hdmi_tx_p),
        .TMDS_DATA_N(hdmi_tx_n)
    );

    color_mapper color_instance(
        .pix_clk(axi_aclk),
        .DrawX(drawX),
        .DrawY(drawY),
        .bram_addra(mem_addra_b),
        .bram_out(mem_data_b),
        .color_palette(color_palette),
        .Red(red),
        .Green(green),
        .Blue(blue)
    );

    blk_mem_gen_0 bram_instance(
        .clka(axi_aclk),
        .ena(mem_enable_a),
        .wea(mem_write_enable_a),
        .addra(mem_addra_a),
        .dina(mem_write_a),
        .douta(mem_data_a),
        .clkb(axi_aclk),
        .enb(mem_enable_b),
        .web(mem_write_enable_b),
        .addrb(mem_addra_b),
        .dinb(mem_write_b),
        .doutb(mem_data_b)
    );

endmodule
