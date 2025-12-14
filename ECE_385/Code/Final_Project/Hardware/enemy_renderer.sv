//enemy_renderer.sv
//Handles enemy sprite detection and ROM address calculation.
//Supports 5 enemies with flip, hit, and attacking states.

module enemy_renderer (
    input  logic [9:0]  DrawX,         //Current pixel X
    input  logic [9:0]  DrawY,         //Current pixel Y

    //Enemy 0
    input  logic [9:0]  enemy0_x,
    input  logic [9:0]  enemy0_y,
    input  logic [2:0]  enemy0_frame,
    input  logic        enemy0_active,
    input  logic        enemy0_flip,
    input  logic        enemy0_attacking,
    input  logic        enemy0_hit,
    input  logic [3:0]  enemy0_type,

    //Enemy 1
    input  logic [9:0]  enemy1_x,
    input  logic [9:0]  enemy1_y,
    input  logic [2:0]  enemy1_frame,
    input  logic        enemy1_active,
    input  logic        enemy1_flip,
    input  logic        enemy1_attacking,
    input  logic        enemy1_hit,
    input  logic [3:0]  enemy1_type,

    //Enemy 2
    input  logic [9:0]  enemy2_x,
    input  logic [9:0]  enemy2_y,
    input  logic [2:0]  enemy2_frame,
    input  logic        enemy2_active,
    input  logic        enemy2_flip,
    input  logic        enemy2_attacking,
    input  logic        enemy2_hit,
    input  logic [3:0]  enemy2_type,

    //Enemy 3
    input  logic [9:0]  enemy3_x,
    input  logic [9:0]  enemy3_y,
    input  logic [2:0]  enemy3_frame,
    input  logic        enemy3_active,
    input  logic        enemy3_flip,
    input  logic        enemy3_attacking,
    input  logic        enemy3_hit,
    input  logic [3:0]  enemy3_type,

    //Enemy 4
    input  logic [9:0]  enemy4_x,
    input  logic [9:0]  enemy4_y,
    input  logic [2:0]  enemy4_frame,
    input  logic        enemy4_active,
    input  logic        enemy4_flip,
    input  logic        enemy4_attacking,
    input  logic        enemy4_hit,
    input  logic [3:0]  enemy4_type,

    //Projectile fallback address 
    input  logic [15:0] projectile_addr,

    //Outputs
    output logic        enemy_on,             //Any enemy is on this pixel
    output logic        enemy0_on, enemy1_on, enemy2_on, enemy3_on, enemy4_on,
    output logic [15:0] enemy_rom_addr,       //Enemy ROM address
    output logic [2:0]  enemy_which,          //Which enemy (5 = projectile)
    output logic        enemy_attacking_current,
    output logic        enemy_hit_current
);

    //Enemy sprite constants
    localparam ENEMY_SPRITE_SIZE = 32;

    //Check if current pixel is inside each enemy's sprite
    logic [4:0] enemy0_pixel_x, enemy0_pixel_y;
    logic [4:0] enemy1_pixel_x, enemy1_pixel_y;
    logic [4:0] enemy2_pixel_x, enemy2_pixel_y;
    logic [4:0] enemy3_pixel_x, enemy3_pixel_y;
    logic [4:0] enemy4_pixel_x, enemy4_pixel_y;

    always_comb begin
        enemy0_on = enemy0_active &&
                    (DrawX >= enemy0_x) &&
                    (DrawX < enemy0_x + ENEMY_SPRITE_SIZE) &&
                    (DrawY >= enemy0_y) &&
                    (DrawY < enemy0_y + ENEMY_SPRITE_SIZE);

        enemy1_on = enemy1_active &&
                    (DrawX >= enemy1_x) &&
                    (DrawX < enemy1_x + ENEMY_SPRITE_SIZE) &&
                    (DrawY >= enemy1_y) &&
                    (DrawY < enemy1_y + ENEMY_SPRITE_SIZE);

        enemy2_on = enemy2_active &&
                    (DrawX >= enemy2_x) &&
                    (DrawX < enemy2_x + ENEMY_SPRITE_SIZE) &&
                    (DrawY >= enemy2_y) &&
                    (DrawY < enemy2_y + ENEMY_SPRITE_SIZE);

        enemy3_on = enemy3_active &&
                    (DrawX >= enemy3_x) &&
                    (DrawX < enemy3_x + ENEMY_SPRITE_SIZE) &&
                    (DrawY >= enemy3_y) &&
                    (DrawY < enemy3_y + ENEMY_SPRITE_SIZE);

        enemy4_on = enemy4_active &&
                    (DrawX >= enemy4_x) &&
                    (DrawX < enemy4_x + ENEMY_SPRITE_SIZE) &&
                    (DrawY >= enemy4_y) &&
                    (DrawY < enemy4_y + ENEMY_SPRITE_SIZE);

        enemy0_pixel_x = DrawX - enemy0_x;
        enemy0_pixel_y = DrawY - enemy0_y;
        enemy1_pixel_x = DrawX - enemy1_x;
        enemy1_pixel_y = DrawY - enemy1_y;
        enemy2_pixel_x = DrawX - enemy2_x;
        enemy2_pixel_y = DrawY - enemy2_y;
        enemy3_pixel_x = DrawX - enemy3_x;
        enemy3_pixel_y = DrawY - enemy3_y;
        enemy4_pixel_x = DrawX - enemy4_x;
        enemy4_pixel_y = DrawY - enemy4_y;
    end

    //Any enemy on screen
    assign enemy_on = enemy0_on || enemy1_on || enemy2_on || enemy3_on || enemy4_on;

    //Enemy ROM address calculation with flip support
    logic [4:0] enemy0_pixel_x_final, enemy1_pixel_x_final;
    logic [4:0] enemy2_pixel_x_final, enemy3_pixel_x_final, enemy4_pixel_x_final;
    logic [15:0] enemy0_addr, enemy1_addr, enemy2_addr, enemy3_addr, enemy4_addr;

    always_comb begin
        //Apply horizontal flip for all enemies
        enemy0_pixel_x_final = enemy0_flip ? (5'd31 - enemy0_pixel_x) : enemy0_pixel_x;
        enemy1_pixel_x_final = enemy1_flip ? (5'd31 - enemy1_pixel_x) : enemy1_pixel_x;
        enemy2_pixel_x_final = enemy2_flip ? (5'd31 - enemy2_pixel_x) : enemy2_pixel_x;
        enemy3_pixel_x_final = enemy3_flip ? (5'd31 - enemy3_pixel_x) : enemy3_pixel_x;
        enemy4_pixel_x_final = enemy4_flip ? (5'd31 - enemy4_pixel_x) : enemy4_pixel_x;

        //Calculate addresses: {enemy_type[2:0], frame[2:0], y[4:0], x[4:0]}
        enemy0_addr = {enemy0_type[2:0], enemy0_frame, enemy0_pixel_y, enemy0_pixel_x_final};
        enemy1_addr = {enemy1_type[2:0], enemy1_frame, enemy1_pixel_y, enemy1_pixel_x_final};
        enemy2_addr = {enemy2_type[2:0], enemy2_frame, enemy2_pixel_y, enemy2_pixel_x_final};
        enemy3_addr = {enemy3_type[2:0], enemy3_frame, enemy3_pixel_y, enemy3_pixel_x_final};
        enemy4_addr = {enemy4_type[2:0], enemy4_frame, enemy4_pixel_y, enemy4_pixel_x_final};

        //Priority: Enemy0 > Enemy1 > Enemy2 > Enemy3 > Enemy4 > Projectile
        if (enemy0_on) begin
            enemy_rom_addr = enemy0_addr;
            enemy_which = 3'd0;
            enemy_attacking_current = enemy0_attacking;
            enemy_hit_current = enemy0_hit;
        end else if (enemy1_on) begin
            enemy_rom_addr = enemy1_addr;
            enemy_which = 3'd1;
            enemy_attacking_current = enemy1_attacking;
            enemy_hit_current = enemy1_hit;
        end else if (enemy2_on) begin
            enemy_rom_addr = enemy2_addr;
            enemy_which = 3'd2;
            enemy_attacking_current = enemy2_attacking;
            enemy_hit_current = enemy2_hit;
        end else if (enemy3_on) begin
            enemy_rom_addr = enemy3_addr;
            enemy_which = 3'd3;
            enemy_attacking_current = enemy3_attacking;
            enemy_hit_current = enemy3_hit;
        end else if (enemy4_on) begin
            enemy_rom_addr = enemy4_addr;
            enemy_which = 3'd4;
            enemy_attacking_current = enemy4_attacking;
            enemy_hit_current = enemy4_hit;
        end else begin
            //No enemy on screen - use projectile address
            enemy_rom_addr = projectile_addr;
            enemy_which = 3'd5;  //Indicates projectile
            enemy_attacking_current = 1'b0;
            enemy_hit_current = 1'b0;
        end
    end

endmodule
