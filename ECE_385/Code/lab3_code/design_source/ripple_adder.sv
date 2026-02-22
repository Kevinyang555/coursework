module ripple_adder (
	input  logic  [15:0] a, 
    input  logic  [15:0] b,
	input  logic         cin,
	
	output logic  [15:0] s,
	output logic         cout
);

	logic [16:0] c;   // carry vector

    always_comb begin
        c[0]= cin;   // initial carry-in
        for (int i= 0; i <16; i++) begin
            s[i]= a[i]^b[i]^c[i];              // sum 
            c[i+1]=(a[i]&b[i]) | (c[i]&(a[i]^b[i]));  // carry out
        end
        cout = c[16];  // final carryout
    end

endmodule
