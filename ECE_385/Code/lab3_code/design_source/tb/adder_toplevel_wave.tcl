restart
log_wave -recursive /adder_toplevel_tb

# Key top-level and internal aliases
add_wave -radix hex /adder_toplevel_tb/sw_i
add_wave -radix hex /adder_toplevel_tb/regA
add_wave -radix hex /adder_toplevel_tb/regB
add_wave -radix hex /adder_toplevel_tb/sum
add_wave             /adder_toplevel_tb/carry
add_wave             /adder_toplevel_tb/reset_s_mon
add_wave             /adder_toplevel_tb/run_s_mon
add_wave             /adder_toplevel_tb/reset
add_wave             /adder_toplevel_tb/run_i
add_wave             /adder_toplevel_tb/clk
add_wave             /adder_toplevel_tb/sign_led

# Also log internal adder interface if present
if {[string length [get_objects /adder_toplevel_tb/dut/adder_la]]} {
  add_wave -radix hex /adder_toplevel_tb/dut/adder_la/a
  add_wave -radix hex /adder_toplevel_tb/dut/adder_la/b
}

# Place HEX-related signals at bottom in their own group
add_wave -group {HEX Displays} -radix hex /adder_toplevel_tb/hex_grid_a
add_wave -group {HEX Displays} -radix hex /adder_toplevel_tb/hex_seg_a
add_wave -group {HEX Displays} -radix hex /adder_toplevel_tb/hex_grid_b
add_wave -group {HEX Displays} -radix hex /adder_toplevel_tb/hex_seg_b

run all
write_waveform -force design_source/tb/adder_toplevel_tb.wcfg
