//------------------------------------------------------------------------------
// Company: 		 UIUC ECE Dept.
// Engineer:		 Stephen Kempf
//
// Create Date:    
// Design Name:    ECE 385 Given Code - SLC-3 core
// Module Name:    SLC3
//
// Comments:
//    Revised 03-22-2007
//    Spring 2007 Distribution
//    Revised 07-26-2013
//    Spring 2015 Distribution
//    Revised 09-22-2015 
//    Revised 06-09-2020
//	  Revised 03-02-2021
//    Xilinx vivado
//    Revised 07-25-2023 
//    Revised 12-29-2023
//    Revised 09-25-2024
//------------------------------------------------------------------------------

module cpu (
    input   logic        clk,
    input   logic        reset,

    input   logic        run_i,
    input   logic        continue_i,
    output  logic [15:0] hex_display_debug,
    output  logic [15:0] led_o,
   
    input   logic [15:0] mem_rdata,
    output  logic [15:0] mem_wdata,
    output  logic [15:0] mem_addr,
    output  logic        mem_mem_ena,
    output  logic        mem_wr_ena
);


// Internal connections, follow the datapath block diagram and add the additional needed signals
logic ld_mar;
logic ld_mdr;
logic ld_ir;
logic ld_pc;
logic ld_led;
logic ld_reg;
logic ld_cc;

logic gate_pc;
logic gate_mdr;
logic gate_mar;
logic gate_alu;

logic [1:0] pcmux;
logic [1:0] adder2mux;
logic       adder1mux;
logic [1:0]	aluk;
logic       sr2mux;  
logic mio_en;

logic [15:0] mar; 
logic [15:0] mdr;
logic [15:0] ir;
logic [15:0] pc;

logic [15:0] bus;
logic [15:0] pc_next;
logic [15:0] alu_mar_result;
logic [15:0] addr1_result;
logic [15:0] addr2_result;
logic [15:0] alu_b;
logic [15:0] alu_a;
logic [15:0] alu_result;
logic [15:0] mdr_next;

logic ben;
logic [2:0] nzp;

logic [2:0]  sr1;
logic [2:0]  sr2;
logic [2:0]  dr;

logic [15:0] reg_file [8]; // 8 registers, each 16 bits wide
logic [7:0]  reg_load; // One-hot load signals for the register file


assign mem_addr = mar;
assign mem_wdata = mdr;

