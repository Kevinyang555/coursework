module select_adder (
    input  logic [15:0] a,
    input  logic [15:0] b,
    input  logic        cin,

    output logic [15:0] s,
    output logic        cout
);

    logic c4, c8, c12;
    //first 4
    ripple4 u0 (
        .a   (a[3:0]),
        .b   (b[3:0]),
        .cin (cin),
        .s   (s[3:0]),
        .cout(c4)
    );

   //[7:4]
    logic[3:0] s4_0,s4_1;
    logic c8_0,c8_1;

    ripple4 u1_c0 (.a(a[7:4]),.b(b[7:4]),.cin(1'b0),.s(s4_0),.cout(c8_0));
    ripple4 u1_c1 (.a(a[7:4]),.b(b[7:4]),.cin(1'b1),.s(s4_1),.cout(c8_1));

    always_comb begin
        if(c4 == 1'b0) begin
            s[7:4]= s4_0;
            c8= c8_0;
        end else begin
            s[7:4]= s4_1;
            c8= c8_1;
        end
    end

   //[11:8]
    logic [3:0] s8_0,s8_1;
    logic c12_0,c12_1;

    ripple4 u2_c0 (.a(a[11:8]),.b(b[11:8]),.cin(1'b0),.s(s8_0),.cout(c12_0));
    ripple4 u2_c1 (.a(a[11:8]),.b(b[11:8]),.cin(1'b1),.s(s8_1),.cout(c12_1));

    always_comb begin
        if(c8 == 1'b0) begin
            s[11:8]= s8_0;
            c12= c12_0;
        end else begin
            s[11:8]= s8_1;
            c12= c12_1;
        end
    end

    //[15:12]
    logic [3:0] s12_0,s12_1;
    logic c16_0,c16_1;

    ripple4 u3_c0 (.a(a[15:12]),.b(b[15:12]),.cin(1'b0),.s(s12_0),.cout(c16_0));
    ripple4 u3_c1 (.a(a[15:12]),.b(b[15:12]),.cin(1'b1),.s(s12_1),.cout(c16_1));

    always_comb begin
        if(c12 == 1'b0) begin
            s[15:12]= s12_0;
            cout= c16_0;
        end else begin
            s[15:12]= s12_1;
            cout= c16_1;
        end
    end

endmodule


//4-bit ripple carry adder
module ripple4 (
    input  logic [3:0] a,
    input  logic [3:0] b,
    input  logic cin,
    output logic [3:0] s,
    output logic cout
);
    logic [4:0] c;
    always_comb begin
        c[0]= cin;
        for(int i= 0; i < 4; i++) begin
            s[i]= a[i] ^b[i]^ c[i];
            c[i+1]=(a[i]&b[i]) | (c[i]&(a[i] ^b[i]));
        end
        cout= c[4];
    end
endmodule