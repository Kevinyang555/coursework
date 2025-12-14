//ui_text_renderer.sv
//Renders 8x8 character bitmaps for UI text (menu, game over, win).
//Characters are scaled to 16x16 for display.

module ui_text_renderer (
    input  logic [9:0]  DrawX,        //Current pixel X
    input  logic [9:0]  DrawY,        //Current pixel Y
    input  logic [1:0]  game_state,   // 0=MENU, 1=PLAYING, 2=GAME_OVER, 3=WIN
    input  logic        menu_selection, // 0=first option, 1=second option

    output logic        is_text_pixel,    // 1 if current pixel is text
    output logic        is_selected_text  // 1 if text is selected (yellow)
);

    //Game state constants
    localparam GAME_STATE_MENU = 2'd0;
    localparam GAME_STATE_PLAYING = 2'd1;
    localparam GAME_STATE_GAMEOVER = 2'd2;
    localparam GAME_STATE_WIN = 2'd3;

    // 8x8 Character Bitmaps for UI Text

    //Function to get character bitmap row
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
            default:
                get_char_row = 8'b00000000;
        endcase
    endfunction

    //Helper function to check if pixel is in a character
    function logic pixel_in_char(
        input logic [9:0] px,
        input logic [9:0] py,
        input logic [9:0] char_x,
        input logic [9:0] char_y,
        input logic [7:0] char_code
    );
        logic [3:0] cx, cy;
        logic [2:0] bx, by;
        logic [7:0] row_bits;

        if (px >= char_x && px < char_x + 16 &&
            py >= char_y && py < char_y + 16) begin
            cx = px - char_x;
            cy = py - char_y;
            bx = cx[3:1];
            by = cy[3:1];
            row_bits = get_char_row(char_code, by);
            pixel_in_char = row_bits[7 - bx];
        end else begin
            pixel_in_char = 1'b0;
        end
    endfunction

    //Check all text strings
    always_comb begin
        is_text_pixel = 1'b0;
        is_selected_text = 1'b0;

        if (game_state == GAME_STATE_MENU) begin
            //Title: "SOUL KNIGHT"
            if (pixel_in_char(DrawX, DrawY, 221, 180, "S")) is_text_pixel = 1'b1;
            if (pixel_in_char(DrawX, DrawY, 239, 180, "O")) is_text_pixel = 1'b1;
            if (pixel_in_char(DrawX, DrawY, 257, 180, "U")) is_text_pixel = 1'b1;
            if (pixel_in_char(DrawX, DrawY, 275, 180, "L")) is_text_pixel = 1'b1;
            if (pixel_in_char(DrawX, DrawY, 311, 180, "K")) is_text_pixel = 1'b1;
            if (pixel_in_char(DrawX, DrawY, 329, 180, "N")) is_text_pixel = 1'b1;
            if (pixel_in_char(DrawX, DrawY, 347, 180, "I")) is_text_pixel = 1'b1;
            if (pixel_in_char(DrawX, DrawY, 365, 180, "G")) is_text_pixel = 1'b1;
            if (pixel_in_char(DrawX, DrawY, 383, 180, "H")) is_text_pixel = 1'b1;
            if (pixel_in_char(DrawX, DrawY, 401, 180, "T")) is_text_pixel = 1'b1;

            // "PRESS SPACE"
            if (pixel_in_char(DrawX, DrawY, 221, 280, "P")) begin is_text_pixel = 1'b1; is_selected_text = 1'b1; end
            if (pixel_in_char(DrawX, DrawY, 239, 280, "R")) begin is_text_pixel = 1'b1; is_selected_text = 1'b1; end
            if (pixel_in_char(DrawX, DrawY, 257, 280, "E")) begin is_text_pixel = 1'b1; is_selected_text = 1'b1; end
            if (pixel_in_char(DrawX, DrawY, 275, 280, "S")) begin is_text_pixel = 1'b1; is_selected_text = 1'b1; end
            if (pixel_in_char(DrawX, DrawY, 293, 280, "S")) begin is_text_pixel = 1'b1; is_selected_text = 1'b1; end
            if (pixel_in_char(DrawX, DrawY, 329, 280, "S")) begin is_text_pixel = 1'b1; is_selected_text = 1'b1; end
            if (pixel_in_char(DrawX, DrawY, 347, 280, "P")) begin is_text_pixel = 1'b1; is_selected_text = 1'b1; end
            if (pixel_in_char(DrawX, DrawY, 365, 280, "A")) begin is_text_pixel = 1'b1; is_selected_text = 1'b1; end
            if (pixel_in_char(DrawX, DrawY, 383, 280, "C")) begin is_text_pixel = 1'b1; is_selected_text = 1'b1; end
            if (pixel_in_char(DrawX, DrawY, 401, 280, "E")) begin is_text_pixel = 1'b1; is_selected_text = 1'b1; end

        end else if (game_state == GAME_STATE_GAMEOVER) begin
            // "GAME OVER"
            if (pixel_in_char(DrawX, DrawY, 248, 180, "G")) is_text_pixel = 1'b1;
            if (pixel_in_char(DrawX, DrawY, 264, 180, "A")) is_text_pixel = 1'b1;
            if (pixel_in_char(DrawX, DrawY, 280, 180, "M")) is_text_pixel = 1'b1;
            if (pixel_in_char(DrawX, DrawY, 296, 180, "E")) is_text_pixel = 1'b1;
            if (pixel_in_char(DrawX, DrawY, 328, 180, "O")) is_text_pixel = 1'b1;
            if (pixel_in_char(DrawX, DrawY, 344, 180, "V")) is_text_pixel = 1'b1;
            if (pixel_in_char(DrawX, DrawY, 360, 180, "E")) is_text_pixel = 1'b1;
            if (pixel_in_char(DrawX, DrawY, 376, 180, "R")) is_text_pixel = 1'b1;

            // "> RESTART"
            if (!menu_selection && pixel_in_char(DrawX, DrawY, 248, 260, ">")) begin is_text_pixel = 1'b1; is_selected_text = 1'b1; end
            if (pixel_in_char(DrawX, DrawY, 280, 260, "R")) begin is_text_pixel = 1'b1; if (!menu_selection) is_selected_text = 1'b1; end
            if (pixel_in_char(DrawX, DrawY, 296, 260, "E")) begin is_text_pixel = 1'b1; if (!menu_selection) is_selected_text = 1'b1; end
            if (pixel_in_char(DrawX, DrawY, 312, 260, "S")) begin is_text_pixel = 1'b1; if (!menu_selection) is_selected_text = 1'b1; end
            if (pixel_in_char(DrawX, DrawY, 328, 260, "T")) begin is_text_pixel = 1'b1; if (!menu_selection) is_selected_text = 1'b1; end
            if (pixel_in_char(DrawX, DrawY, 344, 260, "A")) begin is_text_pixel = 1'b1; if (!menu_selection) is_selected_text = 1'b1; end
            if (pixel_in_char(DrawX, DrawY, 360, 260, "R")) begin is_text_pixel = 1'b1; if (!menu_selection) is_selected_text = 1'b1; end
            if (pixel_in_char(DrawX, DrawY, 376, 260, "T")) begin is_text_pixel = 1'b1; if (!menu_selection) is_selected_text = 1'b1; end

            // "> EXIT"
            if (menu_selection && pixel_in_char(DrawX, DrawY, 272, 300, ">")) begin is_text_pixel = 1'b1; is_selected_text = 1'b1; end
            if (pixel_in_char(DrawX, DrawY, 304, 300, "E")) begin is_text_pixel = 1'b1; if (menu_selection) is_selected_text = 1'b1; end
            if (pixel_in_char(DrawX, DrawY, 320, 300, "X")) begin is_text_pixel = 1'b1; if (menu_selection) is_selected_text = 1'b1; end
            if (pixel_in_char(DrawX, DrawY, 336, 300, "I")) begin is_text_pixel = 1'b1; if (menu_selection) is_selected_text = 1'b1; end
            if (pixel_in_char(DrawX, DrawY, 352, 300, "T")) begin is_text_pixel = 1'b1; if (menu_selection) is_selected_text = 1'b1; end

        end else if (game_state == GAME_STATE_WIN) begin
            // "YOU WIN!"
            if (pixel_in_char(DrawX, DrawY, 256, 180, "Y")) is_text_pixel = 1'b1;
            if (pixel_in_char(DrawX, DrawY, 272, 180, "O")) is_text_pixel = 1'b1;
            if (pixel_in_char(DrawX, DrawY, 288, 180, "U")) is_text_pixel = 1'b1;
            if (pixel_in_char(DrawX, DrawY, 320, 180, "W")) is_text_pixel = 1'b1;
            if (pixel_in_char(DrawX, DrawY, 336, 180, "I")) is_text_pixel = 1'b1;
            if (pixel_in_char(DrawX, DrawY, 352, 180, "N")) is_text_pixel = 1'b1;
            if (pixel_in_char(DrawX, DrawY, 368, 180, "!")) is_text_pixel = 1'b1;

            // "> RESTART"
            if (!menu_selection && pixel_in_char(DrawX, DrawY, 248, 260, ">")) begin is_text_pixel = 1'b1; is_selected_text = 1'b1; end
            if (pixel_in_char(DrawX, DrawY, 280, 260, "R")) begin is_text_pixel = 1'b1; if (!menu_selection) is_selected_text = 1'b1; end
            if (pixel_in_char(DrawX, DrawY, 296, 260, "E")) begin is_text_pixel = 1'b1; if (!menu_selection) is_selected_text = 1'b1; end
            if (pixel_in_char(DrawX, DrawY, 312, 260, "S")) begin is_text_pixel = 1'b1; if (!menu_selection) is_selected_text = 1'b1; end
            if (pixel_in_char(DrawX, DrawY, 328, 260, "T")) begin is_text_pixel = 1'b1; if (!menu_selection) is_selected_text = 1'b1; end
            if (pixel_in_char(DrawX, DrawY, 344, 260, "A")) begin is_text_pixel = 1'b1; if (!menu_selection) is_selected_text = 1'b1; end
            if (pixel_in_char(DrawX, DrawY, 360, 260, "R")) begin is_text_pixel = 1'b1; if (!menu_selection) is_selected_text = 1'b1; end
            if (pixel_in_char(DrawX, DrawY, 376, 260, "T")) begin is_text_pixel = 1'b1; if (!menu_selection) is_selected_text = 1'b1; end

            // "> EXIT"
            if (menu_selection && pixel_in_char(DrawX, DrawY, 272, 300, ">")) begin is_text_pixel = 1'b1; is_selected_text = 1'b1; end
            if (pixel_in_char(DrawX, DrawY, 304, 300, "E")) begin is_text_pixel = 1'b1; if (menu_selection) is_selected_text = 1'b1; end
            if (pixel_in_char(DrawX, DrawY, 320, 300, "X")) begin is_text_pixel = 1'b1; if (menu_selection) is_selected_text = 1'b1; end
            if (pixel_in_char(DrawX, DrawY, 336, 300, "I")) begin is_text_pixel = 1'b1; if (menu_selection) is_selected_text = 1'b1; end
            if (pixel_in_char(DrawX, DrawY, 352, 300, "T")) begin is_text_pixel = 1'b1; if (menu_selection) is_selected_text = 1'b1; end
        end
    end

endmodule