always_ff @(posedge clk)
    begin
        if (reset)
            nzp <= 3'b010; // Reset to zero condition
        else if (ld_cc)
            begin
                nzp[2] <= (bus[15] == 1'b1) ? 1'b1 : 1'b0; // N
                nzp[1] <= (bus == 16'b0) ? 1'b1 : 1'b0;    // Z
                nzp[0] <= (bus[15] == 1'b0 && bus != 16'b0) ? 1'b1 : 1'b0; // P
            end
    end

assign ben = ( (ir[11] & nzp[2] ) | (ir[10] & nzp[1]) | (ir[9] & nzp[0] )); // N or Z or P

logic [15:0] SEXT_offset4;

always_comb
    begin
        SEXT_offset4 = {{11{ir[4]}}, ir[4:0]};

        case(sr2mux)
            1'b0: alu_b = reg_file[sr2]; // SR2
            1'b1: alu_b = SEXT_offset4; // SEXT(IR[4:0])
            default: alu_b = 16'b0; // Default value to avoid latches
        endcase

        alu_a = reg_file[sr1]; // SR1
        case(aluk)
            2'b00: alu_result = alu_a + alu_b; // ADD
            2'b01: alu_result = alu_a & alu_b; // AND
            2'b10: alu_result = ~alu_a;        // NOT
            2'b11: alu_result = alu_a;       // PASSA (not used in this lab)
            default: alu_result = 16'b0;       // Default value to avoid latches
        endcase
    end
always_comb
    begin
        case(dr)
          3'b000: reg_load= 8'b00000001;
          3'b001: reg_load = 8'b00000010;
          3'b010: reg_load = 8'b00000100;
          3'b011: reg_load = 8'b00001000;
          3'b100: reg_load = 8'b00010000;
          3'b101: reg_load = 8'b00100000;
          3'b110: reg_load = 8'b01000000;
          3'b111: reg_load = 8'b10000000;
          default: reg_load = 8'b00000000; // Default value to avoid latches
        endcase
    end
always_comb
    begin
        if(gate_pc)
            bus = pc;
        else if(gate_mdr)
            bus = mdr;
        else if(gate_mar)
            bus = alu_mar_result;
        else if(gate_alu)
            bus = alu_result;
        else
            bus = 16'b0; // Default value to avoid latches
    end

always_comb
    begin
        case(pcmux)
            2'b00: pc_next = pc + 16'd1; // PC + 1
            2'b01: pc_next = bus;        // BUS
            2'b10: pc_next = alu_mar_result; // ALU/MAR result
            default: pc_next = 16'b0;    // Default value to avoid latches
        endcase
    end

always_comb
    begin
        case(mio_en)
            1'b0: mdr_next = mem_rdata; // Memory data
            1'b1: mdr_next = bus;       // BUS
            default: mdr_next = 16'b0;    // Default value to avoid latches
        endcase
    end

logic [15:0] SEXT_offset6;
logic [15:0] SEXT_offset9;
logic [15:0] SEXT_offset11;

always_comb
    begin
        SEXT_offset6 = {{10{ir[5]}}, ir[5:0]};
        SEXT_offset9 = {{7{ir[8]}}, ir[8:0]};
        SEXT_offset11 = {{5{ir[10]}}, ir[10:0]};

        case (adder1mux)
            1'b0: addr1_result = pc; // PC
            1'b1: addr1_result = reg_file[sr1]; // BaseR
            default: addr1_result = 16'b0; // Default value to avoid latches
        endcase
        case (adder2mux)
            2'b00: addr2_result = 16'd0; // 0
            2'b01: addr2_result = SEXT_offset6; // SEXT(IR[5:0])
            2'b10: addr2_result = SEXT_offset9; // SEXT(IR[8:0])
            2'b11: addr2_result = SEXT_offset11; // SEXT(IR[10:0])
            default: addr2_result = 16'b0; // Default value to avoid latches
        endcase
        alu_mar_result = addr1_result + addr2_result;
    end

// State machine, you need to fill in the code here as well
// .* auto-infers module input/output connections which have the same name
// This can help visually condense modules with large instantiations, 
// but can also lead to confusing code if used too commonly
control cpu_control (
    .*
);


assign led_o = ir;
assign hex_display_debug = ir;

load_reg #(.DATA_WIDTH(16)) ir_reg (
    .clk    (clk),
    .reset  (reset),

    .load   (ld_ir),
    .data_i (bus),

    .data_q (ir)
);

load_reg #(.DATA_WIDTH(16)) pc_reg (
    .clk(clk),
    .reset(reset),

    .load(ld_pc),
    .data_i(pc_next),

    .data_q(pc)
);
load_reg #(.DATA_WIDTH(16)) mar_reg (
    .clk(clk),
    .reset(reset),

    .load(ld_mar),
    .data_i(bus),

    .data_q(mar)
);
load_reg #(.DATA_WIDTH(16)) mdr_reg (
    .clk(clk),
    .reset(reset),

    .load(ld_mdr),
    .data_i(mdr_next),

    .data_q(mdr)
);
load_reg #(.DATA_WIDTH(16)) reg_0 (
    .clk(clk),
    .reset(reset),

    .load(reg_load[0] & ld_reg),
    .data_i(bus),

    .data_q(reg_file[0])
);
load_reg #(.DATA_WIDTH(16)) reg_1 (
    .clk(clk),
    .reset(reset),

    .load(reg_load[1] & ld_reg),
    .data_i(bus),

    .data_q(reg_file[1])
);
load_reg #(.DATA_WIDTH(16)) reg_2 (
    .clk(clk),
    .reset(reset),

    .load(reg_load[2] & ld_reg),
    .data_i(bus),

    .data_q(reg_file[2])
);
load_reg #(.DATA_WIDTH(16)) reg_3 (
    .clk(clk),
    .reset(reset),

    .load(reg_load[3] & ld_reg),
    .data_i(bus),

    .data_q(reg_file[3])
);
load_reg #(.DATA_WIDTH(16)) reg_4 (
    .clk(clk),
    .reset(reset),

    .load(reg_load[4] & ld_reg),
    .data_i(bus),

    .data_q(reg_file[4])
);
load_reg #(.DATA_WIDTH(16)) reg_5 (
    .clk(clk),
    .reset(reset),

    .load(reg_load[5] & ld_reg),
    .data_i(bus),

    .data_q(reg_file[5])
);
load_reg #(.DATA_WIDTH(16)) reg_6 (
    .clk(clk),
    .reset(reset),

    .load(reg_load[6] & ld_reg),
    .data_i(bus),

    .data_q(reg_file[6])
);
load_reg #(.DATA_WIDTH(16)) reg_7 (
    .clk(clk),
    .reset(reset),

    .load(reg_load[7] & ld_reg),
    .data_i(bus),

    .data_q(reg_file[7])
);




endmodule