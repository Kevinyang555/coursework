module ripple_adder_9_bits(
        input  logic  [8:0] a, 
        input  logic  [8:0] b,
	input  logic  cin,
	output logic  [8:0] s
);
        logic [9:0] c;

        always_comb begin
            c[0]= cin;   // initial carry-in
            for (int i= 0; i <9; i++) begin
                s[i]= a[i]^b[i]^c[i];              // sum 
                c[i+1]=(a[i]&b[i]) | (c[i]&(a[i]^b[i]));  // carry out
            end
        end
              
endmodule