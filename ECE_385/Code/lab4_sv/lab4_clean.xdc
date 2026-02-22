# Lab 4 Multiplier Pin Constraints for Urbana FPGA Board - CLEANED VERSION

## Clock signal
set_property -dict {PACKAGE_PIN N15 IOSTANDARD LVCMOS33} [get_ports Clk]
create_clock -period 10.000 -name sys_clk_pin -waveform {0.000 5.000} -add [get_ports Clk]

## Switches
set_property -dict {PACKAGE_PIN G1 IOSTANDARD LVCMOS25} [get_ports {SW_i[0]}]
set_property -dict {PACKAGE_PIN F2 IOSTANDARD LVCMOS25} [get_ports {SW_i[1]}]
set_property -dict {PACKAGE_PIN F1 IOSTANDARD LVCMOS25} [get_ports {SW_i[2]}]
set_property -dict {PACKAGE_PIN E2 IOSTANDARD LVCMOS25} [get_ports {SW_i[3]}]
set_property -dict {PACKAGE_PIN E1 IOSTANDARD LVCMOS25} [get_ports {SW_i[4]}]
set_property -dict {PACKAGE_PIN D2 IOSTANDARD LVCMOS25} [get_ports {SW_i[5]}]
set_property -dict {PACKAGE_PIN D1 IOSTANDARD LVCMOS25} [get_ports {SW_i[6]}]
set_property -dict {PACKAGE_PIN C2 IOSTANDARD LVCMOS25} [get_ports {SW_i[7]}]

## LEDs for A register (8 bits)
set_property -dict {PACKAGE_PIN C17 IOSTANDARD LVCMOS33} [get_ports {Aval[0]}]
set_property -dict {PACKAGE_PIN B18 IOSTANDARD LVCMOS33} [get_ports {Aval[1]}]
set_property -dict {PACKAGE_PIN A17 IOSTANDARD LVCMOS33} [get_ports {Aval[2]}]
set_property -dict {PACKAGE_PIN B17 IOSTANDARD LVCMOS33} [get_ports {Aval[3]}]
set_property -dict {PACKAGE_PIN C18 IOSTANDARD LVCMOS33} [get_ports {Aval[4]}]
set_property -dict {PACKAGE_PIN D18 IOSTANDARD LVCMOS33} [get_ports {Aval[5]}]
set_property -dict {PACKAGE_PIN E18 IOSTANDARD LVCMOS33} [get_ports {Aval[6]}]
set_property -dict {PACKAGE_PIN G17 IOSTANDARD LVCMOS33} [get_ports {Aval[7]}]

## LEDs for B register (8 bits)
set_property -dict {PACKAGE_PIN C13 IOSTANDARD LVCMOS33} [get_ports {Bval[0]}]
set_property -dict {PACKAGE_PIN C14 IOSTANDARD LVCMOS33} [get_ports {Bval[1]}]
set_property -dict {PACKAGE_PIN D14 IOSTANDARD LVCMOS33} [get_ports {Bval[2]}]
set_property -dict {PACKAGE_PIN D15 IOSTANDARD LVCMOS33} [get_ports {Bval[3]}]
set_property -dict {PACKAGE_PIN D16 IOSTANDARD LVCMOS33} [get_ports {Bval[4]}]
set_property -dict {PACKAGE_PIN F18 IOSTANDARD LVCMOS33} [get_ports {Bval[5]}]
set_property -dict {PACKAGE_PIN E17 IOSTANDARD LVCMOS33} [get_ports {Bval[6]}]
set_property -dict {PACKAGE_PIN D17 IOSTANDARD LVCMOS33} [get_ports {Bval[7]}]

## LED for X register (1 bit)
set_property -dict {PACKAGE_PIN C9 IOSTANDARD LVCMOS33} [get_ports Xval]

## Buttons
set_property -dict {PACKAGE_PIN J2 IOSTANDARD LVCMOS25} [get_ports reset_load_clear]
set_property -dict {PACKAGE_PIN J1 IOSTANDARD LVCMOS25} [get_ports Run]

## 7-segment display
set_property -dict {PACKAGE_PIN E6 IOSTANDARD LVCMOS25} [get_ports {hex_seg[0]}]
set_property -dict {PACKAGE_PIN B4 IOSTANDARD LVCMOS25} [get_ports {hex_seg[1]}]
set_property -dict {PACKAGE_PIN D5 IOSTANDARD LVCMOS25} [get_ports {hex_seg[2]}]
set_property -dict {PACKAGE_PIN C5 IOSTANDARD LVCMOS25} [get_ports {hex_seg[3]}]
set_property -dict {PACKAGE_PIN D7 IOSTANDARD LVCMOS25} [get_ports {hex_seg[4]}]
set_property -dict {PACKAGE_PIN D6 IOSTANDARD LVCMOS25} [get_ports {hex_seg[5]}]
set_property -dict {PACKAGE_PIN C4 IOSTANDARD LVCMOS25} [get_ports {hex_seg[6]}]
set_property -dict {PACKAGE_PIN B5 IOSTANDARD LVCMOS25} [get_ports {hex_seg[7]}]

## 7-segment display grid (digit select)
set_property -dict {PACKAGE_PIN G6 IOSTANDARD LVCMOS25} [get_ports {hex_grid[0]}]
set_property -dict {PACKAGE_PIN H6 IOSTANDARD LVCMOS25} [get_ports {hex_grid[1]}]
set_property -dict {PACKAGE_PIN C3 IOSTANDARD LVCMOS25} [get_ports {hex_grid[2]}]
set_property -dict {PACKAGE_PIN B3 IOSTANDARD LVCMOS25} [get_ports {hex_grid[3]}]

## Configuration options
set_property CONFIG_VOLTAGE 3.3 [current_design]
set_property CFGBVS VCCO [current_design]