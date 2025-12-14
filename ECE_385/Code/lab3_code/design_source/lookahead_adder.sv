module lookahead_adder (
	input  logic  [15:0] a, 
    input  logic  [15:0] b,
	input  logic         cin,
		
	output logic  [15:0] s,
	output logic         cout
);
    
    logic PG0, PG4, PG8, PG12;
    logic GG0, GG4, GG8, GG12;

    logic C4, C8, C12, C16;

    // 4-bit CLA instances for each 4-bit slice
    cla4 u_cla0 (
        .a    (a[3:0]),
        .b    (b[3:0]),
        .cin  (cin),
        .s    (s[3:0]),
        .cout (),            // not used (equal to C4 by construction)
        .PG   (PG0),
        .GG   (GG0)
    );


    cla4 u_cla4 (
        .a    (a[7:4]),
        .b    (b[7:4]),
        .cin  (C4),
        .s    (s[7:4]),
        .cout (),
        .PG   (PG4),
        .GG   (GG4)
    );

    cla4 u_cla8 (
        .a    (a[11:8]),
        .b    (b[11:8]),
        .cin  (C8),
        .s    (s[11:8]),
        .cout (),
        .PG   (PG8),
        .GG   (GG8)
    );

    cla4 u_cla12 (
        .a    (a[15:12]),
        .b    (b[15:12]),
        .cin  (C12),
        .s    (s[15:12]),
        .cout (),
        .PG   (PG12),
        .GG   (GG12)
    );

    // Group-level CLU computes C4/C8/C12/C16 from {PG,GG} and cin
    clu4 u_clu (
        .PG0  (PG0),  .GG0  (GG0),
        .PG4  (PG4),  .GG4  (GG4),
        .PG8  (PG8),  .GG8  (GG8),
        .PG12 (PG12), .GG12 (GG12),
        .cin  (cin),
        .C4   (C4),
        .C8   (C8),
        .C12  (C12),
        .C16  (C16)
    );

    assign cout = C16;

endmodule


// 4-bit CLA
module cla4 (
    input  logic [3:0] a,
    input  logic [3:0] b,
    input  logic cin,
    output logic [3:0] s,
    output logic cout,
    output logic PG,
    output logic GG
);
    logic [3:0] p, g;     
    logic [4:0] c;        

    assign p= a ^ b;
    assign g= a &b;

    assign c[0]= cin;

    // Bit carries using lookahead
    assign c[1]= g[0] | (p[0]&c[0]);
    assign c[2]= g[1] | (p[1]&g[0]) | (p[1]&p[0]&c[0]);
    assign c[3]= g[2] | (p[2]&g[1]) | (p[2]& p[1]&g[0]) | (p[2]&p[1]&p[0]& c[0]);
    assign c[4]= g[3] | (p[3]&g[2]) | (p[3]&p[2]&g[1]) | (p[3]&p[2]&p[1]&g[0]) | (p[3]&p[2] &p[1]& p[0]&c[0]);

    // Sums
    assign s[0]= p[0]^c[0];
    assign s[1]= p[1]^c[1];
    assign s[2]= p[2]^c[2];
    assign s[3]= p[3]^c[3];

    // Group propagate and generate
    assign PG= p[3]&p[2]&p[1]&p[0];
    assign GG= g[3] | (p[3]&g[2]) | (p[3]&p[2]&g[1]) | (p[3]&p[2]&p[1]& g[0]);

    assign cout= c[4];
endmodule


// 4-input CLU
module clu4 (
    input  logic PG0,  input logic GG0,
    input  logic PG4,  input logic GG4,
    input  logic PG8,  input logic GG8,
    input  logic PG12, input logic GG12,
    input  logic cin,
    output logic C4,
    output logic C8,
    output logic C12,
    output logic C16
);
    // carry outs for next block of cla4
    assign C4= GG0 | (PG0 &cin);
    assign C8= GG4 | (PG4 &GG0) | (PG4 &PG0 &cin);
    assign C12= GG8 | (PG8 &GG4) | (PG8 &PG4 &GG0) | (PG8 &PG4 &PG0 &cin);
    assign C16= GG12 | (PG12 &GG8) | (PG12 &PG8& GG4)| (PG12&PG8 &PG4 &GG0) | (PG12 &PG8 &PG4 &PG0 &cin);
endmodule
