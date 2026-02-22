//  color_mapper.sv  (Week 2 ）


module color_mapper (
    input  logic        pix_clk,
    input  logic [9:0]  DrawX,
    input  logic [9:0]  DrawY,

    output logic [10:0] bram_addra,
    input  logic [31:0] bram_out,

    input  logic [31:0] color_palette [7:0],

    output logic [3:0]  Red,
    output logic [3:0]  Green,
    output logic [3:0]  Blue
);

    // Compute character cell position (80x30 grid, 8x16 pixels per char)
    logic [6:0] countCharX;   
    logic [4:0] countCharY;  

    assign countCharX = DrawX[9:3];// X / 8
    assign countCharY = DrawY[9:4];// Y / 16

    // Determine BRAM word address (40 words per row, 2 chars per word)
    logic [5:0] countRegX;    // 0-39
    logic [4:0] countRegY;    // 0-29
    logic       HalfSelect;   // 0 = char 0, 1 = char 1
    logic [3:0] ROMrow;       // row within font glyph (0-15)
    logic [10:0] RegIdx;

    assign countRegX  = countCharX[6:1]; //divide by 2
    assign countRegY  = countCharY;
    assign HalfSelect = countCharX[0];
    assign ROMrow     = DrawY[3:0];
    assign RegIdx     = (countRegY << 5) + (countRegY << 3) + countRegX; // RegIdx = countRegY * 40 + countRegX
    assign bram_addra = RegIdx;

 
    logic [31:0] cur_Reg;
    logic       HalfSelect_d;
    logic [3:0] ROMrow_d;
    logic [2:0] DrawX_col_d;

    //one cycle delay for sync, but might be useless
    always_ff @(posedge pix_clk) begin
        cur_Reg     <= bram_out;
        HalfSelect_d <= HalfSelect;
        ROMrow_d    <= ROMrow;
        DrawX_col_d <= DrawX[2:0];
    end

    // Extract 16-bit character word from BRAM data
    logic [15:0] charWord;

    always_comb begin
        if (HalfSelect_d == 1'b0)
            charWord = cur_Reg[15:0];
        else
            charWord = cur_Reg[31:16];
    end

    // Decode character fields
    logic        inverted;
    logic [6:0]  asciiChar;
    logic [3:0]  fg_idx, bg_idx;

    assign inverted  = charWord[15];
    assign asciiChar = charWord[14:8];
    assign fg_idx    = charWord[7:4];
    assign bg_idx    = charWord[3:0];

    // Font ROM lookup
    logic [10:0] font_addr;
    logic [7:0]  font_data;

    assign font_addr = (asciiChar << 4) + ROMrow_d; // asciiChar * 16 + ROMrow_d

    font_rom font_inst (
        .addr(font_addr),
        .data(font_data)
    );

   
    logic pixel_from_font;
    logic pixel_on;

    assign pixel_from_font = font_data[7 - DrawX_col_d];
    assign pixel_on = inverted ? ~pixel_from_font : pixel_from_font;

    // Extract colors from palette (even colors in [11:0], odd in [27:16])
    logic [11:0] fg_color;
    logic [11:0] bg_color;
    logic [2:0] fg_reg_idx;
    logic [2:0] bg_reg_idx;

    assign fg_reg_idx = fg_idx >> 1;
    assign bg_reg_idx = bg_idx >> 1;

    // Select foreground color
    always_comb begin
        if (fg_idx[0] == 1'b1)
            fg_color = color_palette[fg_reg_idx][27:16];
        else
            fg_color = color_palette[fg_reg_idx][11:0];
    end

    // Select background color
    always_comb begin
        if (bg_idx[0] == 1'b1)
            bg_color = color_palette[bg_reg_idx][27:16];
        else
            bg_color = color_palette[bg_reg_idx][11:0];
    end

    // Output RGB based on pixel state
    always_comb begin
        if (pixel_on) begin
            Red   = fg_color[11:8];
            Green = fg_color[7:4];
            Blue  = fg_color[3:0];
        end else begin
            Red   = bg_color[11:8];
            Green = bg_color[7:4];
            Blue  = bg_color[3:0];
        end
    end

endmodule
