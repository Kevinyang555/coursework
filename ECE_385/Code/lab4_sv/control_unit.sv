module control_unit (
    input  logic Clk,
    input  logic reset_load_clear,   // Reset/LoadB/ClearA button 
    input  logic Run,                // Run button (sync)
    input  logic M,                  // current LSB of B reg

    output logic load,               // load B, clear A/X 
    output logic Shift,              // shift 
    output logic Add,                // add 
    output logic Sub,                // subtract 
    output logic clear_ax            // clear A & X once 
);

    enum logic [2:0] {
        ST_LOAD,        
        ST_WAIT,        
        ST_START,       
        ST_LOOP,        
        ST_FINAL        
    } curr_state, next_state;

    // 8-cycle counter
    logic [3:0] counter, next_counter;

    //phase for add/sub and shift
    logic phase, next_phase;

    always_comb begin
        // defaults
        load = 1'b0;
        Shift = 1'b0;
        Add = 1'b0;
        Sub = 1'b0;
        clear_ax = 1'b0;

        next_counter = counter;
        next_phase = phase;

        case (curr_state)
            ST_LOAD: begin
                load = 1'b1;
                next_counter = 4'd0;
                next_phase = 1'b0;   // start each run add/sub
            end

            ST_WAIT: begin
                next_counter = 4'd0;
                next_phase = 1'b0;
            end

            ST_START: begin
                clear_ax = 1'b1;
                next_counter = 4'd0;
                next_phase = 1'b0;   // first thing after START is add/sub
            end

            ST_LOOP: begin
                if(phase == 1'b0) begin //if add/sub phase
                    // OP phase
                    if(M) begin
                        if (counter == 4'd7) begin
                            Sub = 1'b1;   // final (MSB) 1 → subtract
                        end else begin
                            Add = 1'b1;   // other 1-bits → add
                        end
                    end
                    // move to SHIFT phase next
                    next_phase = 1'b1;
                end else begin
                    // SHIFT phase
                    Shift = 1'b1;
                    next_counter = counter + 4'd1;
                    next_phase = 1'b0;    
                end
            end

            ST_FINAL: begin
                next_counter = 4'd0;
                next_phase = 1'b0;
            end
        endcase
    end

    always_comb begin
        next_state = curr_state;

        unique case (curr_state)
            ST_LOAD: begin
                // Stay loading while button held; when released, go idle
                if(reset_load_clear) begin
                    next_state = ST_LOAD;
                end else begin         
                    next_state = ST_WAIT;
                end
            end

            ST_WAIT: begin
                if(reset_load_clear) begin
                    next_state = ST_LOAD;
                end 
                else if(Run) begin
                    next_state = ST_START;
                end 
                else begin
                    next_state = ST_WAIT;
                end
            end


            ST_START: begin
                next_state = ST_LOOP;
            end

            ST_LOOP: begin
                if(phase == 1'b1 && (counter + 4'd1) == 4'd8) begin
                    next_state = ST_FINAL;
                end 
                else begin
                    next_state = ST_LOOP;
                end
            end

            ST_FINAL: begin
                // Require Run release to re-arm
                if(~Run) begin
                    next_state = ST_WAIT;
                end 
                else begin
                    next_state = ST_FINAL;
                end

            end
        endcase
    end


    always_ff @(posedge Clk) begin
        if(reset_load_clear) begin
            curr_state <= ST_LOAD;
            counter <= 4'd0;
            phase <= 1'b0;
        end else begin
            curr_state <= next_state;
            counter <= next_counter;
            phase <= next_phase;
        end
    end

endmodule
