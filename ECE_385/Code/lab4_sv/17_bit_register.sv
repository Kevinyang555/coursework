module register_17_bit(
    // Clock and control signals
    input  logic        clk,
    input  logic        reset,
    input  logic        shift,
    input  logic        add,
    input  logic        sub,
    input  logic [7:0]  d_in,
    input  logic [8:0]  d_out,
    input  logic        clearXA,
    
    // Output signals
    output logic        M,
    output logic [16:0] data
);

    // Internal state signals
    logic [16:0] next;
    logic [16:0] curr;
    
    // Control signal combinations
    logic add_sub;
    logic shift_op;
    
    // Combine control signals
    assign add_sub = add | sub;
    assign shift_op = shift & ~reset;
    
    // Next state generation with reorganized priority
    always_comb begin
        // Start with default assignment
        next = curr;
        
        // Priority-based state transitions
        case (1'b1)
            shift_op: begin
                next = {curr[16], curr[16:1]};
            end
            
            add_sub: begin
                next = {d_out, curr[7:0]};
            end
            
            reset: begin
                next = curr;
            end
            
            default: begin
                next = curr;
            end
        endcase
    end
    
    // Sequential state updates with different condition ordering
    always_ff @(posedge clk) begin
        if (reset) begin
            curr[16:8] <= 9'b000000000;
            curr[7:0] <= d_in;
        end
        else begin
            if (clearXA) begin
                curr[16:8] <= 9'h000;
                curr[7:0] <= curr[7:0]; 
            end
            else begin
                curr <= next;
            end
        end
    end
    
    assign M = next[0];      // LSB of next state
    assign data = curr;      // Current register contents

endmodule