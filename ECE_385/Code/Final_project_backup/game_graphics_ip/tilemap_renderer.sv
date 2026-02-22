//-------------------------------------------------------------------------
//  tilemap_renderer.sv
//-------------------------------------------------------------------------
//  Purpose:
//      Renders a fixed tilemap background for Soul Knight game.
//      Prefetches tilemap row during horizontal blank, then renders pixels.
//      Overlays player sprite on top of tilemap.
//      Supports both 32x32 movement sprites and 48x48 attack sprites.
//      FIXED CAMERA - No scrolling (Binding of Isaac style).
//
//  Key Features:
//      - 30x30 tile map stored in BRAM (not hardcoded)
//      - Left 10 tile columns reserved for status bar (black for now)
//      - 16x16 pixel tiles
//      - 32x32 player sprite (movement) OR 48x48 (attack) with transparency
//      - 32x32 enemy sprite with transparency
//      - 8x8 projectiles (solid color, no sprite lookup)
//      - 48x48 sprites are centered on the 32x32 position (-8 offset)
//      - Tiles: RGB444 format: 12-bit per pixel [RRRR][GGGG][BBBB] (4096 colors)
//      - Sprites: RGB222 format: 6-bit per pixel [RR][GG][BB] (64 colors)
//      - Row prefetch during horizontal blank (hsync)
//      - No pipeline delay needed (BRAM runs at 100 MHz, renderer at 25 MHz)
//      - Render priority: Player > Enemy > Projectile > Tile
//
//  Memory Layout in BRAM:
//      0x0000 - 0x0FFF: Tile pixel data (16 tiles x 256 pixels)
//      0x1000 - 0x1383: Tilemap 0 (Empty room, 30x30 = 900 entries)
//      0x1384 - 0x1707: Tilemap 1 (I-shape room, 30x30 = 900 entries)
//
//  Sprite Frame Layout (REDUCED):
//      0-31:  32x32 movement (Idle + Run, 4 frames per direction)
//      32-51: 48x48 attack
//
//  Author: ECE 385 Final Project - Soul Knight Team
//-------------------------------------------------------------------------

