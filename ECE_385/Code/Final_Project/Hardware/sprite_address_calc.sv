//sprite_address_calc.sv
//Calculates ROM addresses for player, boss, and enemy sprites.
//Handles dual-size player sprites (32x32 movement, 48x48 attack).

module sprite_address_calc (
    input  logic [9:0]  DrawX,
    input  logic [9:0]  DrawY,

    //Player sprite inputs
    input  logic [9:0]  player_x,
    input  logic [9:0]  player_y,
    input  logic [6:0]  player_frame,

    //Boss sprite inputs
    input  logic [9:0]  boss_x,
    input  logic [9:0]  boss_y,
    input  logic [4:0]  boss_frame,
    input  logic        boss_active,
    input  logic        boss_flip,

    //Player sprite outputs
    output logic        player_on,
    output logic [16:0] sprite_rom_addr,
    output logic        is_attack_sprite,

    //Boss sprite outputs
    output logic        boss_on,
    output logic [16:0] boss_rom_addr
);

    //Sprite size constants
    localparam MOVE_SPRITE_SIZE = 32;
    localparam ATTACK_SPRITE_SIZE = 48;
    localparam ATTACK_FRAME_START = 32;
    localparam ATTACK_CENTER_OFFSET = 8;
    localparam ATTACK_BASE_ADDR = 32768;
    localparam BOSS_SPRITE_SIZE = 64;

    //Player Sprite - Dual Size Support
    assign is_attack_sprite = (player_frame >= ATTACK_FRAME_START);

    logic [9:0] sprite_draw_x, sprite_draw_y;
    logic [5:0] current_sprite_size;

    always_comb begin
        if (is_attack_sprite) begin
            sprite_draw_x = (player_x >= ATTACK_CENTER_OFFSET) ?
                            (player_x - ATTACK_CENTER_OFFSET) : 10'd0;
            sprite_draw_y = (player_y >= ATTACK_CENTER_OFFSET) ?
                            (player_y - ATTACK_CENTER_OFFSET) : 10'd0;
            current_sprite_size = ATTACK_SPRITE_SIZE;
        end else begin
            sprite_draw_x = player_x;
            sprite_draw_y = player_y;
            current_sprite_size = MOVE_SPRITE_SIZE;
        end
    end

    logic [5:0] sprite_pixel_x, sprite_pixel_y;

    always_comb begin
        player_on = (DrawX >= sprite_draw_x) &&
                    (DrawX < sprite_draw_x + current_sprite_size) &&
                    (DrawY >= sprite_draw_y) &&
                    (DrawY < sprite_draw_y + current_sprite_size);

        sprite_pixel_x = DrawX - sprite_draw_x;
        sprite_pixel_y = DrawY - sprite_draw_y;
    end

    //Player Sprite ROM Address Calculation
    logic [16:0] move_sprite_addr;
    logic [16:0] attack_sprite_addr;
    logic [4:0] attack_frame_offset;
    logic [16:0] attack_frame_base;
    logic [11:0] attack_row_offset;

    always_comb begin
        move_sprite_addr = {player_frame[5:0], sprite_pixel_y[4:0], sprite_pixel_x[4:0]};

        attack_frame_offset = player_frame - 7'd64;
        attack_frame_base = ({12'b0, attack_frame_offset} << 11) + ({12'b0, attack_frame_offset} << 8);
        attack_row_offset = ({6'b0, sprite_pixel_y} << 5) + ({6'b0, sprite_pixel_y} << 4);
        attack_sprite_addr = ATTACK_BASE_ADDR + attack_frame_base + {5'b0, attack_row_offset} + {11'b0, sprite_pixel_x};

        sprite_rom_addr = is_attack_sprite ? attack_sprite_addr : move_sprite_addr;
    end

    //Boss Sprite - 64x64
    logic [5:0] boss_pixel_x, boss_pixel_y;
    logic [5:0] boss_pixel_x_final;

    always_comb begin
        boss_on = boss_active &&
                  (DrawX >= boss_x) &&
                  (DrawX < boss_x + BOSS_SPRITE_SIZE) &&
                  (DrawY >= boss_y) &&
                  (DrawY < boss_y + BOSS_SPRITE_SIZE);

        boss_pixel_x = DrawX - boss_x;
        boss_pixel_y = DrawY - boss_y;
        boss_pixel_x_final = boss_flip ? (6'd63 - boss_pixel_x) : boss_pixel_x;
    end

    assign boss_rom_addr = {boss_frame, boss_pixel_y, boss_pixel_x_final};

endmodule
