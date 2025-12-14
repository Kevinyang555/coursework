`timescale 1ns/1ps

module lookahead_adder_tb;
    // DUT I/O
    logic [15:0] a, b;
    logic        cin;
    logic [15:0] s;
    logic        cout;

    // Device Under Test
    lookahead_adder dut (
        .a   (a),
        .b   (b),
        .cin (cin),
        .s   (s),
        .cout(cout)
    );

    // Simple check task
    task automatic check(
        input logic [15:0] ta,
        input logic [15:0] tb,
        input logic        tcin,
        input logic [15:0] exp_s,
        input logic        exp_cout,
        input string       name
    );
        begin
            a   = ta;
            b   = tb;
            cin = tcin;
            #1; // allow combinational logic to settle
            if (s !== exp_s || cout !== exp_cout) begin
                $error("%s FAILED: a=%h b=%h cin=%b -> s=%h cout=%b (expected s=%h cout=%b)",
                       name, ta, tb, tcin, s, cout, exp_s, exp_cout);
                $fatal;
            end else begin
                $display("%s PASS: a=%h b=%h cin=%b -> s=%h cout=%b",
                         name, ta, tb, tcin, s, cout);
            end
        end
    endtask

    initial begin
        $display("Starting lookahead_adder_tb");
        // Two additions are sufficient per requirements
        check(16'h0003, 16'h0004, 1'b0, 16'h0007, 1'b0, "3 + 4");
        check(16'hFFFF, 16'h0001, 1'b0, 16'h0000, 1'b1, "FFFF + 1 (carry)");
        $display("All tests passed.");
        $finish;
    end
endmodule

