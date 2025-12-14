`timescale 1ns / 1ps

module mb_usb_hdmi_top(
    input  logic        Clk,           
    input  logic        reset_rtl_0,   

    //USB signals
    input  logic [0:0]  gpio_usb_int_tri_i,   
    output logic        gpio_usb_rst_tri_o,   
    input  logic        usb_spi_miso,         
    output logic        usb_spi_mosi,         
    output logic        usb_spi_sclk,         
    output logic        usb_spi_ss,           

    //USB UART signals
    input  logic        uart_rtl_0_rxd,
    output logic        uart_rtl_0_txd,

    //HDMI output signals 
    output logic        hdmi_clk_n,
    output logic        hdmi_clk_p,
    output logic [2:0]  hdmi_tx_n,
    output logic [2:0]  hdmi_tx_p
);
  

    logic [31:0] gpio_usb_keycode_0;  
    logic [31:0] gpio_usb_keycode_1;  



    Final_Dance_wrapper block_design_wrapper (
        .clk_in1_0(Clk),

        .reset_rtl_0(~reset_rtl_0),

        .gpio_usb_int_tri_i(gpio_usb_int_tri_i),     
        .gpio_usb_keycode_0_tri_i(gpio_usb_keycode_0), 
        .gpio_usb_keycode_1_tri_o(gpio_usb_keycode_1), 
        .gpio_usb_rst_tri_o(gpio_usb_rst_tri_o),     

        .usb_spi_miso(usb_spi_miso),
        .usb_spi_mosi(usb_spi_mosi),
        .usb_spi_sclk(usb_spi_sclk),
        .usb_spi_ss(usb_spi_ss),

        .uart_rtl_0_rxd(uart_rtl_0_rxd),
        .uart_rtl_0_txd(uart_rtl_0_txd),

        .HDMI_0_tmds_clk_n(hdmi_clk_n),
        .HDMI_0_tmds_clk_p(hdmi_clk_p),
        .HDMI_0_tmds_data_n(hdmi_tx_n),
        .HDMI_0_tmds_data_p(hdmi_tx_p)
    );

    assign gpio_usb_keycode_0 = 32'b0;

endmodule
