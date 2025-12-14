//tilemap_renderer.sv
//Main renderer module for UI, sprites, enemies, and projectiles.
module tilemap_renderer (
    input  logic        clk,          
    input  logic        reset,

    
    input  logic [9:0]  DrawX,       
    input  logic [9:0]  DrawY,       
    input  logic        blank,        // 1 = in blanking interval
    input  logic        hsync,        

    input  logic [7:0]  level_offset,

    input  logic [2:0]  map_select,

    //Breach open (door visible when set)
    input  logic        breach_open,

    //Breach direction: 0=RIGHT, 1=LEFT, 2=UP, 3=DOWN
    input  logic [1:0]  breach_direction,

    //Player position and frame
    input  logic [9:0]  player_x,
    input  logic [9:0]  player_y,
    input  logic [6:0]  player_frame,

    //Tile BRAM interface - 12-bit RGB444 per pixel
    output logic [13:0] tile_rom_addr,
    input  logic [11:0] tile_rom_data,

    //Player Sprite BRAM interface - 6-bit RGB222 per pixel
    output logic [16:0] sprite_rom_addr,
    input  logic [5:0]  sprite_rom_data,

    //Enemy 0-4 inputs
    input  logic [9:0]  enemy0_x, enemy0_y,
    input  logic [2:0]  enemy0_frame,
    input  logic        enemy0_active, enemy0_flip, enemy0_attacking, enemy0_hit,
    input  logic [3:0]  enemy0_type,

    input  logic [9:0]  enemy1_x, enemy1_y,
    input  logic [2:0]  enemy1_frame,
    input  logic        enemy1_active, enemy1_flip, enemy1_attacking, enemy1_hit,
    input  logic [3:0]  enemy1_type,

    input  logic [9:0]  enemy2_x, enemy2_y,
    input  logic [2:0]  enemy2_frame,
    input  logic        enemy2_active, enemy2_flip, enemy2_attacking, enemy2_hit,
    input  logic [3:0]  enemy2_type,

    input  logic [9:0]  enemy3_x, enemy3_y,
    input  logic [2:0]  enemy3_frame,
    input  logic        enemy3_active, enemy3_flip, enemy3_attacking, enemy3_hit,
    input  logic [3:0]  enemy3_type,

    input  logic [9:0]  enemy4_x, enemy4_y,
    input  logic [2:0]  enemy4_frame,
    input  logic        enemy4_active, enemy4_flip, enemy4_attacking, enemy4_hit,
    input  logic [3:0]  enemy4_type,

    //Enemy Sprite BRAM interface
    output logic [15:0] enemy_rom_addr,
    input  logic [5:0]  enemy_rom_data,

    //Projectiles 
    input  logic [31:0] proj_0, proj_1, proj_2, proj_3,
    input  logic [31:0] proj_4, proj_5, proj_6, proj_7,
    input  logic [31:0] proj_8, proj_9, proj_10, proj_11,
    input  logic [31:0] proj_12, proj_13, proj_14, proj_15,

    //Player projectile animation frame
    input  logic [1:0]  player_proj_frame,

    //Player health and armor
    input  logic [2:0]  player_health,
    input  logic [1:0]  player_armor,

    //Game state for UI rendering
    input  logic [1:0]  game_state,
    input  logic        menu_selection,

    //Boss inputs
    input  logic [9:0]  boss_x, boss_y,
    input  logic [4:0]  boss_frame,
    input  logic        boss_active, boss_flip, boss_hit,
    input  logic [7:0]  boss_health,

    //Boss Sprite BRAM interface
    output logic [16:0] boss_rom_addr,
    input  logic [5:0]  boss_rom_data,

    //Shuriken BRAM interface
    output logic [9:0]  shuriken_rom_addr,
    input  logic [11:0] shuriken_rom_data,

    //Output pixel
    output logic [3:0]  red_out,
    output logic [3:0]  green_out,
    output logic [3:0]  blue_out,
    output logic        pixel_valid,
    output logic        is_sprite_pixel
);

    //Screen and map dimensions
    localparam SCREEN_WIDTH_TILES = 40;
    localparam STATUS_BAR_WIDTH = 10;
    localparam MAP_WIDTH = 30;
    localparam MAP_HEIGHT = 30;

    //Transparency colors
    localparam TRANSPARENT_COLOR = 6'h33;
    localparam SHURIKEN_TRANSPARENT = 12'hF0F;

    //Game state constants
    localparam GAME_STATE_MENU = 2'd0;
    localparam GAME_STATE_PLAYING = 2'd1;
    localparam GAME_STATE_GAMEOVER = 2'd2;
    localparam GAME_STATE_WIN = 2'd3;

    //Tile indices
    localparam TILE_FLOOR = 8'd4;
    localparam TILE_OBSTACLE_ROCK = 8'd10;
    localparam TILE_OBSTACLE_BARREL = 8'd11;
    localparam TILE_ARROW_RIGHT = 8'd12;
    localparam TILE_ARROW_LEFT = 8'd13;
    localparam TILE_ARROW_UP = 8'd14;
    localparam TILE_ARROW_DOWN = 8'd15;
    localparam OBSTACLE_LEVEL2_OFFSET = 8'd12;

    //Breach constants
    localparam DIR_RIGHT = 2'd0;
    localparam DIR_LEFT = 2'd1;
    localparam DIR_UP = 2'd2;
    localparam DIR_DOWN = 2'd3;
    localparam BREACH_CENTER_START = 5'd13;
    localparam BREACH_CENTER_END = 5'd16;

    //BRAM addresses
    localparam TILEMAP_BASE_ADDR = 14'h1800;

    //UI colors (12-bit RGB444)
    localparam UI_BOX_BG = 12'h112;
    localparam UI_TEXT_COLOR = 12'hFFF;
    localparam UI_SELECT_COLOR = 12'hFF0;

    //Heart/Armor colors
    localparam HEART_FILL_COLOR = 12'hF00;
    localparam HEART_OUTLINE_COLOR = 12'hFFF;
    localparam HEART_EMPTY_COLOR = 12'h444;
    localparam ARMOR_FILL_COLOR = 12'h888;
    localparam ARMOR_OUTLINE_COLOR = 12'hAAA;
    localparam ARMOR_EMPTY_COLOR = 12'h333;

    //Sub-module Instantiations

    //UI Text Renderer
    logic is_text_pixel, is_selected_text;
    ui_text_renderer ui_text_inst (
        .DrawX(DrawX),
        .DrawY(DrawY),
        .game_state(game_state),
        .menu_selection(menu_selection),
        .is_text_pixel(is_text_pixel),
        .is_selected_text(is_selected_text)
    );

    //Heart and Armor Renderer
    logic is_heart_fill, is_heart_outline, heart_is_full;
    logic is_armor_fill, is_armor_outline, armor_is_full;
    heart_armor_renderer heart_armor_inst (
        .DrawX(DrawX),
        .DrawY(DrawY),
        .player_health(player_health),
        .player_armor(player_armor),
        .is_heart_fill(is_heart_fill),
        .is_heart_outline(is_heart_outline),
        .heart_is_full(heart_is_full),
        .is_armor_fill(is_armor_fill),
        .is_armor_outline(is_armor_outline),
        .armor_is_full(armor_is_full)
    );

    //Sprite Address Calculator (Player and Boss)
    logic player_on, boss_on, is_attack_sprite;
    logic [16:0] sprite_addr_calc_out, boss_addr_calc_out;
    sprite_address_calc sprite_addr_inst (
        .DrawX(DrawX),
        .DrawY(DrawY),
        .player_x(player_x),
        .player_y(player_y),
        .player_frame(player_frame),
        .boss_x(boss_x),
        .boss_y(boss_y),
        .boss_frame(boss_frame),
        .boss_active(boss_active),
        .boss_flip(boss_flip),
        .player_on(player_on),
        .sprite_rom_addr(sprite_addr_calc_out),
        .is_attack_sprite(is_attack_sprite),
        .boss_on(boss_on),
        .boss_rom_addr(boss_addr_calc_out)
    );

    //Projectile Renderer
    logic proj_on;
    logic [5:0] proj_pixel_x;
    logic [4:0] proj_pixel_y;
    logic [3:0] proj_which;
    logic proj_current_is_player, proj_current_flip;
    logic [15:0] projectile_addr;
    logic [9:0] shuriken_addr_calc;
    projectile_renderer proj_inst (
        .DrawX(DrawX),
        .DrawY(DrawY),
        .proj_0(proj_0), .proj_1(proj_1), .proj_2(proj_2), .proj_3(proj_3),
        .proj_4(proj_4), .proj_5(proj_5), .proj_6(proj_6), .proj_7(proj_7),
        .proj_8(proj_8), .proj_9(proj_9), .proj_10(proj_10), .proj_11(proj_11),
        .proj_12(proj_12), .proj_13(proj_13), .proj_14(proj_14), .proj_15(proj_15),
        .player_proj_frame(player_proj_frame),
        .proj_on(proj_on),
        .proj_pixel_x(proj_pixel_x),
        .proj_pixel_y(proj_pixel_y),
        .proj_which(proj_which),
        .proj_current_is_player(proj_current_is_player),
        .proj_current_flip(proj_current_flip),
        .projectile_addr(projectile_addr),
        .shuriken_rom_addr(shuriken_addr_calc)
    );

    //Enemy Renderer
    logic enemy_on;
    logic enemy0_on, enemy1_on, enemy2_on, enemy3_on, enemy4_on;
    logic [15:0] enemy_addr_calc;
    logic [2:0] enemy_which;
    logic enemy_attacking_current, enemy_hit_current;
    enemy_renderer enemy_inst (
        .DrawX(DrawX),
        .DrawY(DrawY),
        .enemy0_x(enemy0_x), .enemy0_y(enemy0_y), .enemy0_frame(enemy0_frame),
        .enemy0_active(enemy0_active), .enemy0_flip(enemy0_flip),
        .enemy0_attacking(enemy0_attacking), .enemy0_hit(enemy0_hit), .enemy0_type(enemy0_type),
        .enemy1_x(enemy1_x), .enemy1_y(enemy1_y), .enemy1_frame(enemy1_frame),
        .enemy1_active(enemy1_active), .enemy1_flip(enemy1_flip),
        .enemy1_attacking(enemy1_attacking), .enemy1_hit(enemy1_hit), .enemy1_type(enemy1_type),
        .enemy2_x(enemy2_x), .enemy2_y(enemy2_y), .enemy2_frame(enemy2_frame),
        .enemy2_active(enemy2_active), .enemy2_flip(enemy2_flip),
        .enemy2_attacking(enemy2_attacking), .enemy2_hit(enemy2_hit), .enemy2_type(enemy2_type),
        .enemy3_x(enemy3_x), .enemy3_y(enemy3_y), .enemy3_frame(enemy3_frame),
        .enemy3_active(enemy3_active), .enemy3_flip(enemy3_flip),
        .enemy3_attacking(enemy3_attacking), .enemy3_hit(enemy3_hit), .enemy3_type(enemy3_type),
        .enemy4_x(enemy4_x), .enemy4_y(enemy4_y), .enemy4_frame(enemy4_frame),
        .enemy4_active(enemy4_active), .enemy4_flip(enemy4_flip),
        .enemy4_attacking(enemy4_attacking), .enemy4_hit(enemy4_hit), .enemy4_type(enemy4_type),
        .projectile_addr(projectile_addr),
        .enemy_on(enemy_on),
        .enemy0_on(enemy0_on), .enemy1_on(enemy1_on), .enemy2_on(enemy2_on),
        .enemy3_on(enemy3_on), .enemy4_on(enemy4_on),
        .enemy_rom_addr(enemy_addr_calc),
        .enemy_which(enemy_which),
        .enemy_attacking_current(enemy_attacking_current),
        .enemy_hit_current(enemy_hit_current)
    );

    //Connect sub-module outputs to top-level outputs
    assign sprite_rom_addr = sprite_addr_calc_out;
    assign boss_rom_addr = boss_addr_calc_out;
    assign enemy_rom_addr = enemy_addr_calc;
    assign shuriken_rom_addr = shuriken_addr_calc;

    //Row Cache - Stores 30 tile indices for current row
    logic [7:0] row_cache [0:MAP_WIDTH-1];

    //Prefetch State Machine
    typedef enum logic [1:0] {
        IDLE,
        PREFETCH_WAIT,
        PREFETCH_READ,
        PREFETCH_DONE
    } prefetch_state_t;

    prefetch_state_t prefetch_state;
    logic [4:0] prefetch_col;
    logic [4:0] prefetch_row;
    logic       prefetch_active;
    logic       hsync_prev;
    logic [4:0] current_tile_row;

    logic hsync_rising;
    assign hsync_rising = hsync && !hsync_prev;

    logic [4:0] display_row;
    assign display_row = DrawY[8:4];

    //Prefetch FSM
    always_ff @(posedge clk or posedge reset) begin
        if (reset) begin
            prefetch_state <= IDLE;
            prefetch_col <= 5'd0;
            prefetch_row <= 5'd0;
            prefetch_active <= 1'b0;
            hsync_prev <= 1'b0;
            current_tile_row <= 5'd31;
            for (int i = 0; i < MAP_WIDTH; i++) begin
                row_cache[i] <= 8'd0;
            end
        end else begin
            hsync_prev <= hsync;

            case (prefetch_state)
                IDLE: begin
                    if (DrawY[3:0] == 4'd0 && display_row != current_tile_row && !blank) begin
                        prefetch_row <= display_row;
                        prefetch_col <= 5'd0;
                        prefetch_active <= 1'b1;
                        prefetch_state <= PREFETCH_WAIT;
                        current_tile_row <= display_row;
                    end
                end

                PREFETCH_WAIT: begin
                    prefetch_state <= PREFETCH_READ;
                end

                PREFETCH_READ: begin
                    row_cache[prefetch_col] <= tile_rom_data;

                    if (prefetch_col == MAP_WIDTH - 1) begin
                        prefetch_active <= 1'b0;
                        prefetch_state <= PREFETCH_DONE;
                    end else begin
                        prefetch_col <= prefetch_col + 1'd1;
                        prefetch_state <= PREFETCH_WAIT;
                    end
                end

                PREFETCH_DONE: begin
                    prefetch_state <= IDLE;
                end
            endcase
        end
    end

    //Coordinate Calculation
    logic [5:0] screen_tile_x;
    logic [4:0] screen_tile_y;
    logic [3:0] pixel_x;
    logic [3:0] pixel_y;
    logic [5:0] map_tile_x;
    logic       in_game_area;

    always_comb begin
        screen_tile_x = DrawX[9:4];
        screen_tile_y = DrawY[8:4];
        pixel_x = DrawX[3:0];
        pixel_y = DrawY[3:0];

        in_game_area = (screen_tile_x >= STATUS_BAR_WIDTH) &&
                       (screen_tile_x < SCREEN_WIDTH_TILES) &&
                       (screen_tile_y < MAP_HEIGHT);

        map_tile_x = screen_tile_x - STATUS_BAR_WIDTH;
    end

    //Tile Index Lookup with Breach Override
    logic [7:0] tile_index_raw;
    logic [7:0] tile_index;
    logic       is_breach_area;
    logic       is_arrow_area;
    logic [7:0] arrow_tile;
    logic       is_obstacle_tile;

    always_comb begin
        is_breach_area = 1'b0;
        is_arrow_area = 1'b0;
        arrow_tile = TILE_ARROW_RIGHT;

        case (breach_direction)
            DIR_RIGHT: begin
                is_breach_area = breach_open &&
                                 (map_tile_x == 5'd29) &&
                                 (screen_tile_y >= BREACH_CENTER_START) &&
                                 (screen_tile_y <= BREACH_CENTER_END);
                is_arrow_area = breach_open &&
                                (map_tile_x >= 5'd27) && (map_tile_x <= 5'd28) &&
                                (screen_tile_y >= BREACH_CENTER_START) &&
                                (screen_tile_y <= BREACH_CENTER_END);
                arrow_tile = TILE_ARROW_RIGHT;
            end
            DIR_LEFT: begin
                is_breach_area = breach_open &&
                                 (map_tile_x == 5'd0) &&
                                 (screen_tile_y >= BREACH_CENTER_START) &&
                                 (screen_tile_y <= BREACH_CENTER_END);
                is_arrow_area = breach_open &&
                                (map_tile_x >= 5'd1) && (map_tile_x <= 5'd2) &&
                                (screen_tile_y >= BREACH_CENTER_START) &&
                                (screen_tile_y <= BREACH_CENTER_END);
                arrow_tile = TILE_ARROW_LEFT;
            end
            DIR_UP: begin
                is_breach_area = breach_open &&
                                 (screen_tile_y == 5'd0) &&
                                 (map_tile_x >= BREACH_CENTER_START) &&
                                 (map_tile_x <= BREACH_CENTER_END);
                is_arrow_area = breach_open &&
                                (screen_tile_y >= 5'd1) && (screen_tile_y <= 5'd2) &&
                                (map_tile_x >= BREACH_CENTER_START) &&
                                (map_tile_x <= BREACH_CENTER_END);
                arrow_tile = TILE_ARROW_UP;
            end
            DIR_DOWN: begin
                is_breach_area = breach_open &&
                                 (screen_tile_y == 5'd29) &&
                                 (map_tile_x >= BREACH_CENTER_START) &&
                                 (map_tile_x <= BREACH_CENTER_END);
                is_arrow_area = breach_open &&
                                (screen_tile_y >= 5'd27) && (screen_tile_y <= 5'd28) &&
                                (map_tile_x >= BREACH_CENTER_START) &&
                                (map_tile_x <= BREACH_CENTER_END);
                arrow_tile = TILE_ARROW_DOWN;
            end
        endcase

        if (!in_game_area || map_tile_x >= MAP_WIDTH || screen_tile_y >= MAP_HEIGHT) begin
            tile_index_raw = 8'd0;
        end else if (is_breach_area) begin
            tile_index_raw = TILE_FLOOR;
        end else if (is_arrow_area) begin
            tile_index_raw = arrow_tile;
        end else begin
            tile_index_raw = row_cache[map_tile_x[4:0]];
        end

        is_obstacle_tile = (tile_index_raw == TILE_OBSTACLE_ROCK) ||
                           (tile_index_raw == TILE_OBSTACLE_BARREL);

        if (level_offset > 8'd0 && is_obstacle_tile) begin
            tile_index = tile_index_raw + OBSTACLE_LEVEL2_OFFSET;
        end else begin
            tile_index = tile_index_raw;
        end
    end

    //BRAM Address Multiplexing (Tile ROM)
    logic [13:0] tilemap_addr;
    logic [13:0] tile_pixel_addr;
    logic [13:0] map_offset;

    always_comb begin
        case (map_select)
            3'd0: map_offset = 14'd0;
            3'd1: map_offset = 14'd900;
            3'd2: map_offset = 14'd1800;
            3'd3: map_offset = 14'd2700;
            3'd4: map_offset = 14'd3600;
            default: map_offset = 14'd0;
        endcase
    end

    assign tilemap_addr = TILEMAP_BASE_ADDR + map_offset + (prefetch_row * MAP_WIDTH) + prefetch_col;
    assign tile_pixel_addr = {1'b0, tile_index[4:0], pixel_y, pixel_x};
    assign tile_rom_addr = prefetch_active ? tilemap_addr : tile_pixel_addr;

    //Transparency Checks
    logic sprite_transparent;
    logic enemy_transparent;
    logic proj_transparent;
    logic boss_transparent;
    logic shuriken_transparent;

    assign sprite_transparent = (sprite_rom_data == TRANSPARENT_COLOR);
    assign enemy_transparent = (enemy_rom_data == TRANSPARENT_COLOR);
    assign boss_transparent = (boss_rom_data == TRANSPARENT_COLOR);
    assign shuriken_transparent = (shuriken_rom_data == SHURIKEN_TRANSPARENT);
    assign proj_transparent = proj_current_is_player ? shuriken_transparent
                                                     : (enemy_rom_data == TRANSPARENT_COLOR);

    //Level Color Tinting (Desaturate + Darken for Level 2+)
    logic [3:0] tile_r_raw, tile_g_raw, tile_b_raw;
    logic [3:0] tile_r_tinted, tile_g_tinted, tile_b_tinted;
    logic [5:0] gray_sum;
    logic [3:0] gray;
    logic [4:0] r_plus_gray, g_plus_gray, b_plus_gray;

    always_comb begin
        tile_r_raw = tile_rom_data[11:8];
        tile_g_raw = tile_rom_data[7:4];
        tile_b_raw = tile_rom_data[3:0];

        gray_sum = {2'b0, tile_r_raw} + {2'b0, tile_g_raw} + {2'b0, tile_b_raw};
        gray = gray_sum[5:2];

        if (level_offset > 8'd0) begin
            r_plus_gray = {1'b0, tile_r_raw} + {1'b0, gray};
            g_plus_gray = {1'b0, tile_g_raw} + {1'b0, gray};
            b_plus_gray = {1'b0, tile_b_raw} + {1'b0, gray};

            tile_r_tinted = r_plus_gray[4:2] + 4'd1;
            tile_g_tinted = g_plus_gray[4:2] + 4'd1;
            tile_b_tinted = b_plus_gray[4:2] + 4'd1;

            if (tile_r_tinted > 4'd15) tile_r_tinted = 4'd15;
            if (tile_g_tinted > 4'd15) tile_g_tinted = 4'd15;
            if (tile_b_tinted > 4'd15) tile_b_tinted = 4'd15;
        end else begin
            tile_r_tinted = tile_r_raw;
            tile_g_tinted = tile_g_raw;
            tile_b_tinted = tile_b_raw;
        end
    end

    //Output RGB - Priority: UI > Player > Boss > Enemy > Projectile > Heart/Armor > Tile
    always_comb begin
        if (blank) begin
            red_out = 4'h0;
            green_out = 4'h0;
            blue_out = 4'h0;
            pixel_valid = 1'b0;
            is_sprite_pixel = 1'b0;
        end else if (game_state != GAME_STATE_PLAYING) begin
            //Fullscreen UI overlay
            if (is_text_pixel) begin
                if (is_selected_text) begin
                    red_out = UI_SELECT_COLOR[11:8];
                    green_out = UI_SELECT_COLOR[7:4];
                    blue_out = UI_SELECT_COLOR[3:0];
                end else begin
                    red_out = UI_TEXT_COLOR[11:8];
                    green_out = UI_TEXT_COLOR[7:4];
                    blue_out = UI_TEXT_COLOR[3:0];
                end
            end else begin
                red_out = UI_BOX_BG[11:8];
                green_out = UI_BOX_BG[7:4];
                blue_out = UI_BOX_BG[3:0];
            end
            pixel_valid = 1'b1;
            is_sprite_pixel = 1'b0;
        end else if (player_on && !sprite_transparent) begin
            //Player sprite
            red_out = {sprite_rom_data[5:4], sprite_rom_data[5:4]};
            green_out = {sprite_rom_data[3:2], sprite_rom_data[3:2]};
            blue_out = {sprite_rom_data[1:0], sprite_rom_data[1:0]};
            pixel_valid = 1'b1;
            is_sprite_pixel = 1'b1;
        end else if (boss_on && !boss_transparent) begin
            //Boss sprite with hit tint
            if (boss_hit) begin
                red_out = 4'hF;
                green_out = {1'b0, boss_rom_data[3:2], 1'b0};
                blue_out = {1'b0, boss_rom_data[1:0], 1'b0};
            end else begin
                red_out = {boss_rom_data[5:4], boss_rom_data[5:4]};
                green_out = {boss_rom_data[3:2], boss_rom_data[3:2]};
                blue_out = {boss_rom_data[1:0], boss_rom_data[1:0]};
            end
            pixel_valid = 1'b1;
            is_sprite_pixel = 1'b1;
        end else if (enemy_on && !enemy_transparent) begin
            //Enemy sprite with hit/attacking tint
            if (enemy_hit_current) begin
                red_out = 4'hF;
                green_out = {1'b0, enemy_rom_data[3:2], 1'b0};
                blue_out = {1'b0, enemy_rom_data[1:0], 1'b0};
            end else if (enemy_attacking_current) begin
                red_out = {1'b0, enemy_rom_data[5:4], 1'b0};
                green_out = {1'b0, enemy_rom_data[3:2], 1'b0};
                blue_out = 4'hF;
            end else begin
                red_out = {enemy_rom_data[5:4], enemy_rom_data[5:4]};
                green_out = {enemy_rom_data[3:2], enemy_rom_data[3:2]};
                blue_out = {enemy_rom_data[1:0], enemy_rom_data[1:0]};
            end
            pixel_valid = 1'b1;
            is_sprite_pixel = 1'b1;
        end else if (proj_on && !proj_transparent) begin
            //Projectile
            if (proj_current_is_player) begin
                red_out = shuriken_rom_data[11:8];
                green_out = shuriken_rom_data[7:4];
                blue_out = shuriken_rom_data[3:0];
            end else begin
                red_out = {enemy_rom_data[5:4], enemy_rom_data[5:4]};
                green_out = {enemy_rom_data[3:2], enemy_rom_data[3:2]};
                blue_out = {enemy_rom_data[1:0], enemy_rom_data[1:0]};
            end
            is_sprite_pixel = 1'b1;
            pixel_valid = 1'b1;
        end else if (!in_game_area) begin
            //Status bar area
            if (is_heart_outline) begin
                if (heart_is_full) begin
                    red_out = HEART_OUTLINE_COLOR[11:8];
                    green_out = HEART_OUTLINE_COLOR[7:4];
                    blue_out = HEART_OUTLINE_COLOR[3:0];
                end else begin
                    red_out = HEART_EMPTY_COLOR[11:8];
                    green_out = HEART_EMPTY_COLOR[7:4];
                    blue_out = HEART_EMPTY_COLOR[3:0];
                end
                pixel_valid = 1'b1;
                is_sprite_pixel = 1'b0;
            end else if (is_heart_fill && heart_is_full) begin
                red_out = HEART_FILL_COLOR[11:8];
                green_out = HEART_FILL_COLOR[7:4];
                blue_out = HEART_FILL_COLOR[3:0];
                pixel_valid = 1'b1;
                is_sprite_pixel = 1'b0;
            end else if (is_armor_outline) begin
                if (armor_is_full) begin
                    red_out = ARMOR_OUTLINE_COLOR[11:8];
                    green_out = ARMOR_OUTLINE_COLOR[7:4];
                    blue_out = ARMOR_OUTLINE_COLOR[3:0];
                end else begin
                    red_out = ARMOR_EMPTY_COLOR[11:8];
                    green_out = ARMOR_EMPTY_COLOR[7:4];
                    blue_out = ARMOR_EMPTY_COLOR[3:0];
                end
                pixel_valid = 1'b1;
                is_sprite_pixel = 1'b0;
            end else if (is_armor_fill && armor_is_full) begin
                red_out = ARMOR_FILL_COLOR[11:8];
                green_out = ARMOR_FILL_COLOR[7:4];
                blue_out = ARMOR_FILL_COLOR[3:0];
                pixel_valid = 1'b1;
                is_sprite_pixel = 1'b0;
            end else begin
                red_out = 4'h0;
                green_out = 4'h0;
                blue_out = 4'h0;
                pixel_valid = 1'b0;
                is_sprite_pixel = 1'b0;
            end
        end else if (prefetch_active) begin
            red_out = 4'h0;
            green_out = 4'h0;
            blue_out = 4'h0;
            pixel_valid = 1'b0;
            is_sprite_pixel = 1'b0;
        end else begin
            //Tile pixel
            red_out = tile_r_tinted;
            green_out = tile_g_tinted;
            blue_out = tile_b_tinted;
            pixel_valid = 1'b1;
            is_sprite_pixel = 1'b0;
        end
    end

endmodule
