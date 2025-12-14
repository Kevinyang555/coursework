module adder_9_bit (
    input  logic [7:0] A,
    input  logic [7:0] B,
    input  logic sub,  // 1 for subtract, 0 for add
    output logic [8:0] S
);

    logic [8:0] extended_A;
    logic [7:0] temp_B;
    logic [8:0] extended_B;

    always_comb begin
        extended_A = {A[7], A[7:0]};
        temp_B = B ^ {8{sub}};
        extended_B = {temp_B[7], temp_B[7:0]};
    end
    
    ripple_adder_9_bits adder (
        .a(extended_A),
        .b(extended_B),
        .cin(sub), // 2's complement + 1
        .s(S)
    );

endmodule