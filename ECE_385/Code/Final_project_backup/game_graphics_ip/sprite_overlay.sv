//-------------------------------------------------------------------------
//  sprite_overlay.sv
//-------------------------------------------------------------------------
//  Purpose:
//      Composites sprite graphics over tilemap background in real-time.
//      Supports multiple entities (player, enemies, bullets) with transparency.
//      FIXED CAMERA - No scrolling support.
//
//  Key Features:
//      - Reads entity list from BRAM (position, sprite_id, flags)
//      - Checks up to 64 entities for overlap with current pixel
//      - Supports sprite transparency (color index 0 = transparent)
//      - Z-ordering: Higher entity index draws on top
//      - 16x16 pixel sprites
//
//  Entity List Format (per entity, 16 bytes = 4 words):
//      Word 0 (bytes 0-3): [Y_hi, Y_lo, X_hi, X_lo]
//      Word 1 (bytes 4-7): [reserved, reserved, flags, sprite_id]
//      Word 2-3: Reserved
//
//  Author: ECE 385 Final Project - Soul Knight Team
//-------------------------------------------------------------------------

module sprite_overlay (
    input  logic        clk,              // Pixel clock (25 MHz)
    input  logic        reset,

    // VGA timing
    input  logic [9:0]  DrawX,            // Screen X (0-639)
    input  logic [9:0]  DrawY,            // Screen Y (0-479)
    input  logic        blank,

    // Background pixel from tilemap renderer
    input  logic [3:0]  bg_pixel,
    input  logic        bg_valid,

    // Entity list BRAM interface (Port B - read only)
    output logic [9:0]  entity_addr,      // Entity BRAM address (byte address)
    input  logic [31:0] entity_data,      // 32-bit word from BRAM

    // Sprite ROM interface
    output logic [11:0] sprite_rom_addr,
    input  logic [7:0]  sprite_rom_data,  // 2 pixels packed (4-bit each)

    // Final composited output
    output logic [3:0]  pixel_out
);

    //=====================================================================
    // Constants
    //=====================================================================

    localparam MAX_ENTITIES = 64;
    localparam SPRITE_SIZE = 16;  // 16x16 pixels

    // Entity flags
    localparam FLAG_ACTIVE = 0;

    //=====================================================================
    // Pipeline Stage 1: Screen Coordinates (Fixed Camera)
    //=====================================================================

    logic [9:0]  screen_x, screen_y;
    logic [9:0]  screen_x_d1, screen_y_d1;
    logic [3:0]  bg_pixel_d1;
    logic        bg_valid_d1;
    logic        blank_d1;

    always_comb begin
        screen_x = DrawX;
        screen_y = DrawY;
    end

    always_ff @(posedge clk) begin
        screen_x_d1 <= screen_x;
        screen_y_d1 <= screen_y;
        bg_pixel_d1 <= bg_pixel;
        bg_valid_d1 <= bg_valid;
        blank_d1 <= blank;
    end

    //=====================================================================
    // Pipeline Stage 2: Entity Overlap Check (Simplified for Week 1)
    //=====================================================================
    // For Week 1: Check entity 0 only (player)
    // TODO: Expand to multiple entities in later weeks

    // Entity data parsing (from BRAM word 0)
    logic [15:0] entity_x;
    logic [15:0] entity_y;
    logic [7:0]  entity_sprite_id;

    // Current entity being checked
    logic       entity_active;
    logic       entity_overlaps;
    logic [3:0] sprite_pixel_x, sprite_pixel_y;

    // For this simplified version, we read entity 0 from BRAM
    // Entity BRAM layout: each entity is 16 bytes (4 words)
    // Word 0 (bytes 0-3): [Y_hi, Y_lo, X_hi, X_lo]

    always_comb begin
        // Parse entity 0 position (word 0)
        entity_x = entity_data[15:0];
        entity_y = entity_data[31:16];

        // Entity 0 is always active for Week 1
        entity_active = 1'b1;

        // Check if current screen pixel is within entity bounding box
        if (entity_active &&
            (screen_x_d1 >= entity_x[9:0]) && (screen_x_d1 < entity_x[9:0] + SPRITE_SIZE) &&
            (screen_y_d1 >= entity_y[9:0]) && (screen_y_d1 < entity_y[9:0] + SPRITE_SIZE)) begin

            entity_overlaps = 1'b1;
            sprite_pixel_x = screen_x_d1 - entity_x[9:0];  // 0-15
            sprite_pixel_y = screen_y_d1 - entity_y[9:0];  // 0-15
        end else begin
            entity_overlaps = 1'b0;
            sprite_pixel_x = 4'b0;
            sprite_pixel_y = 4'b0;
        end
    end

    // Drive entity BRAM address
    // For Week 1: Always read entity 0, word 0 (position data)
    // Entity 0 starts at byte address 0, word address 0
    assign entity_addr = 10'b0;  // Word address 0

    // Need to also read word 1 for sprite_id
    // For simplicity, we'll hardcode sprite_id = 0 for Week 1
    assign entity_sprite_id = 8'h00;  // Player sprite

    //=====================================================================
    // Pipeline Stage 3: Sprite ROM Lookup
    //=====================================================================

    logic [3:0] bg_pixel_d2;
    logic       bg_valid_d2;
    logic       blank_d2;
    logic       entity_overlaps_d2;
    logic       pixel_select;

    // Sprite ROM address calculation
    // Similar to tile ROM: sprite_id * 128 + pixel_y * 8 + pixel_x/2
    always_comb begin
        if (entity_overlaps) begin
            sprite_rom_addr = (entity_sprite_id << 7) +
                             (sprite_pixel_y << 3) +
                             (sprite_pixel_x >> 1);
            pixel_select = sprite_pixel_x[0];
        end else begin
            sprite_rom_addr = 12'b0;
            pixel_select = 1'b0;
        end
    end

    always_ff @(posedge clk) begin
        bg_pixel_d2 <= bg_pixel_d1;
        bg_valid_d2 <= bg_valid_d1;
        blank_d2 <= blank_d1;
        entity_overlaps_d2 <= entity_overlaps;
    end

    //=====================================================================
    // Pipeline Stage 4: Sprite Pixel Extraction & Compositing
    //=====================================================================

    logic [3:0] sprite_pixel;
    logic [3:0] bg_pixel_d3;
    logic       bg_valid_d3;
    logic       blank_d3;
    logic       entity_overlaps_d3;
    logic       pixel_select_d3;

    always_ff @(posedge clk) begin
        pixel_select_d3 <= pixel_select;
        entity_overlaps_d3 <= entity_overlaps_d2;
        bg_pixel_d3 <= bg_pixel_d2;
        bg_valid_d3 <= bg_valid_d2;
        blank_d3 <= blank_d2;
    end

    // Extract sprite pixel from ROM data
    always_comb begin
        if (pixel_select_d3)
            sprite_pixel = sprite_rom_data[7:4];
        else
            sprite_pixel = sprite_rom_data[3:0];
    end

    //=====================================================================
    // Final Stage: Transparency & Compositing
    //=====================================================================

    always_ff @(posedge clk) begin
        if (reset) begin
            pixel_out <= 4'b0;
        end else begin
            if (blank_d3) begin
                // Blanking interval
                pixel_out <= 4'b0;
            end else if (entity_overlaps_d3 && (sprite_pixel != 4'b0)) begin
                // Sprite pixel is non-transparent, use sprite color
                pixel_out <= sprite_pixel;
            end else begin
                // No sprite, or sprite is transparent, use background
                pixel_out <= bg_pixel_d3;
            end
        end
    end

endmodule
