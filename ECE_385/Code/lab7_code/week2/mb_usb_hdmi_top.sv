//-------------------------------------------------------------------------
//    mb_usb_hdmi_top.sv                                                 --
//    Zuofu Cheng                                                        --
//    2-29-24                                                            --
//                                                                       --
//                                                                       --
//    Spring 2024 Distribution                                           --
//                                                                       --
//    For use with ECE 385 USB + HDMI                                    --
//    University of Illinois ECE Department                              --
//-------------------------------------------------------------------------

module lab7_1(
    input  logic Clk,                   // 100 MHz input clock from board
    input  logic reset_rtl_0,           // active-high reset button
    // UART
    input  logic uart_rtl_0_rxd,
    output logic uart_rtl_0_txd,
    // HDMI
    output logic HDMI_0_tmds_clk_n,
    output logic HDMI_0_tmds_clk_p,
    output logic [2:0] HDMI_0_tmds_data_n,
    output logic [2:0] HDMI_0_tmds_data_p
);

//    logic reset_test;
//    assign reset_test = 1'b1;
    //--------------------------------------------------
    // Instantiate Block Design Wrapper
    //--------------------------------------------------
    hdmi_block1 hdmi_block_inst (
        .HDMI_0_tmds_clk_n(HDMI_0_tmds_clk_n),
        .HDMI_0_tmds_clk_p(HDMI_0_tmds_clk_p),
        .HDMI_0_tmds_data_n(HDMI_0_tmds_data_n),
        .HDMI_0_tmds_data_p(HDMI_0_tmds_data_p),
        .clk_100MHz(Clk),               // connect board clock to BD clock
        .reset_rtl_0(~reset_rtl_0),      // if BD expects active-high reset
        .uart_rtl_0_rxd(uart_rtl_0_rxd),
        .uart_rtl_0_txd(uart_rtl_0_txd)
    );

endmodule