module tilemap_renderer (
    input  logic        clk,          // Pixel clock (25 MHz)
    input  logic        reset,

    // VGA timing inputs
    input  logic [9:0]  DrawX,        // Current pixel X (0-639)
    input  logic [9:0]  DrawY,        // Current pixel Y (0-479)
    input  logic        blank,        // 1 = in blanking interval
    input  logic        hsync,        // Active during horizontal blank

    // Level offset (for future multi-level support)
    input  logic [7:0]  level_offset,

    // Map select (0-7: empty, I-shape, cross, pillars, stair, L2-I, L2-cross, L2-pillars)
    input  logic [2:0]  map_select,

    // Breach open (door visible when set)
    input  logic        breach_open,

    // Breach direction: 0=RIGHT, 1=LEFT, 2=UP, 3=DOWN
    input  logic [1:0]  breach_direction,

    // Player position (top-left corner of 32x32 block)
    input  logic [9:0]  player_x,
    input  logic [9:0]  player_y,
    input  logic [6:0]  player_frame,  // 7-bit for frames 0-83

    // Tile BRAM interface - 12-bit RGB444 per pixel
    output logic [13:0] tile_rom_addr,  // 14-bit address for BRAM (depth 16384)
    input  logic [11:0] tile_rom_data,  // 12-bit data from BRAM (RGB444)

    // Player Sprite BRAM interface - 6-bit RGB222 per pixel
    output logic [16:0] sprite_rom_addr,  // 17-bit address for sprite BRAM
    input  logic [5:0]  sprite_rom_data,  // 6-bit data from sprite BRAM

    // Enemy 0 position and state
    input  logic [9:0]  enemy0_x,
    input  logic [9:0]  enemy0_y,
    input  logic [2:0]  enemy0_frame,      // 3-bit for frames 0-7
    input  logic        enemy0_active,
    input  logic        enemy0_flip,       // Horizontal flip
    input  logic        enemy0_attacking,  // Blue tint when attacking
    input  logic        enemy0_hit,        // Red tint when hit by player
    input  logic [3:0]  enemy0_type,       // Enemy type for BRAM offset

    // Enemy 1 position and state
    input  logic [9:0]  enemy1_x,
    input  logic [9:0]  enemy1_y,
    input  logic [2:0]  enemy1_frame,      // 3-bit for frames 0-7
    input  logic        enemy1_active,
    input  logic        enemy1_flip,       // Horizontal flip
    input  logic        enemy1_attacking,  // Blue tint when attacking
    input  logic        enemy1_hit,        // Red tint when hit by player
    input  logic [3:0]  enemy1_type,       // Enemy type for BRAM offset

    // Enemy 2 position and state
    input  logic [9:0]  enemy2_x,
    input  logic [9:0]  enemy2_y,
    input  logic [2:0]  enemy2_frame,
    input  logic        enemy2_active,
    input  logic        enemy2_flip,
    input  logic        enemy2_attacking,
    input  logic        enemy2_hit,
    input  logic [3:0]  enemy2_type,

    // Enemy 3 position and state
    input  logic [9:0]  enemy3_x,
    input  logic [9:0]  enemy3_y,
    input  logic [2:0]  enemy3_frame,
    input  logic        enemy3_active,
    input  logic        enemy3_flip,
    input  logic        enemy3_attacking,
    input  logic        enemy3_hit,
    input  logic [3:0]  enemy3_type,

    // Enemy 4 position and state
    input  logic [9:0]  enemy4_x,
    input  logic [9:0]  enemy4_y,
    input  logic [2:0]  enemy4_frame,
    input  logic        enemy4_active,
    input  logic        enemy4_flip,
    input  logic        enemy4_attacking,
    input  logic        enemy4_hit,
    input  logic [3:0]  enemy4_type,

    // Enemy Sprite BRAM interface - 6-bit RGB222 per pixel
    output logic [15:0] enemy_rom_addr,   // 16-bit address for 40960-word enemy BRAM (5 enemies)
    input  logic [5:0]  enemy_rom_data,   // 6-bit data from enemy BRAM

    // Projectiles (16 total, packed format: {active[31], is_player[30], y[25:16], x[9:0]})
    // is_player: 1 = player projectile (use tile_rom), 0 = enemy projectile (use enemy_rom)
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

    // Player projectile animation frame (0-3, rotates through 4 frames)
    input  logic [1:0]  player_proj_frame,

    // Player health for status bar (0-6 hearts)
    input  logic [2:0]  player_health,

    // Player armor for armor bar (0-3 armor points)
    input  logic [1:0]  player_armor,

    // Game state for UI rendering
    input  logic [1:0]  game_state,      // 0=MENU, 1=PLAYING, 2=GAME_OVER
    input  logic        menu_selection,  // 0=first option, 1=second option

    // Boss position and state
    input  logic [9:0]  boss_x,
    input  logic [9:0]  boss_y,
    input  logic [4:0]  boss_frame,      // 5-bit for frames 0-19
    input  logic        boss_active,
    input  logic        boss_flip,       // Horizontal flip
    input  logic        boss_hit,        // Red tint when hit
    input  logic [7:0]  boss_health,     // For health bar display

    // Boss Sprite BRAM interface - 6-bit RGB222 per pixel
    output logic [16:0] boss_rom_addr,   // 17-bit address for 131072-word boss BRAM
    input  logic [5:0]  boss_rom_data,   // 6-bit data from boss BRAM

    // Shuriken (player projectile) BRAM interface - 12-bit RGB444 per pixel
    output logic [9:0]  shuriken_rom_addr,  // 10-bit address (4 frames × 256 pixels = 1024)
    input  logic [11:0] shuriken_rom_data,  // 12-bit data from shuriken BRAM

    // Output pixel - RGB444 for tiles, RGB222 for sprites
    // Top module handles expansion to RGB888
    output logic [3:0]  red_out,       // 4-bit red
    output logic [3:0]  green_out,     // 4-bit green
    output logic [3:0]  blue_out,      // 4-bit blue (tiles use all 4, sprites expand 2→4)
    output logic        pixel_valid,
    output logic        is_sprite_pixel // 1 = sprite pixel (RGB222), 0 = tile pixel (RGB444)
);

    //=====================================================================
    // Constants
    //=====================================================================
    localparam SCREEN_WIDTH_TILES = 40;   // 640/16
    localparam SCREEN_HEIGHT_TILES = 30;  // 480/16
    localparam STATUS_BAR_WIDTH = 10;     // Left 10 tiles for status bar
    localparam MAP_WIDTH = 30;            // Game area width in tiles
    localparam MAP_HEIGHT = 30;           // Game area height in tiles
    localparam TILEMAP_BASE_ADDR = 14'h1800;  // Tilemap starts at 0x1800 (after 22 tiles)
    localparam TRANSPARENT_COLOR = 6'h33;     // Magenta = transparent (6-bit: RR=11, GG=00, BB=11)

    // Sprite sizes
    localparam MOVE_SPRITE_SIZE = 32;     // 32x32 for movement
    localparam ATTACK_SPRITE_SIZE = 48;   // 48x48 for attack
    localparam ATTACK_FRAME_START = 32;   // Attack frames start at 32 (was 64)
    localparam ATTACK_CENTER_OFFSET = 8;  // (48-32)/2 = 8 pixel offset for centering

    // Address bases
    localparam MOVE_PIXELS_PER_FRAME = 1024;    // 32*32
    localparam ATTACK_PIXELS_PER_FRAME = 2304;  // 48*48
    localparam ATTACK_BASE_ADDR = 32768;        // 32 * 1024 (was 65536)

    // Enemy sprite size
    localparam ENEMY_SPRITE_SIZE = 32;          // 32x32 enemy sprites

    // Projectile constants
    localparam PROJECTILE_SIZE = 16;            // 16x16 projectiles (sprite from BRAM)
    localparam PROJECTILE_BASE_ADDR = 49152;    // Address in enemy ROM after 6 enemies (6 * 8192)
    localparam PROJECTILE_COLOR = 6'h30;        // Fallback: Bright red (if no sprite) (R=3, G=0, B=0)

    // Player projectile constants (16x16 shuriken, 4 frames in dedicated shuriken ROM)
    localparam SHURIKEN_SIZE = 16;              // 16x16 pixels
    localparam SHURIKEN_FRAMES = 4;             // 4 rotation frames
    localparam SHURIKEN_TRANSPARENT = 12'hF0F;  // Magenta transparent (12-bit RGB444)

    // Boss sprite constants
    localparam BOSS_SPRITE_SIZE = 64;           // 64x64 boss sprite
    localparam BOSS_PIXELS_PER_FRAME = 4096;    // 64 * 64
    localparam BOSS_MAX_HEALTH = 30;            // Max boss health for health bar

    // Heart display constants for status bar
    localparam HEART_SIZE = 16;                 // 16x16 pixel hearts
    localparam HEART_START_X = 8;               // First heart X position
    localparam HEART_START_Y = 8;               // Hearts Y position
    localparam HEART_SPACING = 24;              // Space between hearts (16 + 8 gap)
    localparam NUM_HEARTS = 6;                  // Max 6 hearts

    // Heart colors (RGB444)
    localparam HEART_FILL_COLOR = 12'hF00;      // Red fill
    localparam HEART_OUTLINE_COLOR = 12'hFFF;   // White outline
    localparam HEART_EMPTY_COLOR = 12'h444;     // Dark gray for empty heart outline

    // Armor bar display constants for status bar
    localparam ARMOR_BAR_WIDTH = 32;            // 32 pixels wide per armor point
    localparam ARMOR_BAR_HEIGHT = 8;            // 8 pixels tall
    localparam ARMOR_START_X = 8;               // First armor bar X position
    localparam ARMOR_START_Y = 28;              // Armor bars Y position (below hearts)
    localparam ARMOR_SPACING = 40;              // Space between armor bars (32 + 8 gap)
    localparam NUM_ARMOR = 3;                   // Max 3 armor points

    // Armor colors (RGB444)
    localparam ARMOR_FILL_COLOR = 12'h888;      // Gray fill for full armor
    localparam ARMOR_OUTLINE_COLOR = 12'hAAA;   // Light gray outline
    localparam ARMOR_EMPTY_COLOR = 12'h333;     // Dark gray for empty armor

    // Game state constants
    localparam GAME_STATE_MENU = 2'd0;
    localparam GAME_STATE_PLAYING = 2'd1;
    localparam GAME_STATE_GAMEOVER = 2'd2;
    localparam GAME_STATE_WIN = 2'd3;

    // Breach (door) constants for multi-direction support
    // Direction encoding: 0=RIGHT, 1=LEFT, 2=UP, 3=DOWN
    localparam DIR_RIGHT = 2'd0;
    localparam DIR_LEFT  = 2'd1;
    localparam DIR_UP    = 2'd2;
    localparam DIR_DOWN  = 2'd3;

    // Breach dimensions (4 tiles centered on wall)
    // For horizontal breaches: cols 13-16 (center of 30-wide map)
    // For vertical breaches: rows 13-16 (center of 30-tall map)
    localparam BREACH_CENTER_START = 13;  // Start of 4-tile center
    localparam BREACH_CENTER_END = 16;    // End of 4-tile center

    // Tile indices for breach
    localparam TILE_FLOOR = 8'd4;         // Floor tile index
    localparam TILE_ARROW_RIGHT = 8'd12;  // Arrow pointing right
    localparam TILE_ARROW_LEFT  = 8'd13;  // Arrow pointing left
    localparam TILE_ARROW_UP    = 8'd14;  // Arrow pointing up
    localparam TILE_ARROW_DOWN  = 8'd15;  // Arrow pointing down

    // UI Fullscreen constants (entire 640x480 screen)
    // Text is centered on screen
    localparam UI_SCREEN_W = 10'd640;        // Full screen width
    localparam UI_SCREEN_H = 10'd480;        // Full screen height
    localparam UI_CENTER_X = 10'd320;        // Screen center X
    localparam UI_CENTER_Y = 10'd240;        // Screen center Y

    // UI colors (RGB444)
    localparam UI_BOX_BG = 12'h112;          // Dark blue background
    localparam UI_BOX_BORDER = 12'hFFF;      // White border (unused now)
    localparam UI_TEXT_COLOR = 12'hFFF;      // White text
    localparam UI_SELECT_COLOR = 12'hFF0;    // Yellow for selected item

    //=====================================================================
    // Heart Bitmap (16x16) - Hardcoded in logic, no BRAM needed
    // Two layers: outline (1-pixel border) and fill (interior)
    //=====================================================================
    // Heart shape bitmap (1 = heart pixel, 0 = transparent)
    logic [15:0] heart_fill [0:15];
    logic [15:0] heart_outline [0:15];

    // Initialize heart bitmaps (active area of heart shape)
    // Fill = solid interior, Outline = 1-pixel border around fill
    always_comb begin
        // Heart fill pattern (interior pixels)
        heart_fill[0]  = 16'b0000000000000000;
        heart_fill[1]  = 16'b0000000000000000;
        heart_fill[2]  = 16'b0011100011100000;
        heart_fill[3]  = 16'b0111110111110000;
        heart_fill[4]  = 16'b0111111111110000;
        heart_fill[5]  = 16'b0111111111110000;
        heart_fill[6]  = 16'b0011111111100000;
        heart_fill[7]  = 16'b0001111111000000;
        heart_fill[8]  = 16'b0000111110000000;
        heart_fill[9]  = 16'b0000011100000000;
        heart_fill[10] = 16'b0000001000000000;
        heart_fill[11] = 16'b0000000000000000;
        heart_fill[12] = 16'b0000000000000000;
        heart_fill[13] = 16'b0000000000000000;
        heart_fill[14] = 16'b0000000000000000;
        heart_fill[15] = 16'b0000000000000000;

        // Heart outline pattern (border pixels only)
        heart_outline[0]  = 16'b0000000000000000;
        heart_outline[1]  = 16'b0011100011100000;
        heart_outline[2]  = 16'b0100010100010000;
        heart_outline[3]  = 16'b1000001000001000;
        heart_outline[4]  = 16'b1000000000001000;
        heart_outline[5]  = 16'b1000000000001000;
        heart_outline[6]  = 16'b0100000000010000;
        heart_outline[7]  = 16'b0010000000100000;
        heart_outline[8]  = 16'b0001000001000000;
        heart_outline[9]  = 16'b0000100010000000;
        heart_outline[10] = 16'b0000010100000000;
        heart_outline[11] = 16'b0000001000000000;
        heart_outline[12] = 16'b0000000000000000;
        heart_outline[13] = 16'b0000000000000000;
        heart_outline[14] = 16'b0000000000000000;
        heart_outline[15] = 16'b0000000000000000;
    end

    //=====================================================================
    // 8x8 Character Bitmaps for UI Text
    // Characters needed: S,O,U,L,K,N,I,G,H,T,A,R,E,X,M,V,P,>,space
    //=====================================================================

    // Function to get character bitmap row
    // Returns 8 bits representing one row of the character
    function logic [7:0] get_char_row(input logic [7:0] char_code, input logic [2:0] row);
        case (char_code)
            "S": case (row)
                3'd0: get_char_row = 8'b01111100;
                3'd1: get_char_row = 8'b11000000;
                3'd2: get_char_row = 8'b11000000;
                3'd3: get_char_row = 8'b01111100;
                3'd4: get_char_row = 8'b00000110;
                3'd5: get_char_row = 8'b00000110;
                3'd6: get_char_row = 8'b01111100;
                3'd7: get_char_row = 8'b00000000;
            endcase
            "O": case (row)
                3'd0: get_char_row = 8'b01111100;
                3'd1: get_char_row = 8'b11000110;
                3'd2: get_char_row = 8'b11000110;
                3'd3: get_char_row = 8'b11000110;
                3'd4: get_char_row = 8'b11000110;
                3'd5: get_char_row = 8'b11000110;
                3'd6: get_char_row = 8'b01111100;
                3'd7: get_char_row = 8'b00000000;
            endcase
            "U": case (row)
                3'd0: get_char_row = 8'b11000110;
                3'd1: get_char_row = 8'b11000110;
                3'd2: get_char_row = 8'b11000110;
                3'd3: get_char_row = 8'b11000110;
                3'd4: get_char_row = 8'b11000110;
                3'd5: get_char_row = 8'b11000110;
                3'd6: get_char_row = 8'b01111100;
                3'd7: get_char_row = 8'b00000000;
            endcase
            "L": case (row)
                3'd0: get_char_row = 8'b11000000;
                3'd1: get_char_row = 8'b11000000;
                3'd2: get_char_row = 8'b11000000;
                3'd3: get_char_row = 8'b11000000;
                3'd4: get_char_row = 8'b11000000;
                3'd5: get_char_row = 8'b11000000;
                3'd6: get_char_row = 8'b11111110;
                3'd7: get_char_row = 8'b00000000;
            endcase
            "K": case (row)
                3'd0: get_char_row = 8'b11000110;
                3'd1: get_char_row = 8'b11001100;
                3'd2: get_char_row = 8'b11011000;
                3'd3: get_char_row = 8'b11110000;
                3'd4: get_char_row = 8'b11011000;
                3'd5: get_char_row = 8'b11001100;
                3'd6: get_char_row = 8'b11000110;
                3'd7: get_char_row = 8'b00000000;
            endcase
            "N": case (row)
                3'd0: get_char_row = 8'b11000110;
                3'd1: get_char_row = 8'b11100110;
                3'd2: get_char_row = 8'b11110110;
                3'd3: get_char_row = 8'b11011110;
                3'd4: get_char_row = 8'b11001110;
                3'd5: get_char_row = 8'b11000110;
                3'd6: get_char_row = 8'b11000110;
                3'd7: get_char_row = 8'b00000000;
            endcase
            "I": case (row)
                3'd0: get_char_row = 8'b01111100;
                3'd1: get_char_row = 8'b00011000;
                3'd2: get_char_row = 8'b00011000;
                3'd3: get_char_row = 8'b00011000;
                3'd4: get_char_row = 8'b00011000;
                3'd5: get_char_row = 8'b00011000;
                3'd6: get_char_row = 8'b01111100;
                3'd7: get_char_row = 8'b00000000;
            endcase
            "C": case (row)
                3'd0: get_char_row = 8'b01111100;
                3'd1: get_char_row = 8'b11000110;
                3'd2: get_char_row = 8'b11000000;
                3'd3: get_char_row = 8'b11000000;
                3'd4: get_char_row = 8'b11000000;
                3'd5: get_char_row = 8'b11000110;
                3'd6: get_char_row = 8'b01111100;
                3'd7: get_char_row = 8'b00000000;
            endcase
            "G": case (row)
                3'd0: get_char_row = 8'b01111100;
                3'd1: get_char_row = 8'b11000110;
                3'd2: get_char_row = 8'b11000000;
                3'd3: get_char_row = 8'b11001110;
                3'd4: get_char_row = 8'b11000110;
                3'd5: get_char_row = 8'b11000110;
                3'd6: get_char_row = 8'b01111100;
                3'd7: get_char_row = 8'b00000000;
            endcase
            "H": case (row)
                3'd0: get_char_row = 8'b11000110;
                3'd1: get_char_row = 8'b11000110;
                3'd2: get_char_row = 8'b11000110;
                3'd3: get_char_row = 8'b11111110;
                3'd4: get_char_row = 8'b11000110;
                3'd5: get_char_row = 8'b11000110;
                3'd6: get_char_row = 8'b11000110;
                3'd7: get_char_row = 8'b00000000;
            endcase
            "T": case (row)
                3'd0: get_char_row = 8'b11111110;
                3'd1: get_char_row = 8'b00011000;
                3'd2: get_char_row = 8'b00011000;
                3'd3: get_char_row = 8'b00011000;
                3'd4: get_char_row = 8'b00011000;
                3'd5: get_char_row = 8'b00011000;
                3'd6: get_char_row = 8'b00011000;
                3'd7: get_char_row = 8'b00000000;
            endcase
            "A": case (row)
                3'd0: get_char_row = 8'b00111000;
                3'd1: get_char_row = 8'b01101100;
                3'd2: get_char_row = 8'b11000110;
                3'd3: get_char_row = 8'b11111110;
                3'd4: get_char_row = 8'b11000110;
                3'd5: get_char_row = 8'b11000110;
                3'd6: get_char_row = 8'b11000110;
                3'd7: get_char_row = 8'b00000000;
            endcase
            "R": case (row)
                3'd0: get_char_row = 8'b11111100;
                3'd1: get_char_row = 8'b11000110;
                3'd2: get_char_row = 8'b11000110;
                3'd3: get_char_row = 8'b11111100;
                3'd4: get_char_row = 8'b11011000;
                3'd5: get_char_row = 8'b11001100;
                3'd6: get_char_row = 8'b11000110;
                3'd7: get_char_row = 8'b00000000;
            endcase
            "E": case (row)
                3'd0: get_char_row = 8'b11111110;
                3'd1: get_char_row = 8'b11000000;
                3'd2: get_char_row = 8'b11000000;
                3'd3: get_char_row = 8'b11111100;
                3'd4: get_char_row = 8'b11000000;
                3'd5: get_char_row = 8'b11000000;
                3'd6: get_char_row = 8'b11111110;
                3'd7: get_char_row = 8'b00000000;
            endcase
            "X": case (row)
                3'd0: get_char_row = 8'b11000110;
                3'd1: get_char_row = 8'b01101100;
                3'd2: get_char_row = 8'b00111000;
                3'd3: get_char_row = 8'b00111000;
                3'd4: get_char_row = 8'b00111000;
                3'd5: get_char_row = 8'b01101100;
                3'd6: get_char_row = 8'b11000110;
                3'd7: get_char_row = 8'b00000000;
            endcase
            "M": case (row)
                3'd0: get_char_row = 8'b11000110;
                3'd1: get_char_row = 8'b11101110;
                3'd2: get_char_row = 8'b11111110;
                3'd3: get_char_row = 8'b11010110;
                3'd4: get_char_row = 8'b11000110;
                3'd5: get_char_row = 8'b11000110;
                3'd6: get_char_row = 8'b11000110;
                3'd7: get_char_row = 8'b00000000;
            endcase
            "V": case (row)
                3'd0: get_char_row = 8'b11000110;
                3'd1: get_char_row = 8'b11000110;
                3'd2: get_char_row = 8'b11000110;
                3'd3: get_char_row = 8'b11000110;
                3'd4: get_char_row = 8'b01101100;
                3'd5: get_char_row = 8'b00111000;
                3'd6: get_char_row = 8'b00010000;
                3'd7: get_char_row = 8'b00000000;
            endcase
            "P": case (row)
                3'd0: get_char_row = 8'b11111100;
                3'd1: get_char_row = 8'b11000110;
                3'd2: get_char_row = 8'b11000110;
                3'd3: get_char_row = 8'b11111100;
                3'd4: get_char_row = 8'b11000000;
                3'd5: get_char_row = 8'b11000000;
                3'd6: get_char_row = 8'b11000000;
                3'd7: get_char_row = 8'b00000000;
            endcase
            ">": case (row)
                3'd0: get_char_row = 8'b11000000;
                3'd1: get_char_row = 8'b01100000;
                3'd2: get_char_row = 8'b00110000;
                3'd3: get_char_row = 8'b00011000;
                3'd4: get_char_row = 8'b00110000;
                3'd5: get_char_row = 8'b01100000;
                3'd6: get_char_row = 8'b11000000;
                3'd7: get_char_row = 8'b00000000;
            endcase
            "J": case (row)
                3'd0: get_char_row = 8'b00011110;
                3'd1: get_char_row = 8'b00000110;
                3'd2: get_char_row = 8'b00000110;
                3'd3: get_char_row = 8'b00000110;
                3'd4: get_char_row = 8'b11000110;
                3'd5: get_char_row = 8'b11000110;
                3'd6: get_char_row = 8'b01111100;
                3'd7: get_char_row = 8'b00000000;
            endcase
            "W": case (row)
                3'd0: get_char_row = 8'b11000110;
                3'd1: get_char_row = 8'b11000110;
                3'd2: get_char_row = 8'b11000110;
                3'd3: get_char_row = 8'b11010110;
                3'd4: get_char_row = 8'b11111110;
                3'd5: get_char_row = 8'b11101110;
                3'd6: get_char_row = 8'b11000110;
                3'd7: get_char_row = 8'b00000000;
            endcase
            "Y": case (row)
                3'd0: get_char_row = 8'b11000110;
                3'd1: get_char_row = 8'b11000110;
                3'd2: get_char_row = 8'b01101100;
                3'd3: get_char_row = 8'b00111000;
                3'd4: get_char_row = 8'b00011000;
                3'd5: get_char_row = 8'b00011000;
                3'd6: get_char_row = 8'b00011000;
                3'd7: get_char_row = 8'b00000000;
            endcase
            "!": case (row)
                3'd0: get_char_row = 8'b00011000;
                3'd1: get_char_row = 8'b00011000;
                3'd2: get_char_row = 8'b00011000;
                3'd3: get_char_row = 8'b00011000;
                3'd4: get_char_row = 8'b00011000;
                3'd5: get_char_row = 8'b00000000;
                3'd6: get_char_row = 8'b00011000;
                3'd7: get_char_row = 8'b00000000;
            endcase
            default: // Space or unknown
                get_char_row = 8'b00000000;
        endcase
    endfunction

    //=====================================================================
    // UI Text Rendering Logic - Fullscreen centered text
    //=====================================================================

    // Text rendering - check if current pixel is part of text
    logic is_text_pixel;
    logic is_selected_text;

    // Text positions using absolute screen coordinates
    // Each char is 8x8, scaled to 16x16, with 2px spacing = 18px per char

    // Helper function to check if pixel is in a character at given position
    // Uses absolute screen coordinates (DrawX, DrawY)
    function logic pixel_in_char(
        input logic [9:0] px,      // pixel X (screen coordinate)
        input logic [9:0] py,      // pixel Y (screen coordinate)
        input logic [9:0] char_x,  // character X position (screen coordinate)
        input logic [9:0] char_y,  // character Y position (screen coordinate)
        input logic [7:0] char_code
    );
        logic [3:0] cx, cy;  // position within 16x16 scaled char
        logic [2:0] bx, by;  // position within 8x8 bitmap
        logic [7:0] row_bits;

        if (px >= char_x && px < char_x + 16 &&
            py >= char_y && py < char_y + 16) begin
            cx = px - char_x;
            cy = py - char_y;
            bx = cx[3:1];  // divide by 2 for scaling
            by = cy[3:1];  // divide by 2 for scaling
            row_bits = get_char_row(char_code, by);
            pixel_in_char = row_bits[7 - bx];  // bit 7 is leftmost
        end else begin
            pixel_in_char = 1'b0;
        end
    endfunction

    // Check all text strings - using absolute screen coordinates
    // Text is centered on screen (640x480)
    // "SOUL KNIGHT" = 11 chars * 18px = 198px, centered at X = (640-198)/2 = 221
    // Title at Y=180, options at Y=260, Y=300
    always_comb begin
        is_text_pixel = 1'b0;
        is_selected_text = 1'b0;

        if (game_state == GAME_STATE_MENU) begin
            // Title: "SOUL KNIGHT" centered at Y=180
            // 11 chars (including space) * 18px = 198px, start at X = 221
            if (pixel_in_char(DrawX, DrawY, 221, 180, "S")) is_text_pixel = 1'b1;
            if (pixel_in_char(DrawX, DrawY, 239, 180, "O")) is_text_pixel = 1'b1;
            if (pixel_in_char(DrawX, DrawY, 257, 180, "U")) is_text_pixel = 1'b1;
            if (pixel_in_char(DrawX, DrawY, 275, 180, "L")) is_text_pixel = 1'b1;
            // space
            if (pixel_in_char(DrawX, DrawY, 311, 180, "K")) is_text_pixel = 1'b1;
            if (pixel_in_char(DrawX, DrawY, 329, 180, "N")) is_text_pixel = 1'b1;
            if (pixel_in_char(DrawX, DrawY, 347, 180, "I")) is_text_pixel = 1'b1;
            if (pixel_in_char(DrawX, DrawY, 365, 180, "G")) is_text_pixel = 1'b1;
            if (pixel_in_char(DrawX, DrawY, 383, 180, "H")) is_text_pixel = 1'b1;
            if (pixel_in_char(DrawX, DrawY, 401, 180, "T")) is_text_pixel = 1'b1;

            // "PRESS SPACE" centered at Y=280
            // 11 chars * 18px = 198px, start at X = (640-198)/2 = 221
            if (pixel_in_char(DrawX, DrawY, 221, 280, "P")) begin is_text_pixel = 1'b1; is_selected_text = 1'b1; end
            if (pixel_in_char(DrawX, DrawY, 239, 280, "R")) begin is_text_pixel = 1'b1; is_selected_text = 1'b1; end
            if (pixel_in_char(DrawX, DrawY, 257, 280, "E")) begin is_text_pixel = 1'b1; is_selected_text = 1'b1; end
            if (pixel_in_char(DrawX, DrawY, 275, 280, "S")) begin is_text_pixel = 1'b1; is_selected_text = 1'b1; end
            if (pixel_in_char(DrawX, DrawY, 293, 280, "S")) begin is_text_pixel = 1'b1; is_selected_text = 1'b1; end
            // space at 311
            if (pixel_in_char(DrawX, DrawY, 329, 280, "S")) begin is_text_pixel = 1'b1; is_selected_text = 1'b1; end
            if (pixel_in_char(DrawX, DrawY, 347, 280, "P")) begin is_text_pixel = 1'b1; is_selected_text = 1'b1; end
            if (pixel_in_char(DrawX, DrawY, 365, 280, "A")) begin is_text_pixel = 1'b1; is_selected_text = 1'b1; end
            if (pixel_in_char(DrawX, DrawY, 383, 280, "C")) begin is_text_pixel = 1'b1; is_selected_text = 1'b1; end
            if (pixel_in_char(DrawX, DrawY, 401, 280, "E")) begin is_text_pixel = 1'b1; is_selected_text = 1'b1; end

        end else if (game_state == GAME_STATE_GAMEOVER) begin
            // Title: "GAME OVER" centered at Y=180
            // 9 chars * 16px = 144px, start at X = (640-144)/2 = 248
            if (pixel_in_char(DrawX, DrawY, 248, 180, "G")) is_text_pixel = 1'b1;
            if (pixel_in_char(DrawX, DrawY, 264, 180, "A")) is_text_pixel = 1'b1;
            if (pixel_in_char(DrawX, DrawY, 280, 180, "M")) is_text_pixel = 1'b1;
            if (pixel_in_char(DrawX, DrawY, 296, 180, "E")) is_text_pixel = 1'b1;
            // space at 312
            if (pixel_in_char(DrawX, DrawY, 328, 180, "O")) is_text_pixel = 1'b1;
            if (pixel_in_char(DrawX, DrawY, 344, 180, "V")) is_text_pixel = 1'b1;
            if (pixel_in_char(DrawX, DrawY, 360, 180, "E")) is_text_pixel = 1'b1;
            if (pixel_in_char(DrawX, DrawY, 376, 180, "R")) is_text_pixel = 1'b1;

            // Option 1: "> RESTART" centered at Y=260
            // 9 chars * 16px = 144px, start at X = (640-144)/2 = 248
            if (!menu_selection && pixel_in_char(DrawX, DrawY, 248, 260, ">")) begin
                is_text_pixel = 1'b1;
                is_selected_text = 1'b1;
            end
            // space at 264
            if (pixel_in_char(DrawX, DrawY, 280, 260, "R")) begin
                is_text_pixel = 1'b1;
                if (!menu_selection) is_selected_text = 1'b1;
            end
            if (pixel_in_char(DrawX, DrawY, 296, 260, "E")) begin
                is_text_pixel = 1'b1;
                if (!menu_selection) is_selected_text = 1'b1;
            end
            if (pixel_in_char(DrawX, DrawY, 312, 260, "S")) begin
                is_text_pixel = 1'b1;
                if (!menu_selection) is_selected_text = 1'b1;
            end
            if (pixel_in_char(DrawX, DrawY, 328, 260, "T")) begin
                is_text_pixel = 1'b1;
                if (!menu_selection) is_selected_text = 1'b1;
            end
            if (pixel_in_char(DrawX, DrawY, 344, 260, "A")) begin
                is_text_pixel = 1'b1;
                if (!menu_selection) is_selected_text = 1'b1;
            end
            if (pixel_in_char(DrawX, DrawY, 360, 260, "R")) begin
                is_text_pixel = 1'b1;
                if (!menu_selection) is_selected_text = 1'b1;
            end
            if (pixel_in_char(DrawX, DrawY, 376, 260, "T")) begin
                is_text_pixel = 1'b1;
                if (!menu_selection) is_selected_text = 1'b1;
            end

            // Option 2: "> EXIT" centered at Y=300
            // 6 chars * 16px = 96px, start at X = (640-96)/2 = 272
            if (menu_selection && pixel_in_char(DrawX, DrawY, 272, 300, ">")) begin
                is_text_pixel = 1'b1;
                is_selected_text = 1'b1;
            end
            // space at 288
            if (pixel_in_char(DrawX, DrawY, 304, 300, "E")) begin
                is_text_pixel = 1'b1;
                if (menu_selection) is_selected_text = 1'b1;
            end
            if (pixel_in_char(DrawX, DrawY, 320, 300, "X")) begin
                is_text_pixel = 1'b1;
                if (menu_selection) is_selected_text = 1'b1;
            end
            if (pixel_in_char(DrawX, DrawY, 336, 300, "I")) begin
                is_text_pixel = 1'b1;
                if (menu_selection) is_selected_text = 1'b1;
            end
            if (pixel_in_char(DrawX, DrawY, 352, 300, "T")) begin
                is_text_pixel = 1'b1;
                if (menu_selection) is_selected_text = 1'b1;
            end

        end else if (game_state == GAME_STATE_WIN) begin
            // Title: "YOU WIN!" centered at Y=180
            // 8 chars * 16px = 128px, start at X = (640-128)/2 = 256
            if (pixel_in_char(DrawX, DrawY, 256, 180, "Y")) is_text_pixel = 1'b1;
            if (pixel_in_char(DrawX, DrawY, 272, 180, "O")) is_text_pixel = 1'b1;
            if (pixel_in_char(DrawX, DrawY, 288, 180, "U")) is_text_pixel = 1'b1;
            // space at 304
            if (pixel_in_char(DrawX, DrawY, 320, 180, "W")) is_text_pixel = 1'b1;
            if (pixel_in_char(DrawX, DrawY, 336, 180, "I")) is_text_pixel = 1'b1;
            if (pixel_in_char(DrawX, DrawY, 352, 180, "N")) is_text_pixel = 1'b1;
            if (pixel_in_char(DrawX, DrawY, 368, 180, "!")) is_text_pixel = 1'b1;

            // Option 1: "> RESTART" centered at Y=260
            // 9 chars * 16px = 144px, start at X = (640-144)/2 = 248
            if (!menu_selection && pixel_in_char(DrawX, DrawY, 248, 260, ">")) begin
                is_text_pixel = 1'b1;
                is_selected_text = 1'b1;
            end
            // space at 264
            if (pixel_in_char(DrawX, DrawY, 280, 260, "R")) begin
                is_text_pixel = 1'b1;
                if (!menu_selection) is_selected_text = 1'b1;
            end
            if (pixel_in_char(DrawX, DrawY, 296, 260, "E")) begin
                is_text_pixel = 1'b1;
                if (!menu_selection) is_selected_text = 1'b1;
            end
            if (pixel_in_char(DrawX, DrawY, 312, 260, "S")) begin
                is_text_pixel = 1'b1;
                if (!menu_selection) is_selected_text = 1'b1;
            end
            if (pixel_in_char(DrawX, DrawY, 328, 260, "T")) begin
                is_text_pixel = 1'b1;
                if (!menu_selection) is_selected_text = 1'b1;
            end
            if (pixel_in_char(DrawX, DrawY, 344, 260, "A")) begin
                is_text_pixel = 1'b1;
                if (!menu_selection) is_selected_text = 1'b1;
            end
            if (pixel_in_char(DrawX, DrawY, 360, 260, "R")) begin
                is_text_pixel = 1'b1;
                if (!menu_selection) is_selected_text = 1'b1;
            end
            if (pixel_in_char(DrawX, DrawY, 376, 260, "T")) begin
                is_text_pixel = 1'b1;
                if (!menu_selection) is_selected_text = 1'b1;
            end

            // Option 2: "> EXIT" centered at Y=300
            // 6 chars * 16px = 96px, start at X = (640-96)/2 = 272
            if (menu_selection && pixel_in_char(DrawX, DrawY, 272, 300, ">")) begin
                is_text_pixel = 1'b1;
                is_selected_text = 1'b1;
            end
            // space at 288
            if (pixel_in_char(DrawX, DrawY, 304, 300, "E")) begin
                is_text_pixel = 1'b1;
                if (menu_selection) is_selected_text = 1'b1;
            end
            if (pixel_in_char(DrawX, DrawY, 320, 300, "X")) begin
                is_text_pixel = 1'b1;
                if (menu_selection) is_selected_text = 1'b1;
            end
            if (pixel_in_char(DrawX, DrawY, 336, 300, "I")) begin
                is_text_pixel = 1'b1;
                if (menu_selection) is_selected_text = 1'b1;
            end
            if (pixel_in_char(DrawX, DrawY, 352, 300, "T")) begin
                is_text_pixel = 1'b1;
                if (menu_selection) is_selected_text = 1'b1;
            end
        end
    end

    //=====================================================================
    // Row Cache - Stores 30 tile indices for current row
    //=====================================================================
    logic [7:0] row_cache [0:MAP_WIDTH-1];  // 30 tile indices (8-bit for up to 256 tiles)

    //=====================================================================
    // Prefetch State Machine
    //=====================================================================
    typedef enum logic [1:0] {
        IDLE,           // Waiting for hsync
        PREFETCH_WAIT,  // Wait for BRAM read (2 cycles at 100MHz, but we're at 25MHz)
        PREFETCH_READ,  // Read tile index from BRAM into cache
        PREFETCH_DONE   // Row prefetch complete
    } prefetch_state_t;

    prefetch_state_t prefetch_state;
    logic [4:0] prefetch_col;         // Which column we're prefetching (0-29)
    logic [4:0] prefetch_row;         // Which row to prefetch (next row)
    logic       prefetch_active;      // Currently doing prefetch
    logic       hsync_prev;           // Previous hsync for edge detection
    logic [4:0] current_tile_row;     // Current row being displayed

    // Detect hsync rising edge (start of horizontal blank)
    logic hsync_rising;
    assign hsync_rising = hsync && !hsync_prev;

    // Calculate which row to prefetch (the row we'll display next)
    // During row N's display, prefetch row N's tiles at the START of that row
    // Or prefetch row N+1 during the blank after row N
    logic [4:0] display_row;
    assign display_row = DrawY[8:4];  // Current display row (0-29)

    //=====================================================================
    // Prefetch FSM - Runs on pixel clock (25 MHz)
    //=====================================================================
    always_ff @(posedge clk or posedge reset) begin
        if (reset) begin
            prefetch_state <= IDLE;
            prefetch_col <= 5'd0;
            prefetch_row <= 5'd0;
            prefetch_active <= 1'b0;
            hsync_prev <= 1'b0;
            current_tile_row <= 5'd31;  // Invalid, force prefetch on first row
            for (int i = 0; i < MAP_WIDTH; i++) begin
                row_cache[i] <= 8'd0;
            end
        end else begin
            hsync_prev <= hsync;

            case (prefetch_state)
                IDLE: begin
                    // Start prefetch at beginning of each new tile row
                    // Check if we're at pixel row 0, 16, 32, etc. (start of tile row)
                    if (DrawY[3:0] == 4'd0 && display_row != current_tile_row && !blank) begin
                        // New tile row starting, prefetch this row
                        prefetch_row <= display_row;
                        prefetch_col <= 5'd0;
                        prefetch_active <= 1'b1;
                        prefetch_state <= PREFETCH_WAIT;
                        current_tile_row <= display_row;
                    end
                end

                PREFETCH_WAIT: begin
                    // BRAM read takes 2 cycles at 100MHz = 20ns
                    // At 25MHz, one cycle = 40ns, so data is ready
                    // But we issue address this cycle, read next cycle
                    prefetch_state <= PREFETCH_READ;
                end

                PREFETCH_READ: begin
                    // Store the tile index in cache
                    row_cache[prefetch_col] <= tile_rom_data;

                    if (prefetch_col == MAP_WIDTH - 1) begin
                        // Done prefetching all 30 columns
                        prefetch_active <= 1'b0;
                        prefetch_state <= PREFETCH_DONE;
                    end else begin
                        // Move to next column
                        prefetch_col <= prefetch_col + 1'd1;
                        prefetch_state <= PREFETCH_WAIT;
                    end
                end

                PREFETCH_DONE: begin
                    // Stay done until next tile row
                    prefetch_state <= IDLE;
                end
            endcase
        end
    end

    //=====================================================================
    // Coordinate Calculation (Combinational)
    //=====================================================================

    // Screen tile coordinates
    logic [5:0] screen_tile_x;    // 0-39
    logic [4:0] screen_tile_y;    // 0-29
    logic [3:0] pixel_x;          // 0-15 within tile
    logic [3:0] pixel_y;          // 0-15 within tile

    // Map coordinates (after status bar offset)
    logic [5:0] map_tile_x;
    logic       in_game_area;

    always_comb begin
        screen_tile_x = DrawX[9:4];   // Divide by 16
        screen_tile_y = DrawY[8:4];   // Divide by 16 (only need 5 bits for 0-29)
        pixel_x = DrawX[3:0];         // Mod 16
        pixel_y = DrawY[3:0];

        // Game area starts after status bar (10 tiles = 160 pixels)
        in_game_area = (screen_tile_x >= STATUS_BAR_WIDTH) &&
                       (screen_tile_x < SCREEN_WIDTH_TILES) &&
                       (screen_tile_y < MAP_HEIGHT);

        map_tile_x = screen_tile_x - STATUS_BAR_WIDTH;
    end

    //=====================================================================
    // Tile Index Lookup (from cache)
    // Override with floor/arrow tiles when breach is open
    // Apply obstacle offset for Level 2 (tiles 10-11 -> 22-23)
    //=====================================================================
    logic [7:0] tile_index_raw;  // Raw tile from cache
    logic [7:0] tile_index;      // Final tile index (with Level 2 offset applied)
    logic       is_breach_area;
    logic       is_arrow_area;
    logic [7:0] arrow_tile;
    logic       is_obstacle_tile;  // True if tile is an obstacle (10 or 11)

    // Obstacle tile indices
    localparam TILE_OBSTACLE_ROCK = 8'd10;
    localparam TILE_OBSTACLE_BARREL = 8'd11;
    localparam OBSTACLE_LEVEL2_OFFSET = 8'd12;  // Add 12 to get tiles 22-23

    always_comb begin
        // Default values
        is_breach_area = 1'b0;
        is_arrow_area = 1'b0;
        arrow_tile = TILE_ARROW_RIGHT;

        // Check breach area and arrow area based on direction
        case (breach_direction)
            DIR_RIGHT: begin
                // Breach on right wall (col 29, rows 13-16)
                is_breach_area = breach_open &&
                                 (map_tile_x == 5'd29) &&
                                 (screen_tile_y >= BREACH_CENTER_START) &&
                                 (screen_tile_y <= BREACH_CENTER_END);
                // Arrows in cols 27-28, same rows
                is_arrow_area = breach_open &&
                                (map_tile_x >= 5'd27) &&
                                (map_tile_x <= 5'd28) &&
                                (screen_tile_y >= BREACH_CENTER_START) &&
                                (screen_tile_y <= BREACH_CENTER_END);
                arrow_tile = TILE_ARROW_RIGHT;
            end
            DIR_LEFT: begin
                // Breach on left wall (col 0, rows 13-16)
                is_breach_area = breach_open &&
                                 (map_tile_x == 5'd0) &&
                                 (screen_tile_y >= BREACH_CENTER_START) &&
                                 (screen_tile_y <= BREACH_CENTER_END);
                // Arrows in cols 1-2, same rows
                is_arrow_area = breach_open &&
                                (map_tile_x >= 5'd1) &&
                                (map_tile_x <= 5'd2) &&
                                (screen_tile_y >= BREACH_CENTER_START) &&
                                (screen_tile_y <= BREACH_CENTER_END);
                arrow_tile = TILE_ARROW_LEFT;
            end
            DIR_UP: begin
                // Breach on top wall (row 0, cols 13-16)
                is_breach_area = breach_open &&
                                 (screen_tile_y == 5'd0) &&
                                 (map_tile_x >= BREACH_CENTER_START) &&
                                 (map_tile_x <= BREACH_CENTER_END);
                // Arrows in rows 1-2, same cols
                is_arrow_area = breach_open &&
                                (screen_tile_y >= 5'd1) &&
                                (screen_tile_y <= 5'd2) &&
                                (map_tile_x >= BREACH_CENTER_START) &&
                                (map_tile_x <= BREACH_CENTER_END);
                arrow_tile = TILE_ARROW_UP;
            end
            DIR_DOWN: begin
                // Breach on bottom wall (row 29, cols 13-16)
                is_breach_area = breach_open &&
                                 (screen_tile_y == 5'd29) &&
                                 (map_tile_x >= BREACH_CENTER_START) &&
                                 (map_tile_x <= BREACH_CENTER_END);
                // Arrows in rows 27-28, same cols
                is_arrow_area = breach_open &&
                                (screen_tile_y >= 5'd27) &&
                                (screen_tile_y <= 5'd28) &&
                                (map_tile_x >= BREACH_CENTER_START) &&
                                (map_tile_x <= BREACH_CENTER_END);
                arrow_tile = TILE_ARROW_DOWN;
            end
        endcase

        // Get raw tile index
        if (!in_game_area || map_tile_x >= MAP_WIDTH || screen_tile_y >= MAP_HEIGHT) begin
            tile_index_raw = 8'd0;
        end else if (is_breach_area) begin
            // Breach tiles become floor (door opening)
            tile_index_raw = TILE_FLOOR;
        end else if (is_arrow_area) begin
            // Arrow tiles pointing toward breach
            tile_index_raw = arrow_tile;
        end else begin
            // Normal tile from cache
            tile_index_raw = row_cache[map_tile_x[4:0]];
        end

        // Check if this is an obstacle tile (10 or 11)
        is_obstacle_tile = (tile_index_raw == TILE_OBSTACLE_ROCK) ||
                           (tile_index_raw == TILE_OBSTACLE_BARREL);

        // Apply Level 2 offset to obstacle tiles
        // When level_offset > 0, obstacles use tiles 22-23 instead of 10-11
        if (level_offset > 8'd0 && is_obstacle_tile) begin
            tile_index = tile_index_raw + OBSTACLE_LEVEL2_OFFSET;
        end else begin
            tile_index = tile_index_raw;
        end
    end

    //=====================================================================
    // BRAM Address Multiplexing (Tile ROM)
    //=====================================================================
    // During prefetch: read tilemap indices
    // During display: read tile pixels

    logic [13:0] tilemap_addr;  // 14-bit to handle 5 maps
    logic [13:0] tile_pixel_addr;

    // Tilemap address: 0x1800 + (map_select * 900) + row * 30 + col
    // Map 0: Empty room
    // Map 1: I-shape room
    // Map 2: Cross room
    // Map 3: Pillars room
    // Map 4: Stair room
    // Level 2 reuses same maps but with tile offset (obstacles 10-11 -> 22-23)
    localparam TILEMAP_SIZE = 900;  // 30 * 30
    // Calculate map offset: map_select * 900
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

    // Tile pixel address: tile_index * 256 + pixel_y * 16 + pixel_x
    // Using lower 5 bits of tile_index allows up to 32 tiles
    assign tile_pixel_addr = {1'b0, tile_index[4:0], pixel_y, pixel_x};

    // Mux: prefetch gets priority, then tile pixel
    // Player projectile now uses dedicated shuriken_rom
    assign tile_rom_addr = prefetch_active ? tilemap_addr : tile_pixel_addr;

    //=====================================================================
    // Player Sprite Overlay - Dual Size Support (32x32 and 48x48)
    //=====================================================================

    // Determine if we're in attack mode (48x48) or movement mode (32x32)
    logic is_attack_sprite;
    assign is_attack_sprite = (player_frame >= ATTACK_FRAME_START);

    // Calculate sprite draw position (centered for 48x48)
    // For 48x48: offset by -8 to center on the 32x32 position
    logic [9:0] sprite_draw_x, sprite_draw_y;
    logic [5:0] current_sprite_size;

    always_comb begin
        if (is_attack_sprite) begin
            // 48x48 attack sprite - center it by offsetting -8
            // Handle underflow: if player_x < 8, clamp to 0
            sprite_draw_x = (player_x >= ATTACK_CENTER_OFFSET) ?
                            (player_x - ATTACK_CENTER_OFFSET) : 10'd0;
            sprite_draw_y = (player_y >= ATTACK_CENTER_OFFSET) ?
                            (player_y - ATTACK_CENTER_OFFSET) : 10'd0;
            current_sprite_size = ATTACK_SPRITE_SIZE;
        end else begin
            // 32x32 movement sprite - no offset
            sprite_draw_x = player_x;
            sprite_draw_y = player_y;
            current_sprite_size = MOVE_SPRITE_SIZE;
        end
    end

    // Check if current pixel is inside the player's sprite rectangle
    logic player_on;
    logic [5:0] sprite_pixel_x;  // 0-47 max within sprite
    logic [5:0] sprite_pixel_y;  // 0-47 max within sprite

    always_comb begin
        player_on = (DrawX >= sprite_draw_x) &&
                    (DrawX < sprite_draw_x + current_sprite_size) &&
                    (DrawY >= sprite_draw_y) &&
                    (DrawY < sprite_draw_y + current_sprite_size);

        // Calculate pixel position within sprite
        // Full subtraction, then truncate to 6 bits (max value is 47 for 48x48)
        sprite_pixel_x = DrawX - sprite_draw_x;
        sprite_pixel_y = DrawY - sprite_draw_y;
    end

    //=====================================================================
    // Sprite ROM Address Calculation - Dual Size Support
    //=====================================================================
    // 32x32: addr = frame * 1024 + y * 32 + x
    // 48x48: addr = 65536 + (frame - 64) * 2304 + y * 48 + x

    logic [16:0] move_sprite_addr;
    logic [16:0] attack_sprite_addr;
    logic [4:0] attack_frame_offset;  // 0-19 for frames 64-83

    // Intermediate values with explicit widths to prevent overflow
    logic [16:0] attack_frame_base;   // frame_offset * 2304 (max 19*2304 = 43776)
    logic [11:0] attack_row_offset;   // y * 48 (max 47*48 = 2256)

    always_comb begin
        // 32x32 movement address: frame * 1024 + y * 32 + x
        // = {frame[5:0], y[4:0], x[4:0]}
        move_sprite_addr = {player_frame[5:0], sprite_pixel_y[4:0], sprite_pixel_x[4:0]};

        // 48x48 attack address: 65536 + (frame - 64) * 2304 + y * 48 + x
        // Break down multiplication to ensure correct bit widths:
        // 2304 = 2048 + 256 = (1 << 11) + (1 << 8)
        // 48 = 32 + 16 = (1 << 5) + (1 << 4)
        attack_frame_offset = player_frame - 7'd64;  // frames 64-83 map to 0-19

        // frame_offset * 2304 = frame_offset * (2048 + 256)
        attack_frame_base = ({12'b0, attack_frame_offset} << 11) + ({12'b0, attack_frame_offset} << 8);

        // y * 48 = y * (32 + 16)
        attack_row_offset = ({6'b0, sprite_pixel_y} << 5) + ({6'b0, sprite_pixel_y} << 4);

        attack_sprite_addr = ATTACK_BASE_ADDR + attack_frame_base + {5'b0, attack_row_offset} + {11'b0, sprite_pixel_x};

        // Select address based on sprite type
        sprite_rom_addr = is_attack_sprite ? attack_sprite_addr : move_sprite_addr;
    end

    //=====================================================================
    // Enemy Sprite Overlay - 5 Enemies, 32x32 each
    //=====================================================================

    // Check if current pixel is inside each enemy's sprite rectangle
    logic enemy0_on, enemy1_on, enemy2_on, enemy3_on, enemy4_on;
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

    //=====================================================================
    // Projectile Overlay - 4x 16x16 sprites from BRAM
    // (Defined before enemy ROM mux since projectile_addr is used there)
    //=====================================================================

    // Unpack projectile data (16 projectiles)
    // Format: {active[31], is_boss[30], flip[29], unused[28:26], y[25:16], unused[15:10], x[9:0]}
    logic [9:0] proj_x [0:15];
    logic [9:0] proj_y [0:15];
    logic       proj_active [0:15];
    logic       proj_is_player [0:15];  // 1 = player projectile (use tile_rom), 0 = enemy projectile (use enemy_rom)
    logic       proj_flip [0:15];     // 1 = flip horizontally

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

    // Check if current pixel is inside any projectile and calculate pixel offset
    logic proj_on;
    logic [5:0] proj_pixel_x;  // 0-47 for boss projectiles (wider than 16x16)
    logic [4:0] proj_pixel_y;  // 0-31 for boss projectiles (taller than 16x16)
    logic [3:0] proj_which;    // Which projectile (0-15)
    logic       proj_current_is_player;  // Is current projectile a boss projectile?
    logic       proj_current_flip;     // Should current projectile be flipped?

    always_comb begin
        proj_on = 1'b0;
        proj_pixel_x = 6'd0;
        proj_pixel_y = 5'd0;
        proj_which = 4'd0;
        proj_current_is_player = 1'b0;
        proj_current_flip = 1'b0;

        // Check each projectile (priority: 0 > 1 > ... > 15)
        // All projectiles use 16x16 size
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

    // Projectile ROM address = PROJECTILE_BASE_ADDR + y * 16 + x (for enemy projectiles)
    // Note: proj_pixel_x/y are now wider (6-bit/5-bit), but for enemy projectiles only use lower 4 bits
    logic [15:0] projectile_addr;
    assign projectile_addr = PROJECTILE_BASE_ADDR + {8'b0, proj_pixel_y[3:0], proj_pixel_x[3:0]};

    // Boss projectile ROM address - full 48x32 sprite with flip support
    // Boss projectile starts at address 81920 in boss ROM
    // Address = 81920 + y * 48 + x (or (47-x) if flipped)
    // y*48 = y*32 + y*16 = (y << 5) + (y << 4)
    localparam BOSS_PROJ_BASE_ADDR = 17'd81920;  // Start of boss projectile in boss ROM
    logic [16:0] boss_projectile_addr;
    logic [5:0] boss_proj_x_final;  // X coordinate after flip

    // Apply horizontal flip for boss projectile
    assign boss_proj_x_final = proj_current_flip ? (6'd47 - proj_pixel_x) : proj_pixel_x;

    // y * 48 calculation: y is now 5-bit (0-31), so max = 31*48 = 1488
    // y*48 = y*32 + y*16
    wire [10:0] boss_proj_y_times_32 = {proj_pixel_y, 5'b0};   // 5+5 = 10 bits, extended to 11
    wire [8:0] boss_proj_y_times_16 = {proj_pixel_y, 4'b0};    // 5+4 = 9 bits
    wire [10:0] boss_proj_y_times_48 = boss_proj_y_times_32 + {2'b0, boss_proj_y_times_16};

    assign boss_projectile_addr = BOSS_PROJ_BASE_ADDR + {6'b0, boss_proj_y_times_48} + {11'b0, boss_proj_x_final};

    // Shuriken ROM address (dedicated 12-bit BRAM)
    // Address = frame * 256 + y * 16 + x = {frame[1:0], y[3:0], x[3:0]}
    assign shuriken_rom_addr = {player_proj_frame, proj_pixel_y[3:0], proj_pixel_x[3:0]};

    // Shuriken transparency check (12-bit magenta)
    logic shuriken_transparent;
    assign shuriken_transparent = (shuriken_rom_data == SHURIKEN_TRANSPARENT);

    //=====================================================================
    // Enemy ROM address calculation
    //=====================================================================
    // Each enemy type has 8 frames (frames 0-7)
    // Address = enemy_type * 8192 + frame * 1024 + y * 32 + x
    // = {enemy_type[2:0], frame[2:0], y[4:0], x[4:0]} for 5 enemy types
    //
    // When enemy_flip=1, mirror horizontally: x -> (31 - x)

    logic [4:0] enemy0_pixel_x_final, enemy1_pixel_x_final;
    logic [4:0] enemy2_pixel_x_final, enemy3_pixel_x_final, enemy4_pixel_x_final;
    logic [15:0] enemy0_addr, enemy1_addr, enemy2_addr, enemy3_addr, enemy4_addr;
    logic [2:0] enemy_which;  // Which enemy is being rendered (5 = projectile)
    logic enemy_attacking_current;  // Is current enemy attacking? (blue tint)
    logic enemy_hit_current;        // Is current enemy hit? (red tint)

    always_comb begin
        // Apply horizontal flip for all enemies
        enemy0_pixel_x_final = enemy0_flip ? (5'd31 - enemy0_pixel_x) : enemy0_pixel_x;
        enemy1_pixel_x_final = enemy1_flip ? (5'd31 - enemy1_pixel_x) : enemy1_pixel_x;
        enemy2_pixel_x_final = enemy2_flip ? (5'd31 - enemy2_pixel_x) : enemy2_pixel_x;
        enemy3_pixel_x_final = enemy3_flip ? (5'd31 - enemy3_pixel_x) : enemy3_pixel_x;
        enemy4_pixel_x_final = enemy4_flip ? (5'd31 - enemy4_pixel_x) : enemy4_pixel_x;

        // Calculate addresses for each enemy
        // Address = {enemy_type[2:0], frame[2:0], y[4:0], x[4:0]}
        enemy0_addr = {enemy0_type[2:0], enemy0_frame, enemy0_pixel_y, enemy0_pixel_x_final};
        enemy1_addr = {enemy1_type[2:0], enemy1_frame, enemy1_pixel_y, enemy1_pixel_x_final};
        enemy2_addr = {enemy2_type[2:0], enemy2_frame, enemy2_pixel_y, enemy2_pixel_x_final};
        enemy3_addr = {enemy3_type[2:0], enemy3_frame, enemy3_pixel_y, enemy3_pixel_x_final};
        enemy4_addr = {enemy4_type[2:0], enemy4_frame, enemy4_pixel_y, enemy4_pixel_x_final};

        // Priority: Enemy0 > Enemy1 > Enemy2 > Enemy3 > Enemy4 > Projectile
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
            // No enemy on screen - use projectile address
            enemy_rom_addr = projectile_addr;
            enemy_which = 3'd5;  // Indicates projectile
            enemy_attacking_current = 1'b0;
            enemy_hit_current = 1'b0;
        end
    end

    // Check if any enemy is on screen at this pixel
    logic enemy_on;
    assign enemy_on = enemy0_on || enemy1_on || enemy2_on || enemy3_on || enemy4_on;

    // Check enemy/projectile sprite transparency
    // Player projectiles use shuriken_rom (12-bit), enemy projectiles use enemy_rom (6-bit)
    logic enemy_transparent;
    logic proj_transparent;
    assign enemy_transparent = (enemy_rom_data == TRANSPARENT_COLOR);
    // Player projectile uses 12-bit shuriken_rom_data with magenta transparent (0xF0F)
    // Enemy projectile uses 6-bit enemy_rom_data with magenta transparent (0x33)
    assign proj_transparent = proj_current_is_player ? shuriken_transparent
                                                     : (enemy_rom_data == TRANSPARENT_COLOR);

    //=====================================================================
    // Boss Sprite Overlay - 64x64
    //=====================================================================

    logic boss_on;
    logic [5:0] boss_pixel_x, boss_pixel_y;  // 0-63 within boss sprite
    logic [5:0] boss_pixel_x_final;          // After flip
    logic boss_transparent;

    always_comb begin
        boss_on = boss_active &&
                  (DrawX >= boss_x) &&
                  (DrawX < boss_x + BOSS_SPRITE_SIZE) &&
                  (DrawY >= boss_y) &&
                  (DrawY < boss_y + BOSS_SPRITE_SIZE);

        boss_pixel_x = DrawX - boss_x;
        boss_pixel_y = DrawY - boss_y;

        // Apply horizontal flip if needed
        boss_pixel_x_final = boss_flip ? (6'd63 - boss_pixel_x) : boss_pixel_x;
    end

    // Boss ROM address = frame * 4096 + y * 64 + x
    // = {frame[4:0], y[5:0], x[5:0]}
    // Boss ROM only used for boss sprite (projectiles use enemy_rom)
    assign boss_rom_addr = {boss_frame, boss_pixel_y, boss_pixel_x_final};

    assign boss_transparent = (boss_rom_data == TRANSPARENT_COLOR);

    // Boss projectile transparency check
    logic boss_proj_transparent;
    assign boss_proj_transparent = (boss_rom_data == TRANSPARENT_COLOR);

    //=====================================================================
    // Heart Rendering Logic for Status Bar
    //=====================================================================
    logic is_heart_fill;      // Current pixel is heart fill (red)
    logic is_heart_outline;   // Current pixel is heart outline (white/gray)
    logic heart_is_full;      // This heart slot has health
    logic [2:0] heart_index;  // Which heart (0-5)
    logic [3:0] heart_pixel_x, heart_pixel_y;  // Pixel within heart (0-15)

    always_comb begin
        is_heart_fill = 1'b0;
        is_heart_outline = 1'b0;
        heart_is_full = 1'b0;
        heart_index = 3'd0;
        heart_pixel_x = 4'd0;
        heart_pixel_y = 4'd0;

        // Check if we're in the heart display region
        if (DrawY >= HEART_START_Y && DrawY < (HEART_START_Y + HEART_SIZE)) begin
            // Calculate which heart and pixel within it
            if (DrawX >= HEART_START_X && DrawX < (HEART_START_X + NUM_HEARTS * HEART_SPACING)) begin
                // Determine heart index based on X position
                logic [9:0] rel_x;
                rel_x = DrawX - HEART_START_X;

                // Check each heart slot
                for (int i = 0; i < NUM_HEARTS; i++) begin
                    if (rel_x >= (i * HEART_SPACING) && rel_x < (i * HEART_SPACING + HEART_SIZE)) begin
                        heart_index = i[2:0];
                        heart_pixel_x = rel_x - (i * HEART_SPACING);
                        heart_pixel_y = DrawY - HEART_START_Y;
                        heart_is_full = (player_health > i);

                        // Look up bitmap - note: bit 15 is leftmost pixel
                        is_heart_fill = heart_fill[heart_pixel_y][15 - heart_pixel_x];
                        is_heart_outline = heart_outline[heart_pixel_y][15 - heart_pixel_x];
                    end
                end
            end
        end
    end

    //=====================================================================
    // Armor Bar Rendering Logic
    //=====================================================================
    // Simple rectangular armor bars below the hearts
    // Each armor point is a 32x8 rectangle
    logic is_armor_fill;      // Current pixel is armor fill (gray)
    logic is_armor_outline;   // Current pixel is armor outline
    logic armor_is_full;      // This armor slot has armor
    logic [1:0] armor_index;  // Which armor bar (0-2)
    logic [9:0] rel_x_armor;  // X position relative to armor area
    logic [9:0] rel_y_armor;  // Y position relative to armor area
    logic [9:0] armor_bar_x;  // X position within current armor bar

    always_comb begin
        is_armor_fill = 1'b0;
        is_armor_outline = 1'b0;
        armor_is_full = 1'b0;
        armor_index = 2'd0;
        rel_x_armor = 10'd0;
        rel_y_armor = 10'd0;
        armor_bar_x = 10'd0;

        // Check if we're in the armor display region
        if (DrawY >= ARMOR_START_Y && DrawY < (ARMOR_START_Y + ARMOR_BAR_HEIGHT)) begin
            // Calculate which armor bar and pixel within it
            if (DrawX >= ARMOR_START_X && DrawX < (ARMOR_START_X + NUM_ARMOR * ARMOR_SPACING)) begin
                // Calculate relative positions
                rel_x_armor = DrawX - ARMOR_START_X;
                rel_y_armor = DrawY - ARMOR_START_Y;

                // Check each armor slot
                for (int i = 0; i < NUM_ARMOR; i++) begin
                    if (rel_x_armor >= (i * ARMOR_SPACING) && rel_x_armor < (i * ARMOR_SPACING + ARMOR_BAR_WIDTH)) begin
                        armor_index = i[1:0];
                        armor_is_full = (player_armor > i);

                        // Calculate position within this armor bar
                        armor_bar_x = rel_x_armor - (i * ARMOR_SPACING);

                        // Outline: top row, bottom row, left column, right column
                        if (rel_y_armor == 0 || rel_y_armor == (ARMOR_BAR_HEIGHT - 1) ||
                            armor_bar_x == 0 || armor_bar_x == (ARMOR_BAR_WIDTH - 1)) begin
                            is_armor_outline = 1'b1;
                        end else begin
                            // Fill: everything inside the outline
                            is_armor_fill = 1'b1;
                        end
                    end
                end
            end
        end
    end

    //=====================================================================
    // Level Color Tinting (Option D: Desaturate + Darken for Level 2+)
    //=====================================================================
    // When level_offset > 0, apply dark dungeon effect to tiles:
    // - Mix original color with grayscale (desaturate)
    // - Then darken the result
    // Formula: out = (original * 0.4 + gray * 0.4) * 0.7 ≈ (original + gray) * 0.28
    // Hardware approximation: (original + gray) >> 2 ≈ 0.25

    logic [3:0] tile_r_raw, tile_g_raw, tile_b_raw;
    logic [3:0] tile_r_tinted, tile_g_tinted, tile_b_tinted;
    logic [5:0] gray_sum;      // R + G + B (max 45, needs 6 bits)
    logic [3:0] gray;          // Average gray value (0-15)
    logic [4:0] r_plus_gray, g_plus_gray, b_plus_gray;  // 5 bits for sum

    always_comb begin
        // Raw tile colors from BRAM
        tile_r_raw = tile_rom_data[11:8];
        tile_g_raw = tile_rom_data[7:4];
        tile_b_raw = tile_rom_data[3:0];

        // Calculate grayscale (approximate average: (R+G+B)/3 ≈ (R+G+B) >> 2 for speed)
        // More accurate: multiply by 85/256 ≈ 1/3
        gray_sum = {2'b0, tile_r_raw} + {2'b0, tile_g_raw} + {2'b0, tile_b_raw};
        // Divide by 3 using shift approximation: /3 ≈ *85/256 ≈ >> 2 + adjustment
        // Simpler: just use >> 2 (divide by 4, slightly darker gray)
        gray = gray_sum[5:2];  // Divide by 4 (close enough to /3)

        // Apply tint if level_offset > 0
        if (level_offset > 8'd0) begin
            // Desaturate + darken: (original + gray) * 0.28
            // Approximate as (original + gray) >> 2 = (original + gray) * 0.25
            r_plus_gray = {1'b0, tile_r_raw} + {1'b0, gray};
            g_plus_gray = {1'b0, tile_g_raw} + {1'b0, gray};
            b_plus_gray = {1'b0, tile_b_raw} + {1'b0, gray};

            // Divide by 4 (shift right 2) and add a small boost to prevent too dark
            // Result: (r+gray)/4 + 1 for minimum visibility
            tile_r_tinted = r_plus_gray[4:2] + 4'd1;
            tile_g_tinted = g_plus_gray[4:2] + 4'd1;
            tile_b_tinted = b_plus_gray[4:2] + 4'd1;

            // Clamp to max 15
            if (tile_r_tinted > 4'd15) tile_r_tinted = 4'd15;
            if (tile_g_tinted > 4'd15) tile_g_tinted = 4'd15;
            if (tile_b_tinted > 4'd15) tile_b_tinted = 4'd15;
        end else begin
            // No tint for level 1
            tile_r_tinted = tile_r_raw;
            tile_g_tinted = tile_g_raw;
            tile_b_tinted = tile_b_raw;
        end
    end

    //=====================================================================
    // Output RGB (Combinational - BRAM data is ready within same 25MHz cycle)
    //=====================================================================
    // tile_rom_data format: [11:8]=R, [7:4]=G, [3:0]=B (12-bit RGB444)
    // sprite_rom_data format: [5:4]=R, [3:2]=G, [1:0]=B (6-bit RGB222), 0x33 = transparent

    logic sprite_transparent;
    assign sprite_transparent = (sprite_rom_data == TRANSPARENT_COLOR);

    // Priority: UI > Player > Enemy > Projectile > Heart > Tile
    always_comb begin
        if (blank) begin
            // Blanking interval - output black
            red_out = 4'h0;
            green_out = 4'h0;
            blue_out = 4'h0;
            pixel_valid = 1'b0;
            is_sprite_pixel = 1'b0;
        end else if (game_state != GAME_STATE_PLAYING) begin
            // Fullscreen UI overlay when not in playing state
            // Dark blue background with centered white/yellow text
            if (is_text_pixel) begin
                // Render text
                if (is_selected_text) begin
                    // Selected text in yellow
                    red_out = UI_SELECT_COLOR[11:8];
                    green_out = UI_SELECT_COLOR[7:4];
                    blue_out = UI_SELECT_COLOR[3:0];
                end else begin
                    // Normal text in white
                    red_out = UI_TEXT_COLOR[11:8];
                    green_out = UI_TEXT_COLOR[7:4];
                    blue_out = UI_TEXT_COLOR[3:0];
                end
            end else begin
                // Dark blue background for entire screen
                red_out = UI_BOX_BG[11:8];
                green_out = UI_BOX_BG[7:4];
                blue_out = UI_BOX_BG[3:0];
            end
            pixel_valid = 1'b1;
            is_sprite_pixel = 1'b0;
        end else if (player_on && !sprite_transparent) begin
            // Player sprite pixel (non-transparent) - RGB222
            // Expand 2-bit to 4-bit by replicating: {R2, R2}
            red_out = {sprite_rom_data[5:4], sprite_rom_data[5:4]};
            green_out = {sprite_rom_data[3:2], sprite_rom_data[3:2]};
            blue_out = {sprite_rom_data[1:0], sprite_rom_data[1:0]};
            pixel_valid = 1'b1;
            is_sprite_pixel = 1'b1;
        end else if (boss_on && !boss_transparent) begin
            // Boss sprite pixel (non-transparent) - RGB222
            // Apply red tint when hit
            if (boss_hit) begin
                // Red tint when hit: max out red, halve green and blue
                red_out = 4'hF;  // Full red
                green_out = {1'b0, boss_rom_data[3:2], 1'b0};  // Reduce green
                blue_out = {1'b0, boss_rom_data[1:0], 1'b0};   // Reduce blue
            end else begin
                // Normal rendering
                red_out = {boss_rom_data[5:4], boss_rom_data[5:4]};
                green_out = {boss_rom_data[3:2], boss_rom_data[3:2]};
                blue_out = {boss_rom_data[1:0], boss_rom_data[1:0]};
            end
            pixel_valid = 1'b1;
            is_sprite_pixel = 1'b1;
        end else if (enemy_on && !enemy_transparent) begin
            // Enemy sprite pixel (non-transparent) - RGB222
            // Apply color tints: Red when hit (priority), Blue when attacking
            if (enemy_hit_current) begin
                // Red tint when hit by player: max out red, halve green and blue
                red_out = 4'hF;  // Full red
                green_out = {1'b0, enemy_rom_data[3:2], 1'b0};  // Reduce green
                blue_out = {1'b0, enemy_rom_data[1:0], 1'b0};   // Reduce blue
            end else if (enemy_attacking_current) begin
                // Blue tint when attacking: max out blue, halve red and green
                red_out = {1'b0, enemy_rom_data[5:4], 1'b0};    // Reduce red
                green_out = {1'b0, enemy_rom_data[3:2], 1'b0};  // Reduce green
                blue_out = 4'hF;  // Full blue
            end else begin
                // Normal rendering
                red_out = {enemy_rom_data[5:4], enemy_rom_data[5:4]};
                green_out = {enemy_rom_data[3:2], enemy_rom_data[3:2]};
                blue_out = {enemy_rom_data[1:0], enemy_rom_data[1:0]};
            end
            pixel_valid = 1'b1;
            is_sprite_pixel = 1'b1;
        end else if (proj_on && !proj_transparent) begin
            // Projectile rendering - player uses shuriken_rom (12-bit), others use enemy_rom (6-bit)
            if (proj_current_is_player) begin
                // Player projectile: 12-bit RGB444 from shuriken_rom
                red_out = shuriken_rom_data[11:8];
                green_out = shuriken_rom_data[7:4];
                blue_out = shuriken_rom_data[3:0];
            end else begin
                // Enemy/boss projectiles: 6-bit RGB222 from enemy_rom
                red_out = {enemy_rom_data[5:4], enemy_rom_data[5:4]};
                green_out = {enemy_rom_data[3:2], enemy_rom_data[3:2]};
                blue_out = {enemy_rom_data[1:0], enemy_rom_data[1:0]};
            end
            is_sprite_pixel = 1'b1;
            pixel_valid = 1'b1;
        end else if (!in_game_area) begin
            // Status bar area - render hearts
            if (is_heart_outline) begin
                // Heart outline - white if full, gray if empty
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
                // Heart fill - red only if heart is full
                red_out = HEART_FILL_COLOR[11:8];
                green_out = HEART_FILL_COLOR[7:4];
                blue_out = HEART_FILL_COLOR[3:0];
                pixel_valid = 1'b1;
                is_sprite_pixel = 1'b0;
            end else if (is_armor_outline) begin
                // Armor bar outline - light gray if full, dark gray if empty
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
                // Armor fill - gray only if armor is full
                red_out = ARMOR_FILL_COLOR[11:8];
                green_out = ARMOR_FILL_COLOR[7:4];
                blue_out = ARMOR_FILL_COLOR[3:0];
                pixel_valid = 1'b1;
                is_sprite_pixel = 1'b0;
            end else begin
                // Status bar background - black
                red_out = 4'h0;
                green_out = 4'h0;
                blue_out = 4'h0;
                pixel_valid = 1'b0;
                is_sprite_pixel = 1'b0;
            end
        end else if (prefetch_active) begin
            // During prefetch, output black (BRAM busy with tilemap read)
            red_out = 4'h0;
            green_out = 4'h0;
            blue_out = 4'h0;
            pixel_valid = 1'b0;
            is_sprite_pixel = 1'b0;
        end else begin
            // Valid game area pixel - use tinted tile colors (level_offset controls tint)
            red_out = tile_r_tinted;
            green_out = tile_g_tinted;
            blue_out = tile_b_tinted;
            pixel_valid = 1'b1;
            is_sprite_pixel = 1'b0;
        end
    end

endmodule
