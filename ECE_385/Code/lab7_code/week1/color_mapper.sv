module color_mapper (
      input  logic [9:0]  DrawX, DrawY,
      input  logic [31:0] VRAM [604],
      output logic [3:0]  Red, Green, Blue
  );

      // Character position (pixel → character)
      logic [6:0] countCharX;   // 0-79
      logic [4:0] countCharY;   // 0-29
      
      // Register position (character → register)
      logic [4:0] countRegX;    // 0-19 (since 80/4 = 20)
      logic [4:0] countRegY;    // 0-29
      
      // Position within register
      logic [1:0] ByteReg;      // 0-3 (which character in the register)
      logic [3:0] ROMrow;       // 0-15 (which row within character)
      
      // Register index
      logic [9:0] RegIdx;       // 0-599
      
      // Character data
      logic [7:0] charData;
      logic inverted;
      logic [6:0] asciiChar;
      
      // Font ROM
      logic [10:0] font_addr;
      logic [7:0] font_data;

      // Pixel data
      logic pixel_from_font;
      logic pixel_on;

      // Color register
      logic [31:0] colorReg;

      // Pixel to character
      assign countCharX = DrawX / 8;       // Character column (0-79)
      assign countCharY = DrawY / 16;      // Character row (0-29)

      // Character to register
      assign countRegX = countCharX / 4;   // Register column (0-19)
      assign countRegY = countCharY;       // Register row (0-29)

      // Within-register position
      assign ByteReg = countCharX % 4;     // Which of 4 chars (0-3)
      assign ROMrow = DrawY % 16;          // Which pixel row (0-15)

      // Calculate linear register index
      assign RegIdx = countRegY * 20 + countRegX;  // 20 registers per row

      // Extract character data from VRAM
      assign charData = VRAM[RegIdx][(ByteReg+1)*8-1:ByteReg*8];
      assign inverted = charData[7];
      assign asciiChar = charData[6:0];

      // Calculate font ROM address
      assign font_addr = asciiChar * 16 + ROMrow;

      // Instantiate font ROM
      font_rom font_inst (
          .addr(font_addr),
          .data(font_data)
      );

      // Extract pixel from font data
      assign pixel_from_font = font_data[7 - (DrawX % 8)];

      // Apply inversion
      assign pixel_on = inverted ? ~pixel_from_font : pixel_from_font;

      // Get foreground/background colors from control register
      assign colorReg = VRAM[600];

      // Assign RGB based on pixel
      always_comb begin
          if (pixel_on) begin
              // Foreground color
              Red   = colorReg[27:24];
              Green = colorReg[23:20];
              Blue  = colorReg[19:16];
          end
          else begin
              // Background color
              Red   = colorReg[11:8];
              Green = colorReg[7:4];
              Blue  = colorReg[3:0];
          end
      end

  endmodule