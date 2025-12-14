//------------------------------------------------------------------------------
// Company:          UIUC ECE Dept.
// Engineer:         Stephen Kempf
//
// Create Date:    17:44:03 10/08/06
// Design Name:    ECE 385 Given Code - Incomplete ISDU for SLC-3
// Module Name:    Control - Behavioral
//
// Comments:
//    Revised 03-22-2007
//    Spring 2007 Distribution
//    Revised 07-26-2013
//    Spring 2015 Distribution
//    Revised 02-13-2017
//    Spring 2017 Distribution
//    Revised 07-25-2023
//    Xilinx Vivado
//	  Revised 12-29-2023
// 	  Spring 2024 Distribution
// 	  Revised 6-22-2024
//	  Summer 2024 Distribution
//	  Revised 9-27-2024
//	  Fall 2024 Distribution
//------------------------------------------------------------------------------

module control (
	input logic			clk, 
	input logic			reset,

	input logic  [15:0]	ir,
	input logic			ben,

	input logic 		continue_i,
	input logic 		run_i,

	output logic		ld_mar,
	output logic		ld_mdr,
	output logic		ld_ir,
	output logic		ld_pc,
	output logic        ld_led,
	output logic 		ld_reg,
	output logic 		ld_cc,
						
	output logic		gate_pc,
	output logic		gate_mdr,
	output logic 		gate_mar,
	output logic		gate_alu,
						
	output logic [1:0]	pcmux,
	output logic        sr2mux,
	output logic [1:0]  adder2mux,
	output logic        adder1mux,
	output logic 		mio_en,
	
	//You should add additional control signals according to the SLC-3 datapath design
	output logic [1:0]	aluk,
	output logic [2:0]  sr1,
	output logic [2:0]  sr2,

	output logic [2:0]  dr,

	output logic		mem_mem_ena, // Mem Operation Enable
	output logic		mem_wr_ena  // Mem Write Enable
);

	typedef enum logic [4:0] {
		halted,
		pause_ir1,
		pause_ir2,
		s_18,
		s_33_1,
		s_33_2,
		s_33_3,
		s_35,
		s_32,
		add_op,
		and_op,
		not_op,
		ldr_op1,
		ldr_op2_1,
		ldr_op2_2,
		ldr_op2_3,
		ldr_op3,
		str_op1,
		str_op2,
		str_op3_1,
		str_op3_2,
		str_op3_3,
		jsr_op1,
		jsr_op2,
		jmp_op,
		br_op
	} state_t;

	state_t state, state_nxt;   // Internal state logic
	state_t decode_state;


	always_ff @ (posedge clk)
	begin
		if (reset) 
			state <= halted;
		else 
			state <= state_nxt;
	end
   
	always_comb
	begin 
		
		// Default controls signal values so we don't have to set each signal
		// in each state case below (If we don't set all signals in each state,
		// we can create an inferred latch)
		ld_mar = 1'b0;
		ld_mdr = 1'b0;
		ld_ir = 1'b0;
		ld_pc = 1'b0;
		ld_led = 1'b0;
		ld_reg = 1'b0;
		ld_cc = 1'b0;
		
		gate_pc = 1'b0;
		gate_mdr = 1'b0;
		gate_mar = 1'b0;
		gate_alu = 1'b0;
		 
		pcmux = 2'b00;
		mem_mem_ena = 1'b0; 
		mem_wr_ena = 1'b0;
		mio_en = 1'b0;
		sr2mux = 1'b0;
		adder2mux = 2'b00;
		adder1mux = 1'b0;
		aluk = 2'b00;
		sr1 = 3'b000;
		sr2 = 3'b000;
		dr = 3'b000;
		// Ensure decode_state is driven on all paths to avoid inferred latches
		//decode_state = halted;
		
		
		// Assign relevant control signals based on current state
		case (state)
			halted: ; 
			s_18 : 
				begin 
					gate_pc = 1'b1;
					ld_mar = 1'b1;
					pcmux = 2'b00;
					ld_pc = 1'b1;
					
				end
			s_33_1, s_33_2, s_33_3 : //you may have to think about this as well to adapt to ram with wait-states
				begin
					mem_mem_ena = 1'b1;
					mio_en = 1'b0;
					ld_mdr = 1'b1;
				end
			s_35 : 
				begin 
					gate_mdr = 1'b1;
					ld_ir = 1'b1;
				end
			s_32 : 
				begin
					case (ir[15:12]) // opcode is in ir[15:12]
						4'b0001 : decode_state = add_op; // ADD
						4'b0101 : decode_state = and_op; // AND
						4'b1001 : decode_state = not_op; // NOT
						4'b0110 : decode_state = ldr_op1; // LDR
						4'b0111 : decode_state = str_op1; // STR
						4'b1100 : decode_state = jmp_op; // JMP
						4'b0000 : decode_state = br_op;  // BR
						4'b0100 : decode_state = jsr_op1; // JSR/JSRR
						4'b1101 : decode_state = pause_ir1; // PSE
						default : decode_state = halted; // HALT and other unimplemented opcodes
					endcase
				end
			add_op :
				begin
					aluk = 2'b00;	 // ALUK = 00 for ADD
					dr = ir[11:9]; // DR = IR[11:9]
					sr1 = ir[8:6]; // SR1 = IR[8:6]
					if (ir[5]) begin
						sr2mux = 1'b1;
					end else begin
						sr2mux = 1'b0;
						sr2 = ir[2:0]; // SR2 = IR[2:0]
					end
					gate_alu = 1'b1; // Put ALU result on bus
					ld_reg = 1'b1;   // Load result into register
					ld_cc = 1'b1;    // Update condition codes
				end
			and_op :
				begin
					aluk = 2'b01;	 // ALUK = 01 for AND
					dr = ir[11:9]; // DR = IR[11:9]
					sr1 = ir[8:6]; // SR1 = IR[8:6]
					if (ir[5]) begin
						sr2mux = 1'b1;
					end else begin
						sr2mux = 1'b0;
						sr2 = ir[2:0]; // SR2 = IR[2:0]
					end
					gate_alu = 1'b1; // Put ALU result on bus
					ld_reg = 1'b1;   // Load result into register
					ld_cc = 1'b1;    // Update condition codes
				end
			not_op :
				begin
					aluk = 2'b10;	 // ALUK = 10 for NOT
					dr = ir[11:9]; // DR = IR[11:9]
					sr1 = ir[8:6]; // SR1 = IR[8:6]
					gate_alu = 1'b1; // Put ALU result on bus
					ld_reg = 1'b1;   // Load result into register
					ld_cc = 1'b1;    // Update condition codes
				end
			ldr_op1:
				begin
					ld_mar = 1'b1;
					adder2mux = 2'b01;
					gate_mar = 1'b1;
					sr1 = ir[8:6];
					adder1mux = 1'b1;
				end
			ldr_op2_1, ldr_op2_2, ldr_op2_3:
				begin
					mem_mem_ena = 1'b1;
					ld_mdr = 1'b1;
				end
			ldr_op3:
				begin
					ld_reg = 1'b1;
					ld_cc = 1'b1;
					gate_mdr = 1'b1;
					dr = ir[11:9]; // DR = IR[11:9]
				end

			str_op1:
				begin
					ld_mar = 1'b1;
					adder2mux = 2'b01;
					gate_mar = 1'b1;
					sr1 = ir[8:6];
					adder1mux = 1'b1;
				end
			str_op2:
				begin
					aluk = 2'b11; // PASSA
					sr1 = ir[11:9]; // SR1 = IR[11:9]
					gate_alu = 1'b1;
					ld_mdr = 1'b1;
					mio_en = 1'b1;
				end
			str_op3_1, str_op3_2, str_op3_3:
				begin
					mem_mem_ena = 1'b1;
					mem_wr_ena = 1'b1;
				end
			jsr_op1:
				begin
					gate_pc = 1'b1;
					ld_reg = 1'b1;
					dr = 3'b111; // R7
				end
			jsr_op2:
				begin
					ld_pc = 1'b1;
					adder1mux = 1'b0;
					adder2mux = 2'b11;
					pcmux = 2'b10;
				end
			jmp_op:
				begin
					ld_pc = 1'b1;
					pcmux = 2'b01;
					sr1 = ir[8:6];
					aluk = 2'b11; // PASSA
					gate_alu = 1'b1;
				end
			br_op:
				begin
					ld_pc = 1'b0;
					adder1mux = 1'b0;
					adder2mux = 2'b00;
					pcmux = 2'b00;	
					if (ben) begin
						ld_pc = 1'b1;
						adder1mux = 1'b0;
						adder2mux = 2'b10;
						pcmux = 2'b10;
					end

				end

			pause_ir1: ld_led = 1'b1;
			pause_ir2: ld_led = 1'b1; 

			default : ;
		endcase
	end 

	//Fetch state machine next state logic
	always_comb
	begin
		// default next state is staying at current state
		state_nxt = state;

		unique case (state)
			halted : 
				if (run_i) 
					state_nxt = s_18;
			s_18 : 
				state_nxt = s_33_1; //notice that we usually have 'r' here, but you will need to add extra states instead 
			s_33_1 :                 //e.g. s_33_2, etc. how many? as a hint, note that the bram is synchronous, in addition, 
				state_nxt = s_33_2;   //it has an additional output register. 
			s_33_2 :
				state_nxt = s_33_3;
			s_33_3 : 
				state_nxt = s_35;
			s_35 : 
				state_nxt = s_32;
			s_32 :
				state_nxt = decode_state;
			add_op, and_op, not_op :
				state_nxt = s_18;
			ldr_op1 :
				state_nxt = ldr_op2_1;
			ldr_op2_1 :
				state_nxt = ldr_op2_2;
			ldr_op2_2 :
				state_nxt = ldr_op2_3;
			ldr_op2_3 :
				state_nxt = ldr_op3;	
			ldr_op3 :
				state_nxt = s_18;
			// pause_ir1 and pause_ir2 are only for week 1 such that TAs can see 
			// the values in ir.
			pause_ir1 : 
				if (continue_i) 
					state_nxt = pause_ir2;
			pause_ir2 : 
				if (~continue_i)
					state_nxt = s_18;
			str_op1 :
				state_nxt = str_op2;
			str_op2 :
				state_nxt = str_op3_1;
			str_op3_1 :
				state_nxt = str_op3_2;
			str_op3_2 :
				state_nxt = str_op3_3;
			str_op3_3 :
				state_nxt = s_18;
			jsr_op1 :
				state_nxt = jsr_op2;
			jsr_op2 :
				state_nxt = s_18;
			jmp_op :
				state_nxt = s_18;
			br_op :
				state_nxt = s_18;

			
			default :;
		endcase
	end




	
endmodule
