module processor_top (
    input  logic        Clk,     
    input  logic        reset_load_clear,   
    input  logic        Run,   
    input  logic [7:0]  SW_i,       

    output logic        Xval,    
    output logic [7:0]  Aval,   
    output logic [7:0]  Bval,   
    output logic [7:0]  hex_seg, 
    output logic [3:0]  hex_grid 
);
    logic load;        
    logic Shift;      
    logic Add;        
    logic Sub;         
    logic clear_ax;    
    logic [8:0] result;
    logic       m;            
    logic [7:0] SW_s;        
    logic       reset_load_clear_S; 
    logic       Run_S;
    logic [16:0] XAB;
 
    assign Aval = XAB[15:8];
    assign Bval = XAB[7:0];
    assign Xval = XAB[16];

    control_unit control_part (
        .Clk (Clk),
        .reset_load_clear (reset_load_clear_S),
        .Run (Run_S),
        .M (m),

        .load (load),
        .Shift (Shift),
        .Add (Add),
        .Sub (Sub),
        .clear_ax (clear_ax)
    );
    register_17_bit reg_unit (
        .clk (Clk),
        .reset (load),
        .shift (Shift),
        .add (Add),
        .sub (Sub),
        .d_in (SW_s),
        .d_out (result),
        .clearXA (clear_ax),
        .M (m),
        .data (XAB)
    );
    adder_9_bit adder_unit (
        .A (Aval),
        .B (SW_s),
        .sub (Sub),        
        .S (result)
    );
    
    HexDriver HexA (
        .Clk        (Clk),
        .reset      (reset_load_clear_S),
        .in         ({Aval[7:4], Aval[3:0], Bval[7:4], Bval[3:0]}),
        .hex_seg    (hex_seg),
        .hex_grid   (hex_grid)
    );
    
    // Synchronizers for switches
    sync_debounce Din_sync [7:0] (
        .clk  (Clk), 
        .d    (SW_i), 
        .q    (SW_s) 
    );
    
    // Synchronizers for buttons
    sync_debounce button_sync [1:0] (
        .clk  (Clk),
        .d    ({reset_load_clear, Run}),
        .q    ({reset_load_clear_S, Run_S})
    );         

endmodule
