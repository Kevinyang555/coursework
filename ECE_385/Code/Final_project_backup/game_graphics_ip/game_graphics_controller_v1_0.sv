`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// Company: ECE-Illinois
// Engineer: ECE 385 Student
//
// Create Date: 2025-12-03
// Module Name: game_graphics_controller_v1_0
// Project Name: ECE 385 - Soul Knight Final Project
// Description:
//   Top-level wrapper for tile-based game graphics controller IP.
//   Integrates tilemap renderer with direct RGB444 output.
//   Player sprite overlay with transparency support.
//   Fixed camera (no scrolling) - Binding of Isaac style.
//
// Features:
//   - 30x30 tilemap with 16x16 pixel tiles (right side of screen)
//   - 32x32 player movement sprite with transparency
//   - 48x48 player attack sprite (centered on 32x32 position)
//   - 32x32 enemy sprite with transparency
//   - 8x8 projectiles (solid color)
//   - Left 10 tile columns reserved for status bar
//   - Tiles: RGB444 (12-bit, 4096 colors) expanded to RGB888 for HDMI
//   - Sprites: RGB222 (6-bit, 64 colors) expanded to RGB888 for HDMI
//   - AXI4-Lite interface for MicroBlaze control
//   - HDMI output via hdmi_tx IP
//
//////////////////////////////////////////////////////////////////////////////////


module game_graphics_controller_v1_0 #
(
    // Parameters of AXI Slave Bus Interface S_AXI
    parameter integer C_S_AXI_DATA_WIDTH = 32,
    parameter integer C_S_AXI_ADDR_WIDTH = 14
)
(
    // HDMI output ports
    output logic hdmi_clk_n,
    output logic hdmi_clk_p,
    output logic [2:0] hdmi_tx_n,
    output logic [2:0] hdmi_tx_p,

    // Ports of Axi Slave Bus Interface S_AXI
    input  logic s_axi_aclk,
    input  logic s_axi_aresetn,
    input  logic [C_S_AXI_ADDR_WIDTH-1:0] s_axi_awaddr,
    input  logic [2:0] s_axi_awprot,
    input  logic s_axi_awvalid,
    output logic s_axi_awready,
    input  logic [C_S_AXI_DATA_WIDTH-1:0] s_axi_wdata,
    input  logic [(C_S_AXI_DATA_WIDTH/8)-1:0] s_axi_wstrb,
    input  logic s_axi_wvalid,
    output logic s_axi_wready,
    output logic [1:0] s_axi_bresp,
    output logic s_axi_bvalid,
    input  logic s_axi_bready,
    input  logic [C_S_AXI_ADDR_WIDTH-1:0] s_axi_araddr,
    input  logic [2:0] s_axi_arprot,
    input  logic s_axi_arvalid,
    output logic s_axi_arready,
    output logic [C_S_AXI_DATA_WIDTH-1:0] s_axi_rdata,
    output logic [1:0] s_axi_rresp,
    output logic s_axi_rvalid,
    input  logic s_axi_rready
);

    //=========================================================================
    // Clock Generation
    //=========================================================================

    logic clk_25MHz;     // Pixel clock
    logic clk_125MHz;    // HDMI serializer clock
    logic locked;
    logic reset_ah;      // Active-high reset

    assign reset_ah = ~s_axi_aresetn;

    // Clock wizard - generates 25 MHz pixel clock from 100 MHz AXI clock
    clk_wiz_0 clk_wiz (
        .clk_out1(clk_25MHz),    // 25 MHz pixel clock
        .clk_out2(clk_125MHz),   // 125 MHz for HDMI
        .reset(reset_ah),
        .locked(locked),
        .clk_in1(s_axi_aclk)     // 100 MHz AXI clock
    );

    //=========================================================================
    // VGA Timing Controller
    //=========================================================================

    logic [9:0] drawX, drawY;
    logic active_nblank;
    logic internal_hsync, internal_vsync;

    VGA_controller vga_controller (
        .pixel_clk(clk_25MHz),
        .reset(reset_ah),
        .hs(internal_hsync),
        .vs(internal_vsync),
        .active_nblank(active_nblank),
        .drawX(drawX),
        .drawY(drawY)
    );

    logic blank;
    assign blank = ~active_nblank;

    //=========================================================================
    // Tile ROM - RGB444 Format (12-bit per pixel)
    //=========================================================================

    logic [13:0] tile_rom_addr;   // 14-bit address for up to 16384 words
    logic [11:0] tile_rom_data;   // 12-bit RGB444: [RRRR][GGGG][BBBB]

    tile_rom tile_rom_inst (
        .clk(s_axi_aclk),          // Use 100 MHz clock for fast BRAM reads
        .addr(tile_rom_addr),
        .data(tile_rom_data)
    );

    //=========================================================================
    // Sprite ROM - RGB222 Format (6-bit per pixel)
    // Uses blk_mem_gen_1 reconfigured for sprites
    //=========================================================================

    logic [16:0] sprite_rom_addr;  // 17-bit address for sprites
    logic [5:0]  sprite_rom_data;  // 6-bit RGB222: [RR][GG][BB]

    sprite_rom sprite_rom_inst (
        .clk(s_axi_aclk),          // Use 100 MHz clock for fast BRAM reads
        .addr(sprite_rom_addr),
        .data(sprite_rom_data)
    );

    //=========================================================================
    // Enemy ROM - RGB222 Format (6-bit per pixel)
    // Uses blk_mem_gen_3 for enemy sprites (5 enemies x 8 frames = 40960 words)
    //=========================================================================

    logic [15:0] enemy_rom_addr;   // 16-bit address for 40960 words (5 enemies)
    logic [5:0]  enemy_rom_data;   // 6-bit RGB222: [RR][GG][BB]

    enemy_rom enemy_rom_inst (
        .clk(s_axi_aclk),          // Use 100 MHz clock for fast BRAM reads
        .addr(enemy_rom_addr),
        .data(enemy_rom_data)
    );

    //=========================================================================
    // Boss ROM - RGB222 Format (6-bit per pixel)
    // Uses blk_mem_gen_4 for boss sprites (20 frames x 4096 pixels = 81920 words)
    //=========================================================================

    logic [16:0] boss_rom_addr;    // 17-bit address for 131072 words (covers 83456 actual)
    logic [5:0]  boss_rom_data;    // 6-bit RGB222: [RR][GG][BB]

    boss_rom boss_rom_inst (
        .clk(s_axi_aclk),          // Use 100 MHz clock for fast BRAM reads
        .addr(boss_rom_addr),
        .data(boss_rom_data)
    );

    //=========================================================================
    // Shuriken ROM - RGB444 Format (12-bit per pixel)
    // Uses blk_mem_gen_5 for player projectile (4 frames x 256 pixels = 1024 words)
    //=========================================================================

    logic [9:0]  shuriken_rom_addr;  // 10-bit address for 1024 words
    logic [11:0] shuriken_rom_data;  // 12-bit RGB444: [RRRR][GGGG][BBBB]

    shuriken_rom shuriken_rom_inst (
        .clk(s_axi_aclk),           // Use 100 MHz clock for fast BRAM reads
        .addr(shuriken_rom_addr),
        .data(shuriken_rom_data)
    );

    //=========================================================================
    // Rendering Pipeline - RGB444 for tiles, RGB222 for sprites
    //=========================================================================

    logic [7:0] level_offset;
    logic [9:0] player_x, player_y;
    logic [6:0] player_frame;  // 7-bit for frames 0-51 (movement + attack)
    logic [3:0] tilemap_red, tilemap_green, tilemap_blue;  // 4-bit RGB444
    logic       tilemap_valid;
    logic       is_sprite_pixel;             // 1 = sprite (RGB222), 0 = tile (RGB444)

    // Enemy 0 signals from AXI (ranged)
    logic [9:0] enemy0_x, enemy0_y;
    logic [2:0] enemy0_frame;   // 3-bit for frames 0-7
    logic       enemy0_active;
    logic       enemy0_flip;
    logic       enemy0_attacking;
    logic       enemy0_hit;
    logic [3:0] enemy0_type;

    // Enemy 1 signals from AXI
    logic [9:0] enemy1_x, enemy1_y;
    logic [2:0] enemy1_frame;   // 3-bit for frames 0-7
    logic       enemy1_active;
    logic       enemy1_flip;
    logic       enemy1_attacking;
    logic       enemy1_hit;
    logic [3:0] enemy1_type;

    // Enemy 2 signals from AXI
    logic [9:0] enemy2_x, enemy2_y;
    logic [2:0] enemy2_frame;
    logic       enemy2_active;
    logic       enemy2_flip;
    logic       enemy2_attacking;
    logic       enemy2_hit;
    logic [3:0] enemy2_type;

    // Enemy 3 signals from AXI
    logic [9:0] enemy3_x, enemy3_y;
    logic [2:0] enemy3_frame;
    logic       enemy3_active;
    logic       enemy3_flip;
    logic       enemy3_attacking;
    logic       enemy3_hit;
    logic [3:0] enemy3_type;

    // Enemy 4 signals from AXI
    logic [9:0] enemy4_x, enemy4_y;
    logic [2:0] enemy4_frame;
    logic       enemy4_active;
    logic       enemy4_flip;
    logic       enemy4_attacking;
    logic       enemy4_hit;
    logic [3:0] enemy4_type;

    // Projectile signals from AXI (packed format)
    logic [31:0] proj_0, proj_1, proj_2, proj_3;
    logic [31:0] proj_4, proj_5, proj_6, proj_7;
    logic [31:0] proj_8, proj_9, proj_10, proj_11;
    logic [31:0] proj_12, proj_13, proj_14, proj_15;

    // Player health signal from AXI
    logic [2:0] player_health;

    // Player armor signal from AXI
    logic [1:0] player_armor;

    // Game state signals from AXI
    logic [1:0] game_state;
    logic       menu_selection;
    logic [2:0] map_select;       // 3-bit for 5 room templates
    logic       breach_open;
    logic [1:0] breach_direction; // 2-bit: 0=R, 1=L, 2=U, 3=D

    // Boss signals from AXI
    logic [9:0] boss_x, boss_y;
    logic [4:0] boss_frame;       // 5-bit for frames 0-19
    logic       boss_active;
    logic       boss_flip;
    logic       boss_hit;
    logic [7:0] boss_health;

    // Player projectile animation frame from AXI
    logic [1:0] player_proj_frame;  // 2-bit for frames 0-3

    //=========================================================================
    // Clock Domain Crossing: AXI (100 MHz) -> Renderer (25 MHz)
    // Synchronize player position/frame to avoid metastability
    //=========================================================================
    logic [9:0] player_x_sync, player_y_sync;
    logic [6:0] player_frame_sync;

    // Two-stage synchronizer for player signals
    logic [9:0] player_x_meta, player_y_meta;
    logic [6:0] player_frame_meta;

    always_ff @(posedge clk_25MHz or posedge reset_ah) begin
        if (reset_ah) begin
            player_x_meta <= 10'd320;
            player_y_meta <= 10'd240;
            player_frame_meta <= 7'd0;
            player_x_sync <= 10'd320;
            player_y_sync <= 10'd240;
            player_frame_sync <= 7'd0;
        end else begin
            // First stage (may be metastable)
            player_x_meta <= player_x;
            player_y_meta <= player_y;
            player_frame_meta <= player_frame;
            // Second stage (stable)
            player_x_sync <= player_x_meta;
            player_y_sync <= player_y_meta;
            player_frame_sync <= player_frame_meta;
        end
    end

    //=========================================================================
    // Clock Domain Crossing: Enemy 0 signals (AXI 100 MHz -> Renderer 25 MHz)
    //=========================================================================
    logic [9:0] enemy0_x_sync, enemy0_y_sync;
    logic [2:0] enemy0_frame_sync;
    logic       enemy0_active_sync, enemy0_flip_sync, enemy0_attacking_sync, enemy0_hit_sync;
    logic [3:0] enemy0_type_sync;

    // Two-stage synchronizer for enemy 0 signals
    logic [9:0] enemy0_x_meta, enemy0_y_meta;
    logic [2:0] enemy0_frame_meta;
    logic       enemy0_active_meta, enemy0_flip_meta, enemy0_attacking_meta, enemy0_hit_meta;
    logic [3:0] enemy0_type_meta;

    always_ff @(posedge clk_25MHz or posedge reset_ah) begin
        if (reset_ah) begin
            enemy0_x_meta <= 10'd400; enemy0_y_meta <= 10'd150;
            enemy0_frame_meta <= 3'd0; enemy0_active_meta <= 1'b0;
            enemy0_flip_meta <= 1'b0; enemy0_attacking_meta <= 1'b0; enemy0_hit_meta <= 1'b0;
            enemy0_type_meta <= 4'd0;
            enemy0_x_sync <= 10'd400; enemy0_y_sync <= 10'd150;
            enemy0_frame_sync <= 3'd0; enemy0_active_sync <= 1'b0;
            enemy0_flip_sync <= 1'b0; enemy0_attacking_sync <= 1'b0; enemy0_hit_sync <= 1'b0;
            enemy0_type_sync <= 4'd0;
        end else begin
            // First stage
            enemy0_x_meta <= enemy0_x; enemy0_y_meta <= enemy0_y;
            enemy0_frame_meta <= enemy0_frame; enemy0_active_meta <= enemy0_active;
            enemy0_flip_meta <= enemy0_flip; enemy0_attacking_meta <= enemy0_attacking; enemy0_hit_meta <= enemy0_hit;
            enemy0_type_meta <= enemy0_type;
            // Second stage
            enemy0_x_sync <= enemy0_x_meta; enemy0_y_sync <= enemy0_y_meta;
            enemy0_frame_sync <= enemy0_frame_meta; enemy0_active_sync <= enemy0_active_meta;
            enemy0_flip_sync <= enemy0_flip_meta; enemy0_attacking_sync <= enemy0_attacking_meta; enemy0_hit_sync <= enemy0_hit_meta;
            enemy0_type_sync <= enemy0_type_meta;
        end
    end

    //=========================================================================
    // Clock Domain Crossing: Enemy 1 signals (AXI 100 MHz -> Renderer 25 MHz)
    //=========================================================================
    logic [9:0] enemy1_x_sync, enemy1_y_sync;
    logic [2:0] enemy1_frame_sync;
    logic       enemy1_active_sync, enemy1_flip_sync, enemy1_attacking_sync, enemy1_hit_sync;
    logic [3:0] enemy1_type_sync;

    // Two-stage synchronizer for enemy 1 signals
    logic [9:0] enemy1_x_meta, enemy1_y_meta;
    logic [2:0] enemy1_frame_meta;
    logic       enemy1_active_meta, enemy1_flip_meta, enemy1_attacking_meta, enemy1_hit_meta;
    logic [3:0] enemy1_type_meta;

    always_ff @(posedge clk_25MHz or posedge reset_ah) begin
        if (reset_ah) begin
            enemy1_x_meta <= 10'd400; enemy1_y_meta <= 10'd300;
            enemy1_frame_meta <= 3'd0; enemy1_active_meta <= 1'b0;
            enemy1_flip_meta <= 1'b0; enemy1_attacking_meta <= 1'b0; enemy1_hit_meta <= 1'b0;
            enemy1_type_meta <= 4'd1;
            enemy1_x_sync <= 10'd400; enemy1_y_sync <= 10'd300;
            enemy1_frame_sync <= 3'd0; enemy1_active_sync <= 1'b0;
            enemy1_flip_sync <= 1'b0; enemy1_attacking_sync <= 1'b0; enemy1_hit_sync <= 1'b0;
            enemy1_type_sync <= 4'd1;
        end else begin
            // First stage
            enemy1_x_meta <= enemy1_x; enemy1_y_meta <= enemy1_y;
            enemy1_frame_meta <= enemy1_frame; enemy1_active_meta <= enemy1_active;
            enemy1_flip_meta <= enemy1_flip; enemy1_attacking_meta <= enemy1_attacking; enemy1_hit_meta <= enemy1_hit;
            enemy1_type_meta <= enemy1_type;
            // Second stage
            enemy1_x_sync <= enemy1_x_meta; enemy1_y_sync <= enemy1_y_meta;
            enemy1_frame_sync <= enemy1_frame_meta; enemy1_active_sync <= enemy1_active_meta;
            enemy1_flip_sync <= enemy1_flip_meta; enemy1_attacking_sync <= enemy1_attacking_meta; enemy1_hit_sync <= enemy1_hit_meta;
            enemy1_type_sync <= enemy1_type_meta;
        end
    end

    //=========================================================================
    // Clock Domain Crossing: Enemy 2 signals (AXI 100 MHz -> Renderer 25 MHz)
    //=========================================================================
    logic [9:0] enemy2_x_sync, enemy2_y_sync;
    logic [2:0] enemy2_frame_sync;
    logic       enemy2_active_sync, enemy2_flip_sync, enemy2_attacking_sync, enemy2_hit_sync;
    logic [3:0] enemy2_type_sync;

    logic [9:0] enemy2_x_meta, enemy2_y_meta;
    logic [2:0] enemy2_frame_meta;
    logic       enemy2_active_meta, enemy2_flip_meta, enemy2_attacking_meta, enemy2_hit_meta;
    logic [3:0] enemy2_type_meta;

    always_ff @(posedge clk_25MHz or posedge reset_ah) begin
        if (reset_ah) begin
            enemy2_x_meta <= 10'd0; enemy2_y_meta <= 10'd0;
            enemy2_frame_meta <= 3'd0; enemy2_active_meta <= 1'b0;
            enemy2_flip_meta <= 1'b0; enemy2_attacking_meta <= 1'b0; enemy2_hit_meta <= 1'b0;
            enemy2_type_meta <= 4'd2;
            enemy2_x_sync <= 10'd0; enemy2_y_sync <= 10'd0;
            enemy2_frame_sync <= 3'd0; enemy2_active_sync <= 1'b0;
            enemy2_flip_sync <= 1'b0; enemy2_attacking_sync <= 1'b0; enemy2_hit_sync <= 1'b0;
            enemy2_type_sync <= 4'd2;
        end else begin
            enemy2_x_meta <= enemy2_x; enemy2_y_meta <= enemy2_y;
            enemy2_frame_meta <= enemy2_frame; enemy2_active_meta <= enemy2_active;
            enemy2_flip_meta <= enemy2_flip; enemy2_attacking_meta <= enemy2_attacking; enemy2_hit_meta <= enemy2_hit;
            enemy2_type_meta <= enemy2_type;
            enemy2_x_sync <= enemy2_x_meta; enemy2_y_sync <= enemy2_y_meta;
            enemy2_frame_sync <= enemy2_frame_meta; enemy2_active_sync <= enemy2_active_meta;
            enemy2_flip_sync <= enemy2_flip_meta; enemy2_attacking_sync <= enemy2_attacking_meta; enemy2_hit_sync <= enemy2_hit_meta;
            enemy2_type_sync <= enemy2_type_meta;
        end
    end

    //=========================================================================
    // Clock Domain Crossing: Enemy 3 signals (AXI 100 MHz -> Renderer 25 MHz)
    //=========================================================================
    logic [9:0] enemy3_x_sync, enemy3_y_sync;
    logic [2:0] enemy3_frame_sync;
    logic       enemy3_active_sync, enemy3_flip_sync, enemy3_attacking_sync, enemy3_hit_sync;
    logic [3:0] enemy3_type_sync;

    logic [9:0] enemy3_x_meta, enemy3_y_meta;
    logic [2:0] enemy3_frame_meta;
    logic       enemy3_active_meta, enemy3_flip_meta, enemy3_attacking_meta, enemy3_hit_meta;
    logic [3:0] enemy3_type_meta;

    always_ff @(posedge clk_25MHz or posedge reset_ah) begin
        if (reset_ah) begin
            enemy3_x_meta <= 10'd0; enemy3_y_meta <= 10'd0;
            enemy3_frame_meta <= 3'd0; enemy3_active_meta <= 1'b0;
            enemy3_flip_meta <= 1'b0; enemy3_attacking_meta <= 1'b0; enemy3_hit_meta <= 1'b0;
            enemy3_type_meta <= 4'd3;
            enemy3_x_sync <= 10'd0; enemy3_y_sync <= 10'd0;
            enemy3_frame_sync <= 3'd0; enemy3_active_sync <= 1'b0;
            enemy3_flip_sync <= 1'b0; enemy3_attacking_sync <= 1'b0; enemy3_hit_sync <= 1'b0;
            enemy3_type_sync <= 4'd3;
        end else begin
            enemy3_x_meta <= enemy3_x; enemy3_y_meta <= enemy3_y;
            enemy3_frame_meta <= enemy3_frame; enemy3_active_meta <= enemy3_active;
            enemy3_flip_meta <= enemy3_flip; enemy3_attacking_meta <= enemy3_attacking; enemy3_hit_meta <= enemy3_hit;
            enemy3_type_meta <= enemy3_type;
            enemy3_x_sync <= enemy3_x_meta; enemy3_y_sync <= enemy3_y_meta;
            enemy3_frame_sync <= enemy3_frame_meta; enemy3_active_sync <= enemy3_active_meta;
            enemy3_flip_sync <= enemy3_flip_meta; enemy3_attacking_sync <= enemy3_attacking_meta; enemy3_hit_sync <= enemy3_hit_meta;
            enemy3_type_sync <= enemy3_type_meta;
        end
    end

    //=========================================================================
    // Clock Domain Crossing: Enemy 4 signals (AXI 100 MHz -> Renderer 25 MHz)
    //=========================================================================
    logic [9:0] enemy4_x_sync, enemy4_y_sync;
    logic [2:0] enemy4_frame_sync;
    logic       enemy4_active_sync, enemy4_flip_sync, enemy4_attacking_sync, enemy4_hit_sync;
    logic [3:0] enemy4_type_sync;

    logic [9:0] enemy4_x_meta, enemy4_y_meta;
    logic [2:0] enemy4_frame_meta;
    logic       enemy4_active_meta, enemy4_flip_meta, enemy4_attacking_meta, enemy4_hit_meta;
    logic [3:0] enemy4_type_meta;

    always_ff @(posedge clk_25MHz or posedge reset_ah) begin
        if (reset_ah) begin
            enemy4_x_meta <= 10'd0; enemy4_y_meta <= 10'd0;
            enemy4_frame_meta <= 3'd0; enemy4_active_meta <= 1'b0;
            enemy4_flip_meta <= 1'b0; enemy4_attacking_meta <= 1'b0; enemy4_hit_meta <= 1'b0;
            enemy4_type_meta <= 4'd4;
            enemy4_x_sync <= 10'd0; enemy4_y_sync <= 10'd0;
            enemy4_frame_sync <= 3'd0; enemy4_active_sync <= 1'b0;
            enemy4_flip_sync <= 1'b0; enemy4_attacking_sync <= 1'b0; enemy4_hit_sync <= 1'b0;
            enemy4_type_sync <= 4'd4;
        end else begin
            enemy4_x_meta <= enemy4_x; enemy4_y_meta <= enemy4_y;
            enemy4_frame_meta <= enemy4_frame; enemy4_active_meta <= enemy4_active;
            enemy4_flip_meta <= enemy4_flip; enemy4_attacking_meta <= enemy4_attacking; enemy4_hit_meta <= enemy4_hit;
            enemy4_type_meta <= enemy4_type;
            enemy4_x_sync <= enemy4_x_meta; enemy4_y_sync <= enemy4_y_meta;
            enemy4_frame_sync <= enemy4_frame_meta; enemy4_active_sync <= enemy4_active_meta;
            enemy4_flip_sync <= enemy4_flip_meta; enemy4_attacking_sync <= enemy4_attacking_meta; enemy4_hit_sync <= enemy4_hit_meta;
            enemy4_type_sync <= enemy4_type_meta;
        end
    end

    //=========================================================================
    // Clock Domain Crossing: Projectile signals (AXI 100 MHz -> Renderer 25 MHz)
    //=========================================================================
    logic [31:0] proj_0_sync, proj_1_sync, proj_2_sync, proj_3_sync;
    logic [31:0] proj_4_sync, proj_5_sync, proj_6_sync, proj_7_sync;
    logic [31:0] proj_8_sync, proj_9_sync, proj_10_sync, proj_11_sync;
    logic [31:0] proj_12_sync, proj_13_sync, proj_14_sync, proj_15_sync;

    // Two-stage synchronizer for projectile signals
    logic [31:0] proj_0_meta, proj_1_meta, proj_2_meta, proj_3_meta;
    logic [31:0] proj_4_meta, proj_5_meta, proj_6_meta, proj_7_meta;
    logic [31:0] proj_8_meta, proj_9_meta, proj_10_meta, proj_11_meta;
    logic [31:0] proj_12_meta, proj_13_meta, proj_14_meta, proj_15_meta;

    always_ff @(posedge clk_25MHz or posedge reset_ah) begin
        if (reset_ah) begin
            proj_0_meta <= 32'd0;
            proj_1_meta <= 32'd0;
            proj_2_meta <= 32'd0;
            proj_3_meta <= 32'd0;
            proj_4_meta <= 32'd0;
            proj_5_meta <= 32'd0;
            proj_6_meta <= 32'd0;
            proj_7_meta <= 32'd0;
            proj_8_meta <= 32'd0;
            proj_9_meta <= 32'd0;
            proj_10_meta <= 32'd0;
            proj_11_meta <= 32'd0;
            proj_12_meta <= 32'd0;
            proj_13_meta <= 32'd0;
            proj_14_meta <= 32'd0;
            proj_15_meta <= 32'd0;
            proj_0_sync <= 32'd0;
            proj_1_sync <= 32'd0;
            proj_2_sync <= 32'd0;
            proj_3_sync <= 32'd0;
            proj_4_sync <= 32'd0;
            proj_5_sync <= 32'd0;
            proj_6_sync <= 32'd0;
            proj_7_sync <= 32'd0;
            proj_8_sync <= 32'd0;
            proj_9_sync <= 32'd0;
            proj_10_sync <= 32'd0;
            proj_11_sync <= 32'd0;
            proj_12_sync <= 32'd0;
            proj_13_sync <= 32'd0;
            proj_14_sync <= 32'd0;
            proj_15_sync <= 32'd0;
        end else begin
            // First stage (may be metastable)
            proj_0_meta <= proj_0;
            proj_1_meta <= proj_1;
            proj_2_meta <= proj_2;
            proj_3_meta <= proj_3;
            proj_4_meta <= proj_4;
            proj_5_meta <= proj_5;
            proj_6_meta <= proj_6;
            proj_7_meta <= proj_7;
            proj_8_meta <= proj_8;
            proj_9_meta <= proj_9;
            proj_10_meta <= proj_10;
            proj_11_meta <= proj_11;
            proj_12_meta <= proj_12;
            proj_13_meta <= proj_13;
            proj_14_meta <= proj_14;
            proj_15_meta <= proj_15;
            // Second stage (stable)
            proj_0_sync <= proj_0_meta;
            proj_1_sync <= proj_1_meta;
            proj_2_sync <= proj_2_meta;
            proj_3_sync <= proj_3_meta;
            proj_4_sync <= proj_4_meta;
            proj_5_sync <= proj_5_meta;
            proj_6_sync <= proj_6_meta;
            proj_7_sync <= proj_7_meta;
            proj_8_sync <= proj_8_meta;
            proj_9_sync <= proj_9_meta;
            proj_10_sync <= proj_10_meta;
            proj_11_sync <= proj_11_meta;
            proj_12_sync <= proj_12_meta;
            proj_13_sync <= proj_13_meta;
            proj_14_sync <= proj_14_meta;
            proj_15_sync <= proj_15_meta;
        end
    end

    //=========================================================================
    // Clock Domain Crossing: Player health (AXI 100 MHz -> Renderer 25 MHz)
    //=========================================================================
    logic [2:0] player_health_sync;
    logic [2:0] player_health_meta;

    always_ff @(posedge clk_25MHz or posedge reset_ah) begin
        if (reset_ah) begin
            player_health_meta <= 3'd6;
            player_health_sync <= 3'd6;
        end else begin
            player_health_meta <= player_health;
            player_health_sync <= player_health_meta;
        end
    end

    //=========================================================================
    // Clock Domain Crossing: Player armor (AXI 100 MHz -> Renderer 25 MHz)
    //=========================================================================
    logic [1:0] player_armor_sync;
    logic [1:0] player_armor_meta;

    always_ff @(posedge clk_25MHz or posedge reset_ah) begin
        if (reset_ah) begin
            player_armor_meta <= 2'd3;
            player_armor_sync <= 2'd3;
        end else begin
            player_armor_meta <= player_armor;
            player_armor_sync <= player_armor_meta;
        end
    end

    //=========================================================================
    // Clock Domain Crossing: Game state (AXI 100 MHz -> Renderer 25 MHz)
    //=========================================================================
    logic [1:0] game_state_sync;
    logic       menu_selection_sync;
    logic [2:0] map_select_sync;
    logic       breach_open_sync;
    logic [1:0] breach_direction_sync;
    logic [1:0] game_state_meta;
    logic       menu_selection_meta;
    logic [2:0] map_select_meta;
    logic       breach_open_meta;
    logic [1:0] breach_direction_meta;

    always_ff @(posedge clk_25MHz or posedge reset_ah) begin
        if (reset_ah) begin
            game_state_meta <= 2'd0;       // Start in MENU
            menu_selection_meta <= 1'b0;
            map_select_meta <= 3'b0;       // Start in empty room
            breach_open_meta <= 1'b0;      // Breach closed
            breach_direction_meta <= 2'b0; // Default RIGHT
            game_state_sync <= 2'd0;
            menu_selection_sync <= 1'b0;
            map_select_sync <= 3'b0;
            breach_open_sync <= 1'b0;
            breach_direction_sync <= 2'b0;
        end else begin
            game_state_meta <= game_state;
            menu_selection_meta <= menu_selection;
            map_select_meta <= map_select;
            breach_open_meta <= breach_open;
            breach_direction_meta <= breach_direction;
            game_state_sync <= game_state_meta;
            menu_selection_sync <= menu_selection_meta;
            map_select_sync <= map_select_meta;
            breach_open_sync <= breach_open_meta;
            breach_direction_sync <= breach_direction_meta;
        end
    end

    //=========================================================================
    // Clock Domain Crossing: Boss signals (AXI 100 MHz -> Renderer 25 MHz)
    //=========================================================================
    logic [9:0] boss_x_sync, boss_y_sync;
    logic [4:0] boss_frame_sync;
    logic       boss_active_sync, boss_flip_sync, boss_hit_sync;
    logic [7:0] boss_health_sync;

    logic [9:0] boss_x_meta, boss_y_meta;
    logic [4:0] boss_frame_meta;
    logic       boss_active_meta, boss_flip_meta, boss_hit_meta;
    logic [7:0] boss_health_meta;

    always_ff @(posedge clk_25MHz or posedge reset_ah) begin
        if (reset_ah) begin
            boss_x_meta <= 10'd400; boss_y_meta <= 10'd200;
            boss_frame_meta <= 5'd0; boss_active_meta <= 1'b0;
            boss_flip_meta <= 1'b0; boss_hit_meta <= 1'b0;
            boss_health_meta <= 8'd30;
            boss_x_sync <= 10'd400; boss_y_sync <= 10'd200;
            boss_frame_sync <= 5'd0; boss_active_sync <= 1'b0;
            boss_flip_sync <= 1'b0; boss_hit_sync <= 1'b0;
            boss_health_sync <= 8'd30;
        end else begin
            // First stage
            boss_x_meta <= boss_x; boss_y_meta <= boss_y;
            boss_frame_meta <= boss_frame; boss_active_meta <= boss_active;
            boss_flip_meta <= boss_flip; boss_hit_meta <= boss_hit;
            boss_health_meta <= boss_health;
            // Second stage
            boss_x_sync <= boss_x_meta; boss_y_sync <= boss_y_meta;
            boss_frame_sync <= boss_frame_meta; boss_active_sync <= boss_active_meta;
            boss_flip_sync <= boss_flip_meta; boss_hit_sync <= boss_hit_meta;
            boss_health_sync <= boss_health_meta;
        end
    end

    //=========================================================================
    // Clock Domain Crossing: Player projectile frame (AXI 100 MHz -> Renderer 25 MHz)
    //=========================================================================
    logic [1:0] player_proj_frame_sync;
    logic [1:0] player_proj_frame_meta;

    always_ff @(posedge clk_25MHz or posedge reset_ah) begin
        if (reset_ah) begin
            player_proj_frame_meta <= 2'd0;
            player_proj_frame_sync <= 2'd0;
        end else begin
            player_proj_frame_meta <= player_proj_frame;
            player_proj_frame_sync <= player_proj_frame_meta;
        end
    end

    // Tilemap Renderer - outputs RGB444 directly with row prefetch
    tilemap_renderer tilemap_renderer_inst (
        .clk(clk_25MHz),
        .reset(reset_ah),
        .DrawX(drawX),
        .DrawY(drawY),
        .blank(blank),
        .hsync(internal_hsync),  // For prefetch timing

        // Level offset (not used in RGB222 mode but keep for compatibility)
        .level_offset(level_offset),

        // Map select (0-4: empty, I-shape, cross, pillars, stair)
        .map_select(map_select_sync),

        // Breach open (door visible)
        .breach_open(breach_open_sync),

        // Breach direction: 0=RIGHT, 1=LEFT, 2=UP, 3=DOWN
        .breach_direction(breach_direction_sync),

        // Player position and frame (synchronized to 25 MHz)
        .player_x(player_x_sync),
        .player_y(player_y_sync),
        .player_frame(player_frame_sync),

        // Tile ROM interface - RGB444 packed format
        .tile_rom_addr(tile_rom_addr),
        .tile_rom_data(tile_rom_data),

        // Sprite ROM interface - RGB222 packed format
        .sprite_rom_addr(sprite_rom_addr),
        .sprite_rom_data(sprite_rom_data),

        // Enemy 0 position and state (synchronized to 25 MHz)
        .enemy0_x(enemy0_x_sync),
        .enemy0_y(enemy0_y_sync),
        .enemy0_frame(enemy0_frame_sync),
        .enemy0_active(enemy0_active_sync),
        .enemy0_flip(enemy0_flip_sync),
        .enemy0_attacking(enemy0_attacking_sync),
        .enemy0_hit(enemy0_hit_sync),
        .enemy0_type(enemy0_type_sync),

        // Enemy 1 position and state (synchronized to 25 MHz)
        .enemy1_x(enemy1_x_sync),
        .enemy1_y(enemy1_y_sync),
        .enemy1_frame(enemy1_frame_sync),
        .enemy1_active(enemy1_active_sync),
        .enemy1_flip(enemy1_flip_sync),
        .enemy1_attacking(enemy1_attacking_sync),
        .enemy1_hit(enemy1_hit_sync),
        .enemy1_type(enemy1_type_sync),

        // Enemy 2 position and state (synchronized to 25 MHz)
        .enemy2_x(enemy2_x_sync),
        .enemy2_y(enemy2_y_sync),
        .enemy2_frame(enemy2_frame_sync),
        .enemy2_active(enemy2_active_sync),
        .enemy2_flip(enemy2_flip_sync),
        .enemy2_attacking(enemy2_attacking_sync),
        .enemy2_hit(enemy2_hit_sync),
        .enemy2_type(enemy2_type_sync),

        // Enemy 3 position and state (synchronized to 25 MHz)
        .enemy3_x(enemy3_x_sync),
        .enemy3_y(enemy3_y_sync),
        .enemy3_frame(enemy3_frame_sync),
        .enemy3_active(enemy3_active_sync),
        .enemy3_flip(enemy3_flip_sync),
        .enemy3_attacking(enemy3_attacking_sync),
        .enemy3_hit(enemy3_hit_sync),
        .enemy3_type(enemy3_type_sync),

        // Enemy 4 position and state (synchronized to 25 MHz)
        .enemy4_x(enemy4_x_sync),
        .enemy4_y(enemy4_y_sync),
        .enemy4_frame(enemy4_frame_sync),
        .enemy4_active(enemy4_active_sync),
        .enemy4_flip(enemy4_flip_sync),
        .enemy4_attacking(enemy4_attacking_sync),
        .enemy4_hit(enemy4_hit_sync),
        .enemy4_type(enemy4_type_sync),

        // Enemy ROM interface - RGB222 packed format
        .enemy_rom_addr(enemy_rom_addr),
        .enemy_rom_data(enemy_rom_data),

        // Projectiles (synchronized to 25 MHz)
        .proj_0(proj_0_sync),
        .proj_1(proj_1_sync),
        .proj_2(proj_2_sync),
        .proj_3(proj_3_sync),
        .proj_4(proj_4_sync),
        .proj_5(proj_5_sync),
        .proj_6(proj_6_sync),
        .proj_7(proj_7_sync),
        .proj_8(proj_8_sync),
        .proj_9(proj_9_sync),
        .proj_10(proj_10_sync),
        .proj_11(proj_11_sync),
        .proj_12(proj_12_sync),
        .proj_13(proj_13_sync),
        .proj_14(proj_14_sync),
        .proj_15(proj_15_sync),

        // Player projectile animation frame (synchronized to 25 MHz)
        .player_proj_frame(player_proj_frame_sync),

        // Player health for status bar (synchronized to 25 MHz)
        .player_health(player_health_sync),

        // Player armor for armor bar (synchronized to 25 MHz)
        .player_armor(player_armor_sync),

        // Game state for UI rendering (synchronized to 25 MHz)
        .game_state(game_state_sync),
        .menu_selection(menu_selection_sync),

        // Boss position and state (synchronized to 25 MHz)
        .boss_x(boss_x_sync),
        .boss_y(boss_y_sync),
        .boss_frame(boss_frame_sync),
        .boss_active(boss_active_sync),
        .boss_flip(boss_flip_sync),
        .boss_hit(boss_hit_sync),
        .boss_health(boss_health_sync),

        // Boss ROM interface - RGB222 packed format
        .boss_rom_addr(boss_rom_addr),
        .boss_rom_data(boss_rom_data),

        // Shuriken ROM interface - RGB444 packed format (player projectile)
        .shuriken_rom_addr(shuriken_rom_addr),
        .shuriken_rom_data(shuriken_rom_data),

        // Direct RGB output (RGB444 for tiles, RGB222 expanded to RGB444 for sprites)
        .red_out(tilemap_red),
        .green_out(tilemap_green),
        .blue_out(tilemap_blue),
        .pixel_valid(tilemap_valid),
        .is_sprite_pixel(is_sprite_pixel)
    );

    //=========================================================================
    // Final RGB Output - Expand RGB444 to RGB888 for HDMI
    //=========================================================================
    // RGB444 (4-4-4 bits) -> RGB888 (8-8-8 bits)
    // Each 4-bit channel -> 8-bit by replicating: {R4, R4}
    //
    // Example expansions:
    //   4'b1111 -> 8'b11111111 (255)
    //   4'b1000 -> 8'b10001000 (136)
    //   4'b0000 -> 8'b00000000 (0)

    logic [7:0] red_expanded, green_expanded, blue_expanded;

    // Expand 4-bit to 8-bit: {R4, R4} = replicate
    assign red_expanded   = {tilemap_red,   tilemap_red};
    assign green_expanded = {tilemap_green, tilemap_green};
    assign blue_expanded  = {tilemap_blue,  tilemap_blue};

    //=========================================================================
    // AXI Interface
    //=========================================================================

    // Dummy signals for unused BRAM interfaces (kept for AXI compatibility)
    logic        tilemap_bram_ena;
    logic [3:0]  tilemap_bram_wea;
    logic [11:0] tilemap_bram_addra;
    logic [31:0] tilemap_bram_dina;
    logic [31:0] tilemap_bram_douta;
    assign tilemap_bram_douta = 32'b0;

    logic        entity_bram_ena;
    logic [3:0]  entity_bram_wea;
    logic [7:0]  entity_bram_addra;
    logic [31:0] entity_bram_dina;
    logic [31:0] entity_bram_douta;
    assign entity_bram_douta = 32'b0;

    logic [C_S_AXI_DATA_WIDTH-1:0] color_palette[8];

    game_graphics_controller_v1_0_AXI #(
        .C_S_AXI_DATA_WIDTH(C_S_AXI_DATA_WIDTH),
        .C_S_AXI_ADDR_WIDTH(C_S_AXI_ADDR_WIDTH)
    ) axi_interface (
        // BRAM interfaces (unused, but kept for interface compatibility)
        .tilemap_bram_ena(tilemap_bram_ena),
        .tilemap_bram_wea(tilemap_bram_wea),
        .tilemap_bram_addra(tilemap_bram_addra),
        .tilemap_bram_dina(tilemap_bram_dina),
        .tilemap_bram_douta(tilemap_bram_douta),

        .entity_bram_ena(entity_bram_ena),
        .entity_bram_wea(entity_bram_wea),
        .entity_bram_addra(entity_bram_addra),
        .entity_bram_dina(entity_bram_dina),
        .entity_bram_douta(entity_bram_douta),

        // Color palette (kept for compatibility, not used)
        .color_palette_out(color_palette),

        // Level offset
        .level_offset_out(level_offset),

        // Player position and frame
        .player_x_out(player_x),
        .player_y_out(player_y),
        .player_frame_out(player_frame),

        // Enemy 0 position and state
        .enemy0_x_out(enemy0_x),
        .enemy0_y_out(enemy0_y),
        .enemy0_frame_out(enemy0_frame),
        .enemy0_active_out(enemy0_active),
        .enemy0_flip_out(enemy0_flip),
        .enemy0_attacking_out(enemy0_attacking),
        .enemy0_hit_out(enemy0_hit),
        .enemy0_type_out(enemy0_type),

        // Enemy 1 position and state
        .enemy1_x_out(enemy1_x),
        .enemy1_y_out(enemy1_y),
        .enemy1_frame_out(enemy1_frame),
        .enemy1_active_out(enemy1_active),
        .enemy1_flip_out(enemy1_flip),
        .enemy1_attacking_out(enemy1_attacking),
        .enemy1_hit_out(enemy1_hit),
        .enemy1_type_out(enemy1_type),

        // Enemy 2 position and state
        .enemy2_x_out(enemy2_x),
        .enemy2_y_out(enemy2_y),
        .enemy2_frame_out(enemy2_frame),
        .enemy2_active_out(enemy2_active),
        .enemy2_flip_out(enemy2_flip),
        .enemy2_attacking_out(enemy2_attacking),
        .enemy2_hit_out(enemy2_hit),
        .enemy2_type_out(enemy2_type),

        // Enemy 3 position and state
        .enemy3_x_out(enemy3_x),
        .enemy3_y_out(enemy3_y),
        .enemy3_frame_out(enemy3_frame),
        .enemy3_active_out(enemy3_active),
        .enemy3_flip_out(enemy3_flip),
        .enemy3_attacking_out(enemy3_attacking),
        .enemy3_hit_out(enemy3_hit),
        .enemy3_type_out(enemy3_type),

        // Enemy 4 position and state
        .enemy4_x_out(enemy4_x),
        .enemy4_y_out(enemy4_y),
        .enemy4_frame_out(enemy4_frame),
        .enemy4_active_out(enemy4_active),
        .enemy4_flip_out(enemy4_flip),
        .enemy4_attacking_out(enemy4_attacking),
        .enemy4_hit_out(enemy4_hit),
        .enemy4_type_out(enemy4_type),

        // Projectile data (packed format)
        .proj_0_out(proj_0),
        .proj_1_out(proj_1),
        .proj_2_out(proj_2),
        .proj_3_out(proj_3),
        .proj_4_out(proj_4),
        .proj_5_out(proj_5),
        .proj_6_out(proj_6),
        .proj_7_out(proj_7),
        .proj_8_out(proj_8),
        .proj_9_out(proj_9),
        .proj_10_out(proj_10),
        .proj_11_out(proj_11),
        .proj_12_out(proj_12),
        .proj_13_out(proj_13),
        .proj_14_out(proj_14),
        .proj_15_out(proj_15),

        // Player health
        .player_health_out(player_health),

        // Player projectile animation frame
        .player_proj_frame_out(player_proj_frame),

        // Player armor
        .player_armor_out(player_armor),

        // Game state
        .game_state_out(game_state),
        .menu_selection_out(menu_selection),
        .map_select_out(map_select),
        .breach_open_out(breach_open),
        .breach_direction_out(breach_direction),

        // Boss position and state
        .boss_x_out(boss_x),
        .boss_y_out(boss_y),
        .boss_frame_out(boss_frame),
        .boss_active_out(boss_active),
        .boss_flip_out(boss_flip),
        .boss_hit_out(boss_hit),
        .boss_health_out(boss_health),

        // VGA timing signals
        .drawX(drawX),
        .drawY(drawY),
        .vsync(internal_vsync),

        // AXI interface
        .S_AXI_ACLK(s_axi_aclk),
        .S_AXI_ARESETN(s_axi_aresetn),
        .S_AXI_AWADDR(s_axi_awaddr),
        .S_AXI_AWPROT(s_axi_awprot),
        .S_AXI_AWVALID(s_axi_awvalid),
        .S_AXI_AWREADY(s_axi_awready),
        .S_AXI_WDATA(s_axi_wdata),
        .S_AXI_WSTRB(s_axi_wstrb),
        .S_AXI_WVALID(s_axi_wvalid),
        .S_AXI_WREADY(s_axi_wready),
        .S_AXI_BRESP(s_axi_bresp),
        .S_AXI_BVALID(s_axi_bvalid),
        .S_AXI_BREADY(s_axi_bready),
        .S_AXI_ARADDR(s_axi_araddr),
        .S_AXI_ARPROT(s_axi_arprot),
        .S_AXI_ARVALID(s_axi_arvalid),
        .S_AXI_ARREADY(s_axi_arready),
        .S_AXI_RDATA(s_axi_rdata),
        .S_AXI_RRESP(s_axi_rresp),
        .S_AXI_RVALID(s_axi_rvalid),
        .S_AXI_RREADY(s_axi_rready)
    );

    //=========================================================================
    // HDMI TX - Convert RGB to HDMI
    //=========================================================================

    hdmi_tx_0 vga_to_hdmi (
        .pix_clk(clk_25MHz),
        .pix_clkx5(clk_125MHz),
        .pix_clk_locked(locked),
        .rst(reset_ah),
        .red(red_expanded),       // 8-bit expanded from 4-bit
        .green(green_expanded),   // 8-bit expanded from 4-bit
        .blue(blue_expanded),     // 8-bit expanded from 4-bit
        .hsync(internal_hsync),
        .vsync(internal_vsync),
        .vde(active_nblank),
        .TMDS_CLK_P(hdmi_clk_p),
        .TMDS_CLK_N(hdmi_clk_n),
        .TMDS_DATA_P(hdmi_tx_p),
        .TMDS_DATA_N(hdmi_tx_n)
    );

endmodule
