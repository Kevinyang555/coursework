//projectile_renderer.sv
//Handles projectile detection and ROM address calculation.
//Supports 16 projectiles (player and enemy).

module projectile_renderer (
    input  logic [9:0]  DrawX,         
    input  logic [9:0]  DrawY,         

    //Projectile data 
    input  logic [31:0] proj_0,
    input  logic [31:0] proj_1,
    input  logic [31:0] proj_2,
    input  logic [31:0] proj_3,
    input  logic [31:0] proj_4,
    input  logic [31:0] proj_5,
    input  logic [31:0] proj_6,
    input  logic [31:0] proj_7,
    input  logic [31:0] proj_8,
    input  logic [31:0] proj_9,
    input  logic [31:0] proj_10,
    input  logic [31:0] proj_11,
    input  logic [31:0] proj_12,
    input  logic [31:0] proj_13,
    input  logic [31:0] proj_14,
    input  logic [31:0] proj_15,

    //Player projectile 
    input  logic [1:0]  player_proj_frame,

    //Outputs
    output logic        proj_on,             
    output logic [5:0]  proj_pixel_x,         
    output logic [4:0]  proj_pixel_y,        
    output logic [3:0]  proj_which,          
    output logic        proj_current_is_player, 
    output logic        proj_current_flip,    
    output logic [15:0] projectile_addr,     
    output logic [9:0]  shuriken_rom_addr     
);

    //Projectile constants
    localparam PROJECTILE_SIZE = 16;
    localparam PROJECTILE_BASE_ADDR = 16'd49152;  //After 6 enemies × 8192 pixels each

    
    //Format: {active[31], is_player[30], flip[29], unused[28:26], y[25:16], unused[15:10], x[9:0]}
    logic [9:0] proj_x [0:15];
    logic [9:0] proj_y [0:15];
    logic       proj_active [0:15];
    logic       proj_is_player [0:15];
    logic       proj_flip [0:15];

    always_comb begin
        proj_x[0] = proj_0[9:0];
        proj_y[0] = proj_0[25:16];
        proj_active[0] = proj_0[31];
        proj_is_player[0] = proj_0[30];
        proj_flip[0] = proj_0[29];

        proj_x[1] = proj_1[9:0];
        proj_y[1] = proj_1[25:16];
        proj_active[1] = proj_1[31];
        proj_is_player[1] = proj_1[30];
        proj_flip[1] = proj_1[29];

        proj_x[2] = proj_2[9:0];
        proj_y[2] = proj_2[25:16];
        proj_active[2] = proj_2[31];
        proj_is_player[2] = proj_2[30];
        proj_flip[2] = proj_2[29];

        proj_x[3] = proj_3[9:0];
        proj_y[3] = proj_3[25:16];
        proj_active[3] = proj_3[31];
        proj_is_player[3] = proj_3[30];
        proj_flip[3] = proj_3[29];

        proj_x[4] = proj_4[9:0];
        proj_y[4] = proj_4[25:16];
        proj_active[4] = proj_4[31];
        proj_is_player[4] = proj_4[30];
        proj_flip[4] = proj_4[29];

        proj_x[5] = proj_5[9:0];
        proj_y[5] = proj_5[25:16];
        proj_active[5] = proj_5[31];
        proj_is_player[5] = proj_5[30];
        proj_flip[5] = proj_5[29];

        proj_x[6] = proj_6[9:0];
        proj_y[6] = proj_6[25:16];
        proj_active[6] = proj_6[31];
        proj_is_player[6] = proj_6[30];
        proj_flip[6] = proj_6[29];

        proj_x[7] = proj_7[9:0];
        proj_y[7] = proj_7[25:16];
        proj_active[7] = proj_7[31];
        proj_is_player[7] = proj_7[30];
        proj_flip[7] = proj_7[29];

        proj_x[8] = proj_8[9:0];
        proj_y[8] = proj_8[25:16];
        proj_active[8] = proj_8[31];
        proj_is_player[8] = proj_8[30];
        proj_flip[8] = proj_8[29];

        proj_x[9] = proj_9[9:0];
        proj_y[9] = proj_9[25:16];
        proj_active[9] = proj_9[31];
        proj_is_player[9] = proj_9[30];
        proj_flip[9] = proj_9[29];

        proj_x[10] = proj_10[9:0];
        proj_y[10] = proj_10[25:16];
        proj_active[10] = proj_10[31];
        proj_is_player[10] = proj_10[30];
        proj_flip[10] = proj_10[29];

        proj_x[11] = proj_11[9:0];
        proj_y[11] = proj_11[25:16];
        proj_active[11] = proj_11[31];
        proj_is_player[11] = proj_11[30];
        proj_flip[11] = proj_11[29];

        proj_x[12] = proj_12[9:0];
        proj_y[12] = proj_12[25:16];
        proj_active[12] = proj_12[31];
        proj_is_player[12] = proj_12[30];
        proj_flip[12] = proj_12[29];

        proj_x[13] = proj_13[9:0];
        proj_y[13] = proj_13[25:16];
        proj_active[13] = proj_13[31];
        proj_is_player[13] = proj_13[30];
        proj_flip[13] = proj_13[29];

        proj_x[14] = proj_14[9:0];
        proj_y[14] = proj_14[25:16];
        proj_active[14] = proj_14[31];
        proj_is_player[14] = proj_14[30];
        proj_flip[14] = proj_14[29];

        proj_x[15] = proj_15[9:0];
        proj_y[15] = proj_15[25:16];
        proj_active[15] = proj_15[31];
        proj_is_player[15] = proj_15[30];
        proj_flip[15] = proj_15[29];
    end

    //Check if current pixel is inside any projectile
    always_comb begin
        proj_on = 1'b0;
        proj_pixel_x = 6'd0;
        proj_pixel_y = 5'd0;
        proj_which = 4'd0;
        proj_current_is_player = 1'b0;
        proj_current_flip = 1'b0;

        //Check each projectile (priority: 0 > 1 > ... > 15)
        for (int i = 0; i < 16; i++) begin
            if (!proj_on && proj_active[i]) begin
                if (DrawX >= proj_x[i] &&
                    DrawX < proj_x[i] + PROJECTILE_SIZE &&
                    DrawY >= proj_y[i] &&
                    DrawY < proj_y[i] + PROJECTILE_SIZE) begin
                    proj_on = 1'b1;
                    proj_pixel_x = DrawX - proj_x[i];
                    proj_pixel_y = DrawY - proj_y[i];
                    proj_which = i[3:0];
                    proj_current_is_player = proj_is_player[i];
                    proj_current_flip = proj_flip[i];
                end
            end
        end
    end

    //Enemy projectile ROM address
    assign projectile_addr = PROJECTILE_BASE_ADDR + {8'b0, proj_pixel_y[3:0], proj_pixel_x[3:0]};

    //Shuriken ROM address 
    assign shuriken_rom_addr = {player_proj_frame, proj_pixel_y[3:0], proj_pixel_x[3:0]};

endmodule
