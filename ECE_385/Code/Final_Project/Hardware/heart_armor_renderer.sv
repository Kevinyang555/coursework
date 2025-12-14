//heart_armor_renderer.sv
//Renders health hearts and armor bars in the status bar area.
//Hearts are 16x16 pixels, armor bars are 32x8 pixels.

module heart_armor_renderer (
    input  logic [9:0]  DrawX,        //Current pixel X
    input  logic [9:0]  DrawY,        //Current pixel Y
    input  logic [2:0]  player_health, // 0-6 hearts
    input  logic [1:0]  player_armor,  // 0-3 armor points

    output logic        is_heart_fill,     //Heart fill pixel (red)
    output logic        is_heart_outline,  //Heart outline pixel (white/gray)
    output logic        heart_is_full,     //This heart slot has health
    output logic        is_armor_fill,     //Armor fill pixel (gray)
    output logic        is_armor_outline,  //Armor outline pixel
    output logic        armor_is_full      //This armor slot has armor
);

    //Heart display constants
    localparam HEART_SIZE = 16;
    localparam HEART_START_X = 8;
    localparam HEART_START_Y = 8;
    localparam HEART_SPACING = 24;
    localparam NUM_HEARTS = 6;

    //Armor bar display constants
    localparam ARMOR_BAR_WIDTH = 32;
    localparam ARMOR_BAR_HEIGHT = 8;
    localparam ARMOR_START_X = 8;
    localparam ARMOR_START_Y = 28;
    localparam ARMOR_SPACING = 40;
    localparam NUM_ARMOR = 3;

    //Heart Bitmap (16x16) - Hardcoded in logic, no BRAM needed
    logic [15:0] heart_fill_bitmap [0:15];
    logic [15:0] heart_outline_bitmap [0:15];

    //Initialize heart bitmaps
    always_comb begin
        //Heart fill pattern (interior pixels)
        heart_fill_bitmap[0]  = 16'b0000000000000000;
        heart_fill_bitmap[1]  = 16'b0000000000000000;
        heart_fill_bitmap[2]  = 16'b0011100011100000;
        heart_fill_bitmap[3]  = 16'b0111110111110000;
        heart_fill_bitmap[4]  = 16'b0111111111110000;
        heart_fill_bitmap[5]  = 16'b0111111111110000;
        heart_fill_bitmap[6]  = 16'b0011111111100000;
        heart_fill_bitmap[7]  = 16'b0001111111000000;
        heart_fill_bitmap[8]  = 16'b0000111110000000;
        heart_fill_bitmap[9]  = 16'b0000011100000000;
        heart_fill_bitmap[10] = 16'b0000001000000000;
        heart_fill_bitmap[11] = 16'b0000000000000000;
        heart_fill_bitmap[12] = 16'b0000000000000000;
        heart_fill_bitmap[13] = 16'b0000000000000000;
        heart_fill_bitmap[14] = 16'b0000000000000000;
        heart_fill_bitmap[15] = 16'b0000000000000000;

        //Heart outline pattern (border pixels only)
        heart_outline_bitmap[0]  = 16'b0000000000000000;
        heart_outline_bitmap[1]  = 16'b0011100011100000;
        heart_outline_bitmap[2]  = 16'b0100010100010000;
        heart_outline_bitmap[3]  = 16'b1000001000001000;
        heart_outline_bitmap[4]  = 16'b1000000000001000;
        heart_outline_bitmap[5]  = 16'b1000000000001000;
        heart_outline_bitmap[6]  = 16'b0100000000010000;
        heart_outline_bitmap[7]  = 16'b0010000000100000;
        heart_outline_bitmap[8]  = 16'b0001000001000000;
        heart_outline_bitmap[9]  = 16'b0000100010000000;
        heart_outline_bitmap[10] = 16'b0000010100000000;
        heart_outline_bitmap[11] = 16'b0000001000000000;
        heart_outline_bitmap[12] = 16'b0000000000000000;
        heart_outline_bitmap[13] = 16'b0000000000000000;
        heart_outline_bitmap[14] = 16'b0000000000000000;
        heart_outline_bitmap[15] = 16'b0000000000000000;
    end

    //Heart Rendering Logic
    logic [2:0] heart_index;  //Which heart (0-5)
    logic [3:0] heart_pixel_x, heart_pixel_y;  //Pixel within heart (0-15)

    always_comb begin
        is_heart_fill = 1'b0;
        is_heart_outline = 1'b0;
        heart_is_full = 1'b0;
        heart_index = 3'd0;
        heart_pixel_x = 4'd0;
        heart_pixel_y = 4'd0;

        //Check if we're in the heart display region
        if (DrawY >= HEART_START_Y && DrawY < (HEART_START_Y + HEART_SIZE)) begin
            //Calculate which heart and pixel within it
            if (DrawX >= HEART_START_X && DrawX < (HEART_START_X + NUM_HEARTS * HEART_SPACING)) begin
                //Determine heart index based on X position
                logic [9:0] rel_x;
                rel_x = DrawX - HEART_START_X;

                //Check each heart slot
                for (int i = 0; i < NUM_HEARTS; i++) begin
                    if (rel_x >= (i * HEART_SPACING) && rel_x < (i * HEART_SPACING + HEART_SIZE)) begin
                        heart_index = i[2:0];
                        heart_pixel_x = rel_x - (i * HEART_SPACING);
                        heart_pixel_y = DrawY - HEART_START_Y;
                        heart_is_full = (player_health > i);

                        //Look up bitmap - note: bit 15 is leftmost pixel
                        is_heart_fill = heart_fill_bitmap[heart_pixel_y][15 - heart_pixel_x];
                        is_heart_outline = heart_outline_bitmap[heart_pixel_y][15 - heart_pixel_x];
                    end
                end
            end
        end
    end

    //Armor Bar Rendering Logic
    logic [1:0] armor_index;  //Which armor bar (0-2)
    logic [9:0] rel_x_armor;  //X position relative to armor area
    logic [9:0] rel_y_armor;  //Y position relative to armor area
    logic [9:0] armor_bar_x;  //X position within current armor bar

    always_comb begin
        is_armor_fill = 1'b0;
        is_armor_outline = 1'b0;
        armor_is_full = 1'b0;
        armor_index = 2'd0;
        rel_x_armor = 10'd0;
        rel_y_armor = 10'd0;
        armor_bar_x = 10'd0;

        //Check if we're in the armor display region
        if (DrawY >= ARMOR_START_Y && DrawY < (ARMOR_START_Y + ARMOR_BAR_HEIGHT)) begin
            //Calculate which armor bar and pixel within it
            if (DrawX >= ARMOR_START_X && DrawX < (ARMOR_START_X + NUM_ARMOR * ARMOR_SPACING)) begin
                //Calculate relative positions
                rel_x_armor = DrawX - ARMOR_START_X;
                rel_y_armor = DrawY - ARMOR_START_Y;

                //Check each armor slot
                for (int i = 0; i < NUM_ARMOR; i++) begin
                    if (rel_x_armor >= (i * ARMOR_SPACING) && rel_x_armor < (i * ARMOR_SPACING + ARMOR_BAR_WIDTH)) begin
                        armor_index = i[1:0];
                        armor_is_full = (player_armor > i);

                        //Calculate position within this armor bar
                        armor_bar_x = rel_x_armor - (i * ARMOR_SPACING);

                        //Outline: top row, bottom row, left column, right column
                        if (rel_y_armor == 0 || rel_y_armor == (ARMOR_BAR_HEIGHT - 1) ||
                            armor_bar_x == 0 || armor_bar_x == (ARMOR_BAR_WIDTH - 1)) begin
                            is_armor_outline = 1'b1;
                        end else begin
                            //Fill: everything inside the outline
                            is_armor_fill = 1'b1;
                        end
                    end
                end
            end
        end
    end

endmodule
