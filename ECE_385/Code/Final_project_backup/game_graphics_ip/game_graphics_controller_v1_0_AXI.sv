`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// Company: ECE-Illinois
// Engineer: ECE 385 Student
//
// Create Date: 2025-12-03
// Module Name: game_graphics_controller_v1_0_AXI
// Project Name: ECE 385 - Soul Knight Final Project
// Description:
//   AXI4-Lite slave interface for game graphics controller.
//   Provides memory-mapped access to:
//     - Entity List BRAM (64 entities)
//     - Tilemap BRAM (reserved for future use)
//     - Control/Status Registers
//
//   NOTE: Color palette REMOVED - using direct RGB from tile BRAM
//
// Memory Map (BYTE addresses):
//   0x0000-0x03FF: Entity List BRAM (1KB, 64 entities x 16 bytes)
//   0x1000-0x14AF: Tilemap BRAM (reserved, tilemap is hardcoded)
//   0x3000: PLAYER_X (read/write, 10-bit) - Player top-left X position
//   0x3004: PLAYER_Y (read/write, 10-bit) - Player top-left Y position
//   0x3008: PLAYER_FRAME (read/write, 7-bit) - Player animation frame
//   0x3020: FRAME_COUNTER (read-only)
//   0x3024: CURRENT_DRAW_X (read-only)
//   0x3028: CURRENT_DRAW_Y (read-only)
//   0x302C: LEVEL_OFFSET (read/write, 8-bit)
//   0x3030: ENEMY0_X (read/write, 10-bit) - Enemy 0 X position
//   0x3034: ENEMY0_Y (read/write, 10-bit) - Enemy 0 Y position
//   0x3038: ENEMY0_FRAME (read/write, 6-bit) - Enemy 0 animation frame (0-7)
//   0x303C: ENEMY0_ACTIVE (read/write) - {24'b0, type[7:4], hit[3], attacking[2], flip[1], active[0]}
//   0x3040: PROJ_0 (read/write) - Projectile 0: {active[31], unused[30:26], y[25:16], unused[15:10], x[9:0]}
//   0x3044: PROJ_1 (read/write) - Projectile 1
//   0x3048: PROJ_2 (read/write) - Projectile 2
//   0x304C: PROJ_3 (read/write) - Projectile 3
//   0x3050: PLAYER_HEALTH (read/write, 3-bit) - Player health (0-6)
//   0x3054: PLAYER_ARMOR (read/write, 2-bit) - Player armor (0-3)
//   0x3060: ENEMY1_X (read/write, 10-bit) - Enemy 1 X position
//   0x3064: ENEMY1_Y (read/write, 10-bit) - Enemy 1 Y position
//   0x3068: ENEMY1_FRAME (read/write, 6-bit) - Enemy 1 animation frame (0-7)
//   0x306C: ENEMY1_ACTIVE (read/write) - {24'b0, type[7:4], hit[3], attacking[2], flip[1], active[0]}
//   0x3070-0x307C: Enemy 2 registers (same format as Enemy 0/1)
//   0x3080-0x308C: Enemy 3 registers
//   0x3090-0x309C: Enemy 4 registers
//   0x30A0: GAME_STATE (read/write, 2-bit) - 0=MENU, 1=PLAYING, 2=GAME_OVER
//   0x30A4: MENU_SELECT (read/write, 1-bit) - 0=first option, 1=second option
//   0x30A8: MAP_SELECT (read/write, 3-bit) - 0=empty, 1=I-shape, 2=cross, 3=pillars, 4=stair
//   0x30AC: BREACH_OPEN (read/write, 1-bit) - 0=closed, 1=open
//   0x30B0: BREACH_DIRECTION (read/write, 2-bit) - 0=RIGHT, 1=LEFT, 2=UP, 3=DOWN
//   0x30B4: BOSS_X (read/write, 10-bit) - Boss X position
//   0x30B8: BOSS_Y (read/write, 10-bit) - Boss Y position
//   0x30BC: BOSS_FRAME (read/write, 5-bit) - Boss animation frame (0-19)
//   0x30C0: BOSS_CONTROL (read/write) - {hit[2], flip[1], active[0]}
//   0x30C4: BOSS_HEALTH (read/write, 8-bit) - Boss health (0-255)
//   0x30D0-0x310C: PROJ_4 to PROJ_15 (read/write) - Extended projectiles
//   0x3110: PLAYER_PROJ_FRAME (read/write, 2-bit) - Player projectile animation frame (0-3)
//
//////////////////////////////////////////////////////////////////////////////////

module game_graphics_controller_v1_0_AXI #
(
    parameter integer C_S_AXI_DATA_WIDTH = 32,
    parameter integer C_S_AXI_ADDR_WIDTH = 14
)
(
    // Entity BRAM Port A interface
    output logic        entity_bram_ena,
    output logic [3:0]  entity_bram_wea,
    output logic [7:0]  entity_bram_addra,
    output logic [31:0] entity_bram_dina,
    input  logic [31:0] entity_bram_douta,

    // Tilemap BRAM Port A interface (reserved for future use)
    output logic        tilemap_bram_ena,
    output logic [3:0]  tilemap_bram_wea,
    output logic [11:0] tilemap_bram_addra,
    output logic [31:0] tilemap_bram_dina,
    input  logic [31:0] tilemap_bram_douta,

    // Color palette output - KEPT FOR INTERFACE COMPATIBILITY BUT NOT USED
    output logic [C_S_AXI_DATA_WIDTH-1:0] color_palette_out[8],

    // Level offset output (for tilemap renderer)
    output logic [7:0] level_offset_out,

    // Player position outputs (for renderer)
    output logic [9:0] player_x_out,
    output logic [9:0] player_y_out,
    output logic [6:0] player_frame_out,  // 7-bit for frames 0-83 (movement + attack)

    // Enemy 0 outputs (for renderer)
    output logic [9:0] enemy0_x_out,
    output logic [9:0] enemy0_y_out,
    output logic [2:0] enemy0_frame_out,   // 3-bit for frames 0-7
    output logic       enemy0_active_out,
    output logic       enemy0_flip_out,    // Horizontal flip
    output logic       enemy0_attacking_out, // Blue tint when attacking (melee dash)
    output logic       enemy0_hit_out,     // Red tint when hit by player
    output logic [3:0] enemy0_type_out,    // Enemy type for BRAM offset

    // Enemy 1 outputs (for renderer)
    output logic [9:0] enemy1_x_out,
    output logic [9:0] enemy1_y_out,
    output logic [2:0] enemy1_frame_out,   // 3-bit for frames 0-7
    output logic       enemy1_active_out,
    output logic       enemy1_flip_out,    // Horizontal flip
    output logic       enemy1_attacking_out, // Blue tint when attacking (melee dash)
    output logic       enemy1_hit_out,     // Red tint when hit by player
    output logic [3:0] enemy1_type_out,    // Enemy type for BRAM offset

    // Enemy 2 outputs (for renderer)
    output logic [9:0] enemy2_x_out,
    output logic [9:0] enemy2_y_out,
    output logic [2:0] enemy2_frame_out,
    output logic       enemy2_active_out,
    output logic       enemy2_flip_out,
    output logic       enemy2_attacking_out,
    output logic       enemy2_hit_out,
    output logic [3:0] enemy2_type_out,

    // Enemy 3 outputs (for renderer)
    output logic [9:0] enemy3_x_out,
    output logic [9:0] enemy3_y_out,
    output logic [2:0] enemy3_frame_out,
    output logic       enemy3_active_out,
    output logic       enemy3_flip_out,
    output logic       enemy3_attacking_out,
    output logic       enemy3_hit_out,
    output logic [3:0] enemy3_type_out,

    // Enemy 4 outputs (for renderer)
    output logic [9:0] enemy4_x_out,
    output logic [9:0] enemy4_y_out,
    output logic [2:0] enemy4_frame_out,
    output logic       enemy4_active_out,
    output logic       enemy4_flip_out,
    output logic       enemy4_attacking_out,
    output logic       enemy4_hit_out,
    output logic [3:0] enemy4_type_out,

    // Projectile outputs (for renderer) - packed format
    output logic [31:0] proj_0_out,       // {active[31], y[25:16], x[9:0]}
    output logic [31:0] proj_1_out,
    output logic [31:0] proj_2_out,
    output logic [31:0] proj_3_out,
    output logic [31:0] proj_4_out,
    output logic [31:0] proj_5_out,
    output logic [31:0] proj_6_out,
    output logic [31:0] proj_7_out,
    output logic [31:0] proj_8_out,
    output logic [31:0] proj_9_out,
    output logic [31:0] proj_10_out,
    output logic [31:0] proj_11_out,
    output logic [31:0] proj_12_out,
    output logic [31:0] proj_13_out,
    output logic [31:0] proj_14_out,
    output logic [31:0] proj_15_out,

    // Player health output (for status bar rendering)
    output logic [2:0] player_health_out, // 0-6 hearts

    // Player armor output (for armor bar rendering)
    output logic [1:0] player_armor_out,  // 0-3 armor points

    // Game state outputs (for UI rendering)
    output logic [1:0] game_state_out,     // 0=MENU, 1=PLAYING, 2=GAME_OVER
    output logic       menu_selection_out, // 0=first option, 1=second option
    output logic [2:0] map_select_out,     // 0-4: empty, I-shape, cross, pillars, stair
    output logic       breach_open_out,    // 1=breach is open (door visible)
    output logic [1:0] breach_direction_out, // 0=RIGHT, 1=LEFT, 2=UP, 3=DOWN

    // Boss outputs (for renderer)
    output logic [9:0] boss_x_out,
    output logic [9:0] boss_y_out,
    output logic [4:0] boss_frame_out,     // 5-bit for frames 0-19
    output logic       boss_active_out,
    output logic       boss_flip_out,
    output logic       boss_hit_out,
    output logic [7:0] boss_health_out,

    // Player projectile animation frame output
    output logic [1:0] player_proj_frame_out,  // 2-bit for frames 0-3

    // VGA timing inputs for status registers
    input logic [9:0] drawX,
    input logic [9:0] drawY,
    input logic vsync,

    // AXI4-Lite Interface
    input  logic S_AXI_ACLK,
    input  logic S_AXI_ARESETN,
    input  logic [C_S_AXI_ADDR_WIDTH-1:0] S_AXI_AWADDR,
    input  logic [2:0] S_AXI_AWPROT,
    input  logic S_AXI_AWVALID,
    output logic S_AXI_AWREADY,
    input  logic [C_S_AXI_DATA_WIDTH-1:0] S_AXI_WDATA,
    input  logic [(C_S_AXI_DATA_WIDTH/8)-1:0] S_AXI_WSTRB,
    input  logic S_AXI_WVALID,
    output logic S_AXI_WREADY,
    output logic [1:0] S_AXI_BRESP,
    output logic S_AXI_BVALID,
    input  logic S_AXI_BREADY,
    input  logic [C_S_AXI_ADDR_WIDTH-1:0] S_AXI_ARADDR,
    input  logic [2:0] S_AXI_ARPROT,
    input  logic S_AXI_ARVALID,
    output logic S_AXI_ARREADY,
    output logic [C_S_AXI_DATA_WIDTH-1:0] S_AXI_RDATA,
    output logic [1:0] S_AXI_RRESP,
    output logic S_AXI_RVALID,
    input  logic S_AXI_RREADY
);

    //=========================================================================
    // AXI4-Lite signals
    //=========================================================================
    logic [C_S_AXI_ADDR_WIDTH-1:0] axi_awaddr;
    logic axi_awready;
    logic axi_wready;
    logic [1:0] axi_bresp;
    logic axi_bvalid;
    logic [C_S_AXI_ADDR_WIDTH-1:0] axi_araddr;
    logic axi_arready;
    logic [C_S_AXI_DATA_WIDTH-1:0] axi_rdata;
    logic [1:0] axi_rresp;
    logic axi_rvalid;

    //=========================================================================
    // Internal Registers and Signals
    //=========================================================================

    // Control registers
    logic [7:0] level_offset;
    logic [9:0] player_x;
    logic [9:0] player_y;
    logic [6:0] player_frame;  // 7-bit for frames 0-83

    // Enemy 0 registers
    logic [9:0] enemy0_x;
    logic [9:0] enemy0_y;
    logic [2:0] enemy0_frame;   // 3-bit for frames 0-7
    logic       enemy0_active;
    logic       enemy0_flip;
    logic       enemy0_attacking;
    logic       enemy0_hit;
    logic [3:0] enemy0_type;

    // Enemy 1 registers
    logic [9:0] enemy1_x;
    logic [9:0] enemy1_y;
    logic [2:0] enemy1_frame;   // 3-bit for frames 0-7
    logic       enemy1_active;
    logic       enemy1_flip;
    logic       enemy1_attacking;
    logic       enemy1_hit;
    logic [3:0] enemy1_type;

    // Enemy 2 registers
    logic [9:0] enemy2_x;
    logic [9:0] enemy2_y;
    logic [2:0] enemy2_frame;
    logic       enemy2_active;
    logic       enemy2_flip;
    logic       enemy2_attacking;
    logic       enemy2_hit;
    logic [3:0] enemy2_type;

    // Enemy 3 registers
    logic [9:0] enemy3_x;
    logic [9:0] enemy3_y;
    logic [2:0] enemy3_frame;
    logic       enemy3_active;
    logic       enemy3_flip;
    logic       enemy3_attacking;
    logic       enemy3_hit;
    logic [3:0] enemy3_type;

    // Enemy 4 registers
    logic [9:0] enemy4_x;
    logic [9:0] enemy4_y;
    logic [2:0] enemy4_frame;
    logic       enemy4_active;
    logic       enemy4_flip;
    logic       enemy4_attacking;
    logic       enemy4_hit;
    logic [3:0] enemy4_type;

    // Projectile registers (packed format)
    logic [31:0] proj_0;       // {active[31], y[25:16], x[9:0]}
    logic [31:0] proj_1;
    logic [31:0] proj_2;
    logic [31:0] proj_3;
    logic [31:0] proj_4;
    logic [31:0] proj_5;
    logic [31:0] proj_6;
    logic [31:0] proj_7;
    logic [31:0] proj_8;
    logic [31:0] proj_9;
    logic [31:0] proj_10;
    logic [31:0] proj_11;
    logic [31:0] proj_12;
    logic [31:0] proj_13;
    logic [31:0] proj_14;
    logic [31:0] proj_15;

    // Player health register
    logic [2:0] player_health; // 0-6 hearts

    // Player armor register
    logic [1:0] player_armor;  // 0-3 armor points

    // Game state registers
    logic [1:0] game_state;      // 0=MENU, 1=PLAYING, 2=GAME_OVER
    logic       menu_selection;  // 0=first option, 1=second option
    logic [2:0] map_select;      // 0-4: empty, I-shape, cross, pillars, stair
    logic       breach_open;     // 1=breach is open (door visible)
    logic [1:0] breach_direction; // 0=RIGHT, 1=LEFT, 2=UP, 3=DOWN

    // Boss registers
    logic [9:0] boss_x;
    logic [9:0] boss_y;
    logic [4:0] boss_frame;      // 5-bit for frames 0-19
    logic       boss_active;
    logic       boss_flip;
    logic       boss_hit;
    logic [7:0] boss_health;

    // Player projectile animation frame register
    logic [1:0] player_proj_frame;  // 2-bit for frames 0-3

    // Status registers
    logic [C_S_AXI_DATA_WIDTH-1:0] frame_counter;
    logic [C_S_AXI_DATA_WIDTH-1:0] current_draw_x;
    logic [C_S_AXI_DATA_WIDTH-1:0] current_draw_y;

    // BRAM control signals
    logic entity_bram_en_reg;
    logic [3:0] entity_bram_we_reg;
    logic [7:0] entity_bram_addr_reg;
    logic [31:0] entity_bram_din_reg;

    logic tilemap_bram_en_reg;
    logic [3:0] tilemap_bram_we_reg;
    logic [11:0] tilemap_bram_addr_reg;
    logic [31:0] tilemap_bram_din_reg;

    // AXI control signals
    logic slv_reg_rden;
    logic slv_reg_wren;
    logic [C_S_AXI_DATA_WIDTH-1:0] reg_data_out;
    logic aw_en;

    // Vsync edge detection for frame counter
    logic vsync_prev;
    logic vsync_rising_edge;

    // BRAM read latency: 1 cycle delay
    logic read_in_progress;

    //=========================================================================
    // Address Decode
    //=========================================================================
    logic [13:0] write_byte_addr;
    logic [13:0] read_byte_addr;

    assign write_byte_addr = axi_awaddr[13:0];
    assign read_byte_addr = axi_araddr[13:0];

    // Decode address regions for writes
    logic is_entity_region_write;
    logic is_tilemap_region_write;

    always_comb begin
        is_entity_region_write  = (write_byte_addr >= 14'h0000) && (write_byte_addr < 14'h0400);
        is_tilemap_region_write = (write_byte_addr >= 14'h1000) && (write_byte_addr < 14'h14B0);
    end

    // Decode address regions for reads
    logic is_entity_region_read;
    logic is_tilemap_region_read;
    logic is_control_region_read;

    always_comb begin
        is_entity_region_read  = (read_byte_addr >= 14'h0000) && (read_byte_addr < 14'h0400);
        is_tilemap_region_read = (read_byte_addr >= 14'h1000) && (read_byte_addr < 14'h14B0);
        is_control_region_read = (read_byte_addr >= 14'h3000) && (read_byte_addr < 14'h3120);  // Extended for boss + projectiles + player proj frame
    end

    //=========================================================================
    // AXI Interface Assignments
    //=========================================================================
    assign S_AXI_AWREADY = axi_awready;
    assign S_AXI_WREADY = axi_wready;
    assign S_AXI_BRESP = axi_bresp;
    assign S_AXI_BVALID = axi_bvalid;
    assign S_AXI_ARREADY = axi_arready;
    assign S_AXI_RDATA = axi_rdata;
    assign S_AXI_RRESP = axi_rresp;
    assign S_AXI_RVALID = axi_rvalid;

    // BRAM outputs
    assign entity_bram_ena = entity_bram_en_reg;
    assign entity_bram_wea = entity_bram_we_reg;
    assign entity_bram_addra = entity_bram_addr_reg;
    assign entity_bram_dina = entity_bram_din_reg;

    assign tilemap_bram_ena = tilemap_bram_en_reg;
    assign tilemap_bram_wea = tilemap_bram_we_reg;
    assign tilemap_bram_addra = tilemap_bram_addr_reg;
    assign tilemap_bram_dina = tilemap_bram_din_reg;

    // Color palette - output zeros (not used, kept for interface compatibility)
    assign color_palette_out[0] = 32'h0;
    assign color_palette_out[1] = 32'h0;
    assign color_palette_out[2] = 32'h0;
    assign color_palette_out[3] = 32'h0;
    assign color_palette_out[4] = 32'h0;
    assign color_palette_out[5] = 32'h0;
    assign color_palette_out[6] = 32'h0;
    assign color_palette_out[7] = 32'h0;

    assign level_offset_out = level_offset;
    assign player_x_out = player_x;
    assign player_y_out = player_y;
    assign player_frame_out = player_frame;

    // Enemy 0 outputs
    assign enemy0_x_out = enemy0_x;
    assign enemy0_y_out = enemy0_y;
    assign enemy0_frame_out = enemy0_frame;
    assign enemy0_active_out = enemy0_active;
    assign enemy0_flip_out = enemy0_flip;
    assign enemy0_attacking_out = enemy0_attacking;
    assign enemy0_hit_out = enemy0_hit;
    assign enemy0_type_out = enemy0_type;

    // Enemy 1 outputs
    assign enemy1_x_out = enemy1_x;
    assign enemy1_y_out = enemy1_y;
    assign enemy1_frame_out = enemy1_frame;
    assign enemy1_active_out = enemy1_active;
    assign enemy1_flip_out = enemy1_flip;
    assign enemy1_attacking_out = enemy1_attacking;
    assign enemy1_hit_out = enemy1_hit;
    assign enemy1_type_out = enemy1_type;

    assign enemy2_x_out = enemy2_x;
    assign enemy2_y_out = enemy2_y;
    assign enemy2_frame_out = enemy2_frame;
    assign enemy2_active_out = enemy2_active;
    assign enemy2_flip_out = enemy2_flip;
    assign enemy2_attacking_out = enemy2_attacking;
    assign enemy2_hit_out = enemy2_hit;
    assign enemy2_type_out = enemy2_type;

    assign enemy3_x_out = enemy3_x;
    assign enemy3_y_out = enemy3_y;
    assign enemy3_frame_out = enemy3_frame;
    assign enemy3_active_out = enemy3_active;
    assign enemy3_flip_out = enemy3_flip;
    assign enemy3_attacking_out = enemy3_attacking;
    assign enemy3_hit_out = enemy3_hit;
    assign enemy3_type_out = enemy3_type;

    assign enemy4_x_out = enemy4_x;
    assign enemy4_y_out = enemy4_y;
    assign enemy4_frame_out = enemy4_frame;
    assign enemy4_active_out = enemy4_active;
    assign enemy4_flip_out = enemy4_flip;
    assign enemy4_attacking_out = enemy4_attacking;
    assign enemy4_hit_out = enemy4_hit;
    assign enemy4_type_out = enemy4_type;

    // Projectile outputs
    assign proj_0_out = proj_0;
    assign proj_1_out = proj_1;
    assign proj_2_out = proj_2;
    assign proj_3_out = proj_3;
    assign proj_4_out = proj_4;
    assign proj_5_out = proj_5;
    assign proj_6_out = proj_6;
    assign proj_7_out = proj_7;
    assign proj_8_out = proj_8;
    assign proj_9_out = proj_9;
    assign proj_10_out = proj_10;
    assign proj_11_out = proj_11;
    assign proj_12_out = proj_12;
    assign proj_13_out = proj_13;
    assign proj_14_out = proj_14;
    assign proj_15_out = proj_15;

    // Player health output
    assign player_health_out = player_health;

    // Player armor output
    assign player_armor_out = player_armor;

    // Game state outputs
    assign game_state_out = game_state;
    assign menu_selection_out = menu_selection;
    assign map_select_out = map_select;
    assign breach_open_out = breach_open;
    assign breach_direction_out = breach_direction;

    // Boss outputs
    assign boss_x_out = boss_x;
    assign boss_y_out = boss_y;
    assign boss_frame_out = boss_frame;
    assign boss_active_out = boss_active;
    assign boss_flip_out = boss_flip;
    assign boss_hit_out = boss_hit;
    assign boss_health_out = boss_health;

    // Player projectile frame output
    assign player_proj_frame_out = player_proj_frame;

    //=========================================================================
    // AXI Write Address Channel
    //=========================================================================
    always_ff @(posedge S_AXI_ACLK) begin
        if (~S_AXI_ARESETN) begin
            axi_awready <= 1'b0;
            aw_en <= 1'b1;
        end else begin
            if (~axi_awready && S_AXI_AWVALID && S_AXI_WVALID && aw_en) begin
                axi_awready <= 1'b1;
                aw_en <= 1'b0;
            end else if (S_AXI_BREADY && axi_bvalid) begin
                aw_en <= 1'b1;
                axi_awready <= 1'b0;
            end else begin
                axi_awready <= 1'b0;
            end
        end
    end

    always_ff @(posedge S_AXI_ACLK) begin
        if (~S_AXI_ARESETN) begin
            axi_awaddr <= '0;
        end else begin
            if (~axi_awready && S_AXI_AWVALID && S_AXI_WVALID && aw_en) begin
                axi_awaddr <= S_AXI_AWADDR;
            end
        end
    end

    //=========================================================================
    // AXI Write Data Channel
    //=========================================================================
    always_ff @(posedge S_AXI_ACLK) begin
        if (~S_AXI_ARESETN) begin
            axi_wready <= 1'b0;
        end else begin
            if (~axi_wready && S_AXI_WVALID && S_AXI_AWVALID && aw_en) begin
                axi_wready <= 1'b1;
            end else begin
                axi_wready <= 1'b0;
            end
        end
    end

    assign slv_reg_wren = axi_wready && S_AXI_WVALID && axi_awready && S_AXI_AWVALID;

    //=========================================================================
    // Write Logic
    //=========================================================================
    always_ff @(posedge S_AXI_ACLK) begin
        if (~S_AXI_ARESETN) begin
            // Reset BRAM control
            entity_bram_en_reg <= 1'b0;
            entity_bram_we_reg <= 4'b0000;
            entity_bram_addr_reg <= 8'd0;
            entity_bram_din_reg <= 32'd0;

            tilemap_bram_en_reg <= 1'b0;
            tilemap_bram_we_reg <= 4'b0000;
            tilemap_bram_addr_reg <= 12'd0;
            tilemap_bram_din_reg <= 32'd0;

            // Reset control registers
            level_offset <= 8'd0;
            player_x <= 10'd320;  // Start near center
            player_y <= 10'd240;
            player_frame <= 7'd0;

            // Reset enemy 0 registers
            enemy0_x <= 10'd400;
            enemy0_y <= 10'd150;
            enemy0_frame <= 3'd0;
            enemy0_active <= 1'b0;
            enemy0_flip <= 1'b0;
            enemy0_attacking <= 1'b0;
            enemy0_hit <= 1'b0;
            enemy0_type <= 4'd0;

            // Reset enemy 1 registers
            enemy1_x <= 10'd400;
            enemy1_y <= 10'd300;
            enemy1_frame <= 3'd0;
            enemy1_active <= 1'b0;
            enemy1_flip <= 1'b0;
            enemy1_attacking <= 1'b0;
            enemy1_hit <= 1'b0;
            enemy1_type <= 4'd1;

            // Reset enemy 2 registers
            enemy2_x <= 10'd0;
            enemy2_y <= 10'd0;
            enemy2_frame <= 3'd0;
            enemy2_active <= 1'b0;
            enemy2_flip <= 1'b0;
            enemy2_attacking <= 1'b0;
            enemy2_hit <= 1'b0;
            enemy2_type <= 4'd2;

            // Reset enemy 3 registers
            enemy3_x <= 10'd0;
            enemy3_y <= 10'd0;
            enemy3_frame <= 3'd0;
            enemy3_active <= 1'b0;
            enemy3_flip <= 1'b0;
            enemy3_attacking <= 1'b0;
            enemy3_hit <= 1'b0;
            enemy3_type <= 4'd3;

            // Reset enemy 4 registers
            enemy4_x <= 10'd0;
            enemy4_y <= 10'd0;
            enemy4_frame <= 3'd0;
            enemy4_active <= 1'b0;
            enemy4_flip <= 1'b0;
            enemy4_attacking <= 1'b0;
            enemy4_hit <= 1'b0;
            enemy4_type <= 4'd4;

            // Reset projectile registers (all inactive)
            proj_0 <= 32'd0;
            proj_1 <= 32'd0;
            proj_2 <= 32'd0;
            proj_3 <= 32'd0;
            proj_4 <= 32'd0;
            proj_5 <= 32'd0;
            proj_6 <= 32'd0;
            proj_7 <= 32'd0;
            proj_8 <= 32'd0;
            proj_9 <= 32'd0;
            proj_10 <= 32'd0;
            proj_11 <= 32'd0;
            proj_12 <= 32'd0;
            proj_13 <= 32'd0;
            proj_14 <= 32'd0;
            proj_15 <= 32'd0;

            // Reset player health (full health = 6)
            player_health <= 3'd6;

            // Reset player armor (full armor = 3)
            player_armor <= 2'd3;

            // Reset game state (start in MENU)
            game_state <= 2'd0;       // 0 = MENU
            menu_selection <= 1'b0;   // First option selected
            map_select <= 3'b0;       // Empty room (map 0)
            breach_open <= 1'b0;      // Breach closed
            breach_direction <= 2'b0; // RIGHT direction (default)

            // Reset boss registers
            boss_x <= 10'd400;        // Center-right area
            boss_y <= 10'd200;
            boss_frame <= 5'd0;
            boss_active <= 1'b0;      // Inactive by default
            boss_flip <= 1'b0;
            boss_hit <= 1'b0;
            boss_health <= 8'd30;     // 30 HP

            // Reset player projectile frame
            player_proj_frame <= 2'd0;

            // Reset status registers
            current_draw_x <= 32'd0;
            current_draw_y <= 32'd0;
        end else begin
            // Default: disable BRAM writes
            entity_bram_we_reg <= 4'b0000;
            entity_bram_en_reg <= 1'b0;
            tilemap_bram_we_reg <= 4'b0000;
            tilemap_bram_en_reg <= 1'b0;

            // Update status registers
            current_draw_x <= {22'd0, drawX};
            current_draw_y <= {22'd0, drawY};

            // Handle AXI writes
            if (slv_reg_wren) begin
                if (is_entity_region_write) begin
                    entity_bram_en_reg <= 1'b1;
                    entity_bram_we_reg <= S_AXI_WSTRB;
                    entity_bram_addr_reg <= write_byte_addr[9:2];
                    entity_bram_din_reg <= S_AXI_WDATA;
                end else if (is_tilemap_region_write) begin
                    tilemap_bram_en_reg <= 1'b1;
                    tilemap_bram_we_reg <= S_AXI_WSTRB;
                    tilemap_bram_addr_reg <= (write_byte_addr - 14'h1000) >> 2;
                    tilemap_bram_din_reg <= S_AXI_WDATA;
                end else if (write_byte_addr == 14'h3000) begin
                    player_x <= S_AXI_WDATA[9:0];
                end else if (write_byte_addr == 14'h3004) begin
                    player_y <= S_AXI_WDATA[9:0];
                end else if (write_byte_addr == 14'h3008) begin
                    player_frame <= S_AXI_WDATA[6:0];  // 7-bit for frames 0-83
                end else if (write_byte_addr == 14'h302C) begin
                    level_offset <= S_AXI_WDATA[7:0];
                // Enemy 0 registers
                end else if (write_byte_addr == 14'h3030) begin
                    enemy0_x <= S_AXI_WDATA[9:0];
                end else if (write_byte_addr == 14'h3034) begin
                    enemy0_y <= S_AXI_WDATA[9:0];
                end else if (write_byte_addr == 14'h3038) begin
                    enemy0_frame <= S_AXI_WDATA[2:0];  // 3-bit for frames 0-7
                end else if (write_byte_addr == 14'h303C) begin
                    // Format: {type[7:4], hit[3], attacking[2], flip[1], active[0]}
                    enemy0_active <= S_AXI_WDATA[0];
                    enemy0_flip <= S_AXI_WDATA[1];
                    enemy0_attacking <= S_AXI_WDATA[2];
                    enemy0_hit <= S_AXI_WDATA[3];
                    enemy0_type <= S_AXI_WDATA[7:4];
                // Projectile registers (packed format)
                end else if (write_byte_addr == 14'h3040) begin
                    proj_0 <= S_AXI_WDATA;
                end else if (write_byte_addr == 14'h3044) begin
                    proj_1 <= S_AXI_WDATA;
                end else if (write_byte_addr == 14'h3048) begin
                    proj_2 <= S_AXI_WDATA;
                end else if (write_byte_addr == 14'h304C) begin
                    proj_3 <= S_AXI_WDATA;
                // Player health register
                end else if (write_byte_addr == 14'h3050) begin
                    player_health <= S_AXI_WDATA[2:0];  // 3-bit for 0-6
                // Player armor register
                end else if (write_byte_addr == 14'h3054) begin
                    player_armor <= S_AXI_WDATA[1:0];   // 2-bit for 0-3
                // Enemy 1 registers
                end else if (write_byte_addr == 14'h3060) begin
                    enemy1_x <= S_AXI_WDATA[9:0];
                end else if (write_byte_addr == 14'h3064) begin
                    enemy1_y <= S_AXI_WDATA[9:0];
                end else if (write_byte_addr == 14'h3068) begin
                    enemy1_frame <= S_AXI_WDATA[2:0];  // 3-bit for frames 0-7
                end else if (write_byte_addr == 14'h306C) begin
                    // Format: {type[7:4], hit[3], attacking[2], flip[1], active[0]}
                    enemy1_active <= S_AXI_WDATA[0];
                    enemy1_flip <= S_AXI_WDATA[1];
                    enemy1_attacking <= S_AXI_WDATA[2];
                    enemy1_hit <= S_AXI_WDATA[3];
                    enemy1_type <= S_AXI_WDATA[7:4];
                // Enemy 2 registers (0x3070-0x307C)
                end else if (write_byte_addr == 14'h3070) begin
                    enemy2_x <= S_AXI_WDATA[9:0];
                end else if (write_byte_addr == 14'h3074) begin
                    enemy2_y <= S_AXI_WDATA[9:0];
                end else if (write_byte_addr == 14'h3078) begin
                    enemy2_frame <= S_AXI_WDATA[2:0];
                end else if (write_byte_addr == 14'h307C) begin
                    enemy2_active <= S_AXI_WDATA[0];
                    enemy2_flip <= S_AXI_WDATA[1];
                    enemy2_attacking <= S_AXI_WDATA[2];
                    enemy2_hit <= S_AXI_WDATA[3];
                    enemy2_type <= S_AXI_WDATA[7:4];
                // Enemy 3 registers (0x3080-0x308C)
                end else if (write_byte_addr == 14'h3080) begin
                    enemy3_x <= S_AXI_WDATA[9:0];
                end else if (write_byte_addr == 14'h3084) begin
                    enemy3_y <= S_AXI_WDATA[9:0];
                end else if (write_byte_addr == 14'h3088) begin
                    enemy3_frame <= S_AXI_WDATA[2:0];
                end else if (write_byte_addr == 14'h308C) begin
                    enemy3_active <= S_AXI_WDATA[0];
                    enemy3_flip <= S_AXI_WDATA[1];
                    enemy3_attacking <= S_AXI_WDATA[2];
                    enemy3_hit <= S_AXI_WDATA[3];
                    enemy3_type <= S_AXI_WDATA[7:4];
                // Enemy 4 registers (0x3090-0x309C)
                end else if (write_byte_addr == 14'h3090) begin
                    enemy4_x <= S_AXI_WDATA[9:0];
                end else if (write_byte_addr == 14'h3094) begin
                    enemy4_y <= S_AXI_WDATA[9:0];
                end else if (write_byte_addr == 14'h3098) begin
                    enemy4_frame <= S_AXI_WDATA[2:0];
                end else if (write_byte_addr == 14'h309C) begin
                    enemy4_active <= S_AXI_WDATA[0];
                    enemy4_flip <= S_AXI_WDATA[1];
                    enemy4_attacking <= S_AXI_WDATA[2];
                    enemy4_hit <= S_AXI_WDATA[3];
                    enemy4_type <= S_AXI_WDATA[7:4];
                // Game state registers (0x30A0-0x30A8)
                end else if (write_byte_addr == 14'h30A0) begin
                    game_state <= S_AXI_WDATA[1:0];  // 2-bit: 0=MENU, 1=PLAYING, 2=GAME_OVER
                end else if (write_byte_addr == 14'h30A4) begin
                    menu_selection <= S_AXI_WDATA[0];  // 1-bit: 0=first, 1=second
                end else if (write_byte_addr == 14'h30A8) begin
                    map_select <= S_AXI_WDATA[2:0];   // 3-bit: 0-4 room templates
                end else if (write_byte_addr == 14'h30AC) begin
                    breach_open <= S_AXI_WDATA[0];    // 1-bit: 0=closed, 1=open
                end else if (write_byte_addr == 14'h30B0) begin
                    breach_direction <= S_AXI_WDATA[1:0]; // 2-bit: 0=R, 1=L, 2=U, 3=D
                // Boss registers (0x30B4-0x30C4)
                end else if (write_byte_addr == 14'h30B4) begin
                    boss_x <= S_AXI_WDATA[9:0];
                end else if (write_byte_addr == 14'h30B8) begin
                    boss_y <= S_AXI_WDATA[9:0];
                end else if (write_byte_addr == 14'h30BC) begin
                    boss_frame <= S_AXI_WDATA[4:0];   // 5-bit for frames 0-19
                end else if (write_byte_addr == 14'h30C0) begin
                    // Format: {hit[2], flip[1], active[0]}
                    boss_active <= S_AXI_WDATA[0];
                    boss_flip <= S_AXI_WDATA[1];
                    boss_hit <= S_AXI_WDATA[2];
                end else if (write_byte_addr == 14'h30C4) begin
                    boss_health <= S_AXI_WDATA[7:0];  // 8-bit health
                // Extended projectile registers (0x30D0-0x310C)
                end else if (write_byte_addr == 14'h30D0) begin
                    proj_4 <= S_AXI_WDATA;
                end else if (write_byte_addr == 14'h30D4) begin
                    proj_5 <= S_AXI_WDATA;
                end else if (write_byte_addr == 14'h30D8) begin
                    proj_6 <= S_AXI_WDATA;
                end else if (write_byte_addr == 14'h30DC) begin
                    proj_7 <= S_AXI_WDATA;
                end else if (write_byte_addr == 14'h30E0) begin
                    proj_8 <= S_AXI_WDATA;
                end else if (write_byte_addr == 14'h30E4) begin
                    proj_9 <= S_AXI_WDATA;
                end else if (write_byte_addr == 14'h30E8) begin
                    proj_10 <= S_AXI_WDATA;
                end else if (write_byte_addr == 14'h30EC) begin
                    proj_11 <= S_AXI_WDATA;
                end else if (write_byte_addr == 14'h30F0) begin
                    proj_12 <= S_AXI_WDATA;
                end else if (write_byte_addr == 14'h30F4) begin
                    proj_13 <= S_AXI_WDATA;
                end else if (write_byte_addr == 14'h30F8) begin
                    proj_14 <= S_AXI_WDATA;
                end else if (write_byte_addr == 14'h30FC) begin
                    proj_15 <= S_AXI_WDATA;
                // Player projectile frame register (0x3110)
                end else if (write_byte_addr == 14'h3110) begin
                    player_proj_frame <= S_AXI_WDATA[1:0];  // 2-bit frame (0-3)
                end
            end else if (slv_reg_rden) begin
                if (is_entity_region_read) begin
                    entity_bram_en_reg <= 1'b1;
                    entity_bram_we_reg <= 4'b0000;
                    entity_bram_addr_reg <= read_byte_addr[9:2];
                end else if (is_tilemap_region_read) begin
                    tilemap_bram_en_reg <= 1'b1;
                    tilemap_bram_we_reg <= 4'b0000;
                    tilemap_bram_addr_reg <= (read_byte_addr - 14'h1000) >> 2;
                end
            end
        end
    end

    //=========================================================================
    // AXI Write Response Channel
    //=========================================================================
    always_ff @(posedge S_AXI_ACLK) begin
        if (~S_AXI_ARESETN) begin
            axi_bvalid <= 1'b0;
            axi_bresp <= 2'b00;
        end else begin
            if (axi_awready && S_AXI_AWVALID && ~axi_bvalid && axi_wready && S_AXI_WVALID) begin
                axi_bvalid <= 1'b1;
                axi_bresp <= 2'b00;
            end else begin
                if (S_AXI_BREADY && axi_bvalid) begin
                    axi_bvalid <= 1'b0;
                end
            end
        end
    end

    //=========================================================================
    // AXI Read Address Channel
    //=========================================================================
    always_ff @(posedge S_AXI_ACLK) begin
        if (~S_AXI_ARESETN) begin
            axi_arready <= 1'b0;
            axi_araddr <= '0;
        end else begin
            if (~axi_arready && S_AXI_ARVALID) begin
                axi_arready <= 1'b1;
                axi_araddr <= S_AXI_ARADDR;
            end else begin
                axi_arready <= 1'b0;
            end
        end
    end

    //=========================================================================
    // AXI Read Data Channel
    //=========================================================================
    always_ff @(posedge S_AXI_ACLK) begin
        if (~S_AXI_ARESETN) begin
            axi_rvalid <= 1'b0;
            axi_rresp <= 2'b00;
            read_in_progress <= 1'b0;
            axi_rdata <= '0;
        end else begin
            if (axi_arready && S_AXI_ARVALID && ~axi_rvalid && ~read_in_progress) begin
                read_in_progress <= 1'b1;
            end else if (read_in_progress) begin
                axi_rdata <= reg_data_out;
                axi_rvalid <= 1'b1;
                axi_rresp <= 2'b00;
                read_in_progress <= 1'b0;
            end else if (axi_rvalid && S_AXI_RREADY) begin
                axi_rvalid <= 1'b0;
            end
        end
    end

    assign slv_reg_rden = axi_arready & S_AXI_ARVALID & ~axi_rvalid;

    //=========================================================================
    // Read Data Multiplexer
    //=========================================================================
    always_comb begin
        if (is_entity_region_read) begin
            reg_data_out = entity_bram_douta;
        end else if (is_tilemap_region_read) begin
            reg_data_out = tilemap_bram_douta;
        end else if (is_control_region_read) begin
            case (read_byte_addr)
                14'h3000: reg_data_out = {22'd0, player_x};
                14'h3004: reg_data_out = {22'd0, player_y};
                14'h3008: reg_data_out = {25'd0, player_frame};  // 7-bit frame
                14'h3020: reg_data_out = frame_counter;
                14'h3024: reg_data_out = current_draw_x;
                14'h3028: reg_data_out = current_draw_y;
                14'h302C: reg_data_out = {24'd0, level_offset};
                // Enemy 0 registers
                14'h3030: reg_data_out = {22'd0, enemy0_x};
                14'h3034: reg_data_out = {22'd0, enemy0_y};
                14'h3038: reg_data_out = {29'd0, enemy0_frame};
                14'h303C: reg_data_out = {24'd0, enemy0_type, 1'b0, enemy0_attacking, enemy0_flip, enemy0_active};
                // Projectile registers
                14'h3040: reg_data_out = proj_0;
                14'h3044: reg_data_out = proj_1;
                14'h3048: reg_data_out = proj_2;
                14'h304C: reg_data_out = proj_3;
                // Player health
                14'h3050: reg_data_out = {29'd0, player_health};
                // Enemy 1 registers
                14'h3060: reg_data_out = {22'd0, enemy1_x};
                14'h3064: reg_data_out = {22'd0, enemy1_y};
                14'h3068: reg_data_out = {29'd0, enemy1_frame};
                14'h306C: reg_data_out = {24'd0, enemy1_type, 1'b0, enemy1_attacking, enemy1_flip, enemy1_active};
                // Enemy 2 registers
                14'h3070: reg_data_out = {22'd0, enemy2_x};
                14'h3074: reg_data_out = {22'd0, enemy2_y};
                14'h3078: reg_data_out = {29'd0, enemy2_frame};
                14'h307C: reg_data_out = {24'd0, enemy2_type, 1'b0, enemy2_attacking, enemy2_flip, enemy2_active};
                // Enemy 3 registers
                14'h3080: reg_data_out = {22'd0, enemy3_x};
                14'h3084: reg_data_out = {22'd0, enemy3_y};
                14'h3088: reg_data_out = {29'd0, enemy3_frame};
                14'h308C: reg_data_out = {24'd0, enemy3_type, 1'b0, enemy3_attacking, enemy3_flip, enemy3_active};
                // Enemy 4 registers
                14'h3090: reg_data_out = {22'd0, enemy4_x};
                14'h3094: reg_data_out = {22'd0, enemy4_y};
                14'h3098: reg_data_out = {29'd0, enemy4_frame};
                14'h309C: reg_data_out = {24'd0, enemy4_type, 1'b0, enemy4_attacking, enemy4_flip, enemy4_active};
                // Game state registers
                14'h30A0: reg_data_out = {30'd0, game_state};
                14'h30A4: reg_data_out = {31'd0, menu_selection};
                14'h30A8: reg_data_out = {29'd0, map_select};       // 3-bit
                14'h30AC: reg_data_out = {31'd0, breach_open};      // 1-bit
                14'h30B0: reg_data_out = {30'd0, breach_direction}; // 2-bit
                // Boss registers
                14'h30B4: reg_data_out = {22'd0, boss_x};
                14'h30B8: reg_data_out = {22'd0, boss_y};
                14'h30BC: reg_data_out = {27'd0, boss_frame};       // 5-bit
                14'h30C0: reg_data_out = {29'd0, boss_hit, boss_flip, boss_active};
                14'h30C4: reg_data_out = {24'd0, boss_health};      // 8-bit
                // Extended projectile registers
                14'h30D0: reg_data_out = proj_4;
                14'h30D4: reg_data_out = proj_5;
                14'h30D8: reg_data_out = proj_6;
                14'h30DC: reg_data_out = proj_7;
                14'h30E0: reg_data_out = proj_8;
                14'h30E4: reg_data_out = proj_9;
                14'h30E8: reg_data_out = proj_10;
                14'h30EC: reg_data_out = proj_11;
                14'h30F0: reg_data_out = proj_12;
                14'h30F4: reg_data_out = proj_13;
                14'h30F8: reg_data_out = proj_14;
                14'h30FC: reg_data_out = proj_15;
                14'h3110: reg_data_out = {30'd0, player_proj_frame};  // 2-bit
                default:  reg_data_out = 32'd0;
            endcase
        end else begin
            reg_data_out = 32'd0;
        end
    end

    //=========================================================================
    // Frame Counter
    //=========================================================================
    always_ff @(posedge S_AXI_ACLK) begin
        if (~S_AXI_ARESETN) begin
            vsync_prev <= 1'b0;
            vsync_rising_edge <= 1'b0;
        end else begin
            vsync_prev <= vsync;
            vsync_rising_edge <= vsync && ~vsync_prev;
        end
    end

    always_ff @(posedge S_AXI_ACLK) begin
        if (~S_AXI_ARESETN) begin
            frame_counter <= 32'd0;
        end else if (vsync_rising_edge) begin
            frame_counter <= frame_counter + 1;
        end
    end

endmodule